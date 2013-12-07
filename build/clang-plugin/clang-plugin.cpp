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

  class NonHeapClassChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
    void noteInferred(QualType T, DiagnosticsEngine &Diag);
  };

  StackClassChecker stackClassChecker;
  NonHeapClassChecker nonheapClassChecker;
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

/**
 * Where classes may be allocated. Regular classes can be allocated anywhere,
 * non-heap classes on the stack or as static variables, and stack classes only
 * on the stack. Note that stack classes subsumes non-heap classes.
 */
enum ClassAllocationNature {
  RegularClass = 0,
  NonHeapClass = 1,
  StackClass = 2
};

/// A cached data of whether classes are stack classes, non-heap classes, or
/// neither.
DenseMap<const CXXRecordDecl *,
  std::pair<const Decl *, ClassAllocationNature> > inferredAllocCauses;

ClassAllocationNature getClassAttrs(QualType T);

ClassAllocationNature getClassAttrs(CXXRecordDecl *D) {
  // Normalize so that D points to the definition if it exists. If it doesn't,
  // then we can't allocate it anyways.
  if (!D->hasDefinition())
    return RegularClass;
  D = D->getDefinition();
  // Base class: anyone with this annotation is obviously a stack class
  if (MozChecker::hasCustomAnnotation(D, "moz_stack_class"))
    return StackClass;

  // See if we cached the result.
  DenseMap<const CXXRecordDecl *,
    std::pair<const Decl *, ClassAllocationNature> >::iterator it =
    inferredAllocCauses.find(D);
  if (it != inferredAllocCauses.end()) {
    return it->second.second;
  }

  // Continue looking, we might be a stack class yet. Even if we're a nonheap
  // class, it might be possible that we've inferred to be a stack class.
  ClassAllocationNature type = RegularClass;
  if (MozChecker::hasCustomAnnotation(D, "moz_nonheap_class")) {
    type = NonHeapClass;
  }
  inferredAllocCauses.insert(std::make_pair(D,
    std::make_pair((const Decl *)0, type)));

  // Look through all base cases to figure out if the parent is a stack class or
  // a non-heap class. Since we might later infer to also be a stack class, keep
  // going.
  for (CXXRecordDecl::base_class_iterator base = D->bases_begin(),
       e = D->bases_end(); base != e; ++base) {
    ClassAllocationNature super = getClassAttrs(base->getType());
    if (super == StackClass) {
      inferredAllocCauses[D] = std::make_pair(
        base->getType()->getAsCXXRecordDecl(), StackClass);
      return StackClass;
    } else if (super == NonHeapClass) {
      inferredAllocCauses[D] = std::make_pair(
        base->getType()->getAsCXXRecordDecl(), NonHeapClass);
      type = NonHeapClass;
    }
  }

  // Maybe it has a member which is a stack class.
  for (RecordDecl::field_iterator field = D->field_begin(), e = D->field_end();
       field != e; ++field) {
    ClassAllocationNature fieldType = getClassAttrs(field->getType());
    if (fieldType == StackClass) {
      inferredAllocCauses[D] = std::make_pair(*field, StackClass);
      return StackClass;
    } else if (fieldType == NonHeapClass) {
      inferredAllocCauses[D] = std::make_pair(*field, NonHeapClass);
      type = NonHeapClass;
    }
  }

  return type;
}

ClassAllocationNature getClassAttrs(QualType T) {
  while (const ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();
  return clazz ? getClassAttrs(clazz) : RegularClass;
}

}

namespace clang {
namespace ast_matchers {

/// This matcher will match any class with the stack class assertion or an
/// array of such classes.
AST_MATCHER(QualType, stackClassAggregate) {
  return getClassAttrs(Node) == StackClass;
}

/// This matcher will match any class with the stack class assertion or an
/// array of such classes.
AST_MATCHER(QualType, nonheapClassAggregate) {
  return getClassAttrs(Node) == NonHeapClass;
}

/// This matcher will match any function declaration that is declared as a heap
/// allocator.
AST_MATCHER(FunctionDecl, heapAllocator) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_heap_allocator");
}
}
}

