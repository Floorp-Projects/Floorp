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

      // While we are looping over parent classes, we'll also check to make sure
      // that the subclass has the annotation if the superclass does.
      if (hasCustomAnnotation(parent, "moz_stack_class") &&
          !hasCustomAnnotation(d, "moz_stack_class")) {
        unsigned badInheritID = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error, "%0 inherits from a stack class %1");
        Diag.Report(d->getLocation(), badInheritID)
          << d->getDeclName() << parent->getDeclName();
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
}

namespace clang {
namespace ast_matchers {

/// This matcher will match any class with the stack class assertion or an
/// array of such classes.
AST_MATCHER(QualType, stackClassAggregate) {
  QualType t = Node;
  while (const ArrayType *arrTy = t->getAsArrayTypeUnsafe())
    t = arrTy->getElementType();
  CXXRecordDecl *clazz = t->getAsCXXRecordDecl();
  return clazz && MozChecker::hasCustomAnnotation(clazz, "moz_stack_class");
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
  // Stack class assertion: a stack-class field is permitted only if it's a
  // member of a class with the annotation
  astMatcher.addMatcher(fieldDecl(hasType(stackClassAggregate())).bind("field"),
    &stackClassChecker);
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
  } else if (const CXXNewExpr *expr =
      Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // If it's placement new, then this match doesn't count.
    if (expr->getNumPlacementArgs() > 0)
      return;
    Diag.Report(expr->getStartLoc(), stackID) << expr->getAllocatedType();
  } else if (const FieldDecl *field =
      Result.Nodes.getNodeAs<FieldDecl>("field")) {
    // AST matchers don't let me get the class that contains a field...
    const RecordDecl *parent = field->getParent();
    if (!MozChecker::hasCustomAnnotation(parent, "moz_stack_class")) {
      // We use a more verbose error message here.
      unsigned stackID = Diag.getDiagnosticIDs()->getCustomDiagID(
        DiagnosticIDs::Error,
        "member of type %0 in class %1 that is not a stack class");
      Diag.Report(field->getLocation(), stackID) << field->getType() <<
        parent->getDeclName();
    }
  }
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
