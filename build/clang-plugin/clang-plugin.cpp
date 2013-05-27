/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/Version.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/DenseMap.h"

#define CLANG_VERSION_FULL (CLANG_VERSION_MAJOR * 100 + CLANG_VERSION_MINOR)

using namespace llvm;
using namespace clang;

namespace {

using namespace clang::ast_matchers;
class DiagnosticsMatcher {
public:
  DiagnosticsMatcher();

  ASTConsumer *makeASTConsumer() {
    return astMatcher.newASTConsumer();
  }

private:
  class StackClassChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
    void noteInferred(QualType T, DiagnosticsEngine &Diag);
  };

  StackClassChecker stackClassChecker;
  MatchFinder astMatcher;
};

class MozChecker : public ASTConsumer, public RecursiveASTVisitor<MozChecker> {
  DiagnosticsEngine &Diag;
  const CompilerInstance &CI;
  DiagnosticsMatcher matcher;
public:
  MozChecker(const CompilerInstance &CI) : Diag(CI.getDiagnostics()), CI(CI) {}

  ASTConsumer *getOtherConsumer() {
    return matcher.makeASTConsumer();
  }

  virtual void HandleTranslationUnit(ASTContext &ctx) {
    TraverseDecl(ctx.getTranslationUnitDecl());
  }

  static bool hasCustomAnnotation(const Decl *d, const char *spelling) {
    AnnotateAttr *attr = d->getAttr<AnnotateAttr>();
    if (!attr)
      return false;

    return attr->getAnnotation() == spelling;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *d) {
    // We need definitions, not declarations
    if (!d->isThisDeclarationADefinition()) return true;

    // Look through all of our immediate bases to find methods that need to be
    // overridden
    typedef std::vector<CXXMethodDecl *> OverridesVector;
    OverridesVector must_overrides;
    for (CXXRecordDecl::base_class_iterator base = d->bases_begin(),
         e = d->bases_end(); base != e; ++base) {
      // The base is either a class (CXXRecordDecl) or it's a templated class...
      CXXRecordDecl *parent = base->getType()
        .getDesugaredType(d->getASTContext())->getAsCXXRecordDecl();
      // The parent might not be resolved to a type yet. In this case, we can't
      // do any checking here. For complete correctness, we should visit
      // template instantiations, but this case is likely to be rare, so we will
      // ignore it until it becomes important.
      if (!parent) {
        continue;
      }
      parent = parent->getDefinition();
      for (CXXRecordDecl::method_iterator M = parent->method_begin();
          M != parent->method_end(); ++M) {
        if (hasCustomAnnotation(*M, "moz_must_override"))
          must_overrides.push_back(*M);
      }
    }

    for (OverridesVector::iterator it = must_overrides.begin();
        it != must_overrides.end(); ++it) {
      bool overridden = false;
      for (CXXRecordDecl::method_iterator M = d->method_begin();
          !overridden && M != d->method_end(); ++M) {
        // The way that Clang checks if a method M overrides its parent method
        // is if the method has the same name but would not overload.
        if (M->getName() == (*it)->getName() &&
            !CI.getSema().IsOverload(*M, (*it), false))
          overridden = true;
      }
      if (!overridden) {
        unsigned overrideID = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error, "%0 must override %1");
        unsigned overrideNote = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note, "function to override is here");
        Diag.Report(d->getLocation(), overrideID) << d->getDeclName() <<
          (*it)->getDeclName();
        Diag.Report((*it)->getLocation(), overrideNote);
      }
    }
    return true;
  }
};

DenseMap<const CXXRecordDecl *, const Decl *> stackClassCauses;

bool isStackClass(QualType T);