namespace {

bool isPlacementNew(const CXXNewExpr *expr) {
  // Regular new expressions aren't placement new
  if (expr->getNumPlacementArgs() == 0)
    return false;
  if (MozChecker::hasCustomAnnotation(expr->getOperatorNew(),
      "moz_heap_allocator"))
    return false;
  return true;
}

DiagnosticsMatcher::DiagnosticsMatcher() {
  // Stack class assertion: non-local variables of a stack class are forbidden
  // (non-localness checked in the callback)
  astMatcher.addMatcher(varDecl(hasType(stackClassAggregate())).bind("node"),
    &stackClassChecker);
  // Stack class assertion: new stack class is forbidden (unless placement new)
  astMatcher.addMatcher(newExpr(hasType(pointerType(
      pointee(stackClassAggregate())
    ))).bind("node"), &stackClassChecker);
  // Non-heap class assertion: new non-heap class is forbidden (unless placement
  // new)
  astMatcher.addMatcher(newExpr(hasType(pointerType(
      pointee(nonheapClassAggregate())
    ))).bind("node"), &nonheapClassChecker);

  // Any heap allocation function that returns a non-heap or a stack class is
  // definitely doing something wrong
  astMatcher.addMatcher(callExpr(callee(functionDecl(allOf(heapAllocator(),
      returns(pointerType(pointee(nonheapClassAggregate()))))))).bind("node"),
    &nonheapClassChecker);
  astMatcher.addMatcher(callExpr(callee(functionDecl(allOf(heapAllocator(),
      returns(pointerType(pointee(stackClassAggregate()))))))).bind("node"),
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
    noteInferred(d->getType(), Diag);
  } else if (const CXXNewExpr *expr =
      Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // If it's placement new, then this match doesn't count.
    if (isPlacementNew(expr))
      return;
    Diag.Report(expr->getStartLoc(), stackID) << expr->getAllocatedType();
    noteInferred(expr->getAllocatedType(), Diag);
  } else if (const CallExpr *expr =
      Result.Nodes.getNodeAs<CallExpr>("node")) {
    QualType badType = expr->getCallReturnType()->getPointeeType();
    Diag.Report(expr->getLocStart(), stackID) << badType;
    noteInferred(badType, Diag);
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

  const Decl *cause = inferredAllocCauses[clazz].first;
  if (const CXXRecordDecl *CRD = dyn_cast<CXXRecordDecl>(cause)) {
    Diag.Report(clazz->getLocation(), inheritsID) << T << CRD->getDeclName();
  } else if (const FieldDecl *FD = dyn_cast<FieldDecl>(cause)) {
    Diag.Report(FD->getLocation(), memberID) << T << FD << FD->getType();
  }
  
  // Recursively follow this back.
  noteInferred(cast<ValueDecl>(cause)->getType(), Diag);
}

void DiagnosticsMatcher::NonHeapClassChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned stackID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Error, "variable of type %0 is not valid on the heap");
  if (const CXXNewExpr *expr = Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // If it's placement new, then this match doesn't count.
    if (isPlacementNew(expr))
      return;
    Diag.Report(expr->getStartLoc(), stackID) << expr->getAllocatedType();
    noteInferred(expr->getAllocatedType(), Diag);
  } else if (const CallExpr *expr = Result.Nodes.getNodeAs<CallExpr>("node")) {
    QualType badType = expr->getCallReturnType()->getPointeeType();
    Diag.Report(expr->getLocStart(), stackID) << badType;
    noteInferred(badType, Diag);
  }
}

void DiagnosticsMatcher::NonHeapClassChecker::noteInferred(QualType T,
    DiagnosticsEngine &Diag) {
  unsigned inheritsID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note,
    "%0 is a non-heap class because it inherits from a non-heap class %1");
  unsigned memberID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note,
    "%0 is a non-heap class because member %1 is a non-heap class %2");

  // Find the CXXRecordDecl that is the stack class of interest
  while (const ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();

  // Direct result, we're done.
  if (MozChecker::hasCustomAnnotation(clazz, "moz_nonheap_class"))
    return;

  const Decl *cause = inferredAllocCauses[clazz].first;
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