bool isStackClass(CXXRecordDecl *D) {
  // Normalize so that D points to the definition if it exists. If it doesn't,
  // then we can't allocate it anyways.
  if (!D->hasDefinition())
    return false;
  D = D->getDefinition();
  // Base class: anyone with this annotation is obviously a stack class
  if (MozChecker::hasCustomAnnotation(D, "moz_stack_class"))
    return true;

  // See if we cached the result.
  DenseMap<const CXXRecordDecl *, const Decl *>::iterator it =
    stackClassCauses.find(D);
  if (it != stackClassCauses.end()) {
    // If the cause is NULL, then this is not a stack class.
    return it->second != NULL;
  }

  // Look through all base cases to figure out if the parent is a stack class.
  for (CXXRecordDecl::base_class_iterator base = D->bases_begin(),
       e = D->bases_end(); base != e; ++base) {
    if (isStackClass(base->getType())) {
      stackClassCauses.insert(std::make_pair(D,
        base->getType()->getAsCXXRecordDecl()));
      return true;
    }
  }

  // Maybe it has a member which is a stack class.
  for (RecordDecl::field_iterator field = D->field_begin(), e = D->field_end();
       field != e; ++field) {
    if (isStackClass(field->getType())) {
      stackClassCauses.insert(std::make_pair(D, *field));
      return true;
    }
  }

  // Nope, this class is not a stack class.
  stackClassCauses.insert(std::make_pair(D, (const Decl*)0));
  return false;
}

bool isStackClass(QualType T) {
  while (const ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();
  return clazz && isStackClass(clazz);
}

}

namespace clang {
namespace ast_matchers {

/// This matcher will match any class with the stack class assertion or an
/// array of such classes.
AST_MATCHER(QualType, stackClassAggregate) {
  return isStackClass(Node);
}
}
}

namespace {

DiagnosticsMatcher::DiagnosticsMatcher() {
  // Stack class assertion: non-local variables of a stack class are forbidden
  // (non-localness checked in the callback)
  astMatcher.addMatcher(varDecl(hasType(stackClassAggregate())).bind("node"),
    &stackClassChecker);
  // Stack class assertion: new stack class is forbidden (unless placement new)
  astMatcher.addMatcher(newExpr(hasType(pointerType(
      pointee(stackClassAggregate())
    ))).bind("node"), &stackClassChecker);
}

void DiagnosticsMatcher::StackClassChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned stackID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Error, "variable of type %0 only valid on the stack");
  if (const VarDecl *d = Result.Nodes.getNodeAs<VarDecl>("node")) {
    // Ignore the match if it's a local variable.
    if (d->hasLocalStorage())
      return;

    Diag.Report(d->getLocation(), stackID) << d->getType();
    noteInferred(d->getType(), Diag);
  } else if (const CXXNewExpr *expr =
      Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // If it's placement new, then this match doesn't count.
    if (expr->getNumPlacementArgs() > 0)
      return;
    Diag.Report(expr->getStartLoc(), stackID) << expr->getAllocatedType();
    noteInferred(expr->getAllocatedType(), Diag);
  }
}

void DiagnosticsMatcher::StackClassChecker::noteInferred(QualType T,
    DiagnosticsEngine &Diag) {
  unsigned inheritsID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note,
    "%0 is a stack class because it inherits from a stack class %1");
  unsigned memberID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note,
    "%0 is a stack class because member %1 is a stack class %2");

  // Find the CXXRecordDecl that is the stack class of interest
  while (const ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();

  // Direct result, we're done.
  if (MozChecker::hasCustomAnnotation(clazz, "moz_stack_class"))
    return;

  const Decl *cause = stackClassCauses[clazz];
  if (const CXXRecordDecl *CRD = dyn_cast<CXXRecordDecl>(cause)) {
    Diag.Report(clazz->getLocation(), inheritsID) << T << CRD->getDeclName();
  } else if (const FieldDecl *FD = dyn_cast<FieldDecl>(cause)) {
    Diag.Report(FD->getLocation(), memberID) << T << FD << FD->getType();
  }
  
  // Recursively follow this back.
  noteInferred(cast<ValueDecl>(cause)->getType(), Diag);
}

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumer *CreateASTConsumer(CompilerInstance &CI, StringRef fileName) {
    MozChecker *checker = new MozChecker(CI);

    ASTConsumer *consumers[] = { checker, checker->getOtherConsumer() };
    return new MultiplexConsumer(consumers);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) {
    return true;
  }
};
}

static FrontendPluginRegistry::Add<MozCheckAction>
X("moz-check", "check moz action");
