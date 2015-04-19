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
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <memory>

#define CLANG_VERSION_FULL (CLANG_VERSION_MAJOR * 100 + CLANG_VERSION_MINOR)

using namespace llvm;
using namespace clang;

#if CLANG_VERSION_FULL >= 306
typedef std::unique_ptr<ASTConsumer> ASTConsumerPtr;
#else
typedef ASTConsumer *ASTConsumerPtr;
#endif

namespace {

using namespace clang::ast_matchers;
class DiagnosticsMatcher {
public:
  DiagnosticsMatcher();

  ASTConsumerPtr makeASTConsumer() {
    return astMatcher.newASTConsumer();
  }

private:
  class ScopeChecker : public MatchFinder::MatchCallback {
  public:
    enum Scope {
      eLocal,
      eGlobal
    };
    ScopeChecker(Scope scope_) :
      scope(scope_) {}
    virtual void run(const MatchFinder::MatchResult &Result);
    void noteInferred(QualType T, DiagnosticsEngine &Diag);
  private:
    Scope scope;
  };

  class NonHeapClassChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
    void noteInferred(QualType T, DiagnosticsEngine &Diag);
  };

  class ArithmeticArgChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class TrivialCtorDtorChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NaNExprChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NoAddRefReleaseOnReturnChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class RefCountedInsideLambdaChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class ExplicitOperatorBoolChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  ScopeChecker stackClassChecker;
  ScopeChecker globalClassChecker;
  NonHeapClassChecker nonheapClassChecker;
  ArithmeticArgChecker arithmeticArgChecker;
  TrivialCtorDtorChecker trivialCtorDtorChecker;
  NaNExprChecker nanExprChecker;
  NoAddRefReleaseOnReturnChecker noAddRefReleaseOnReturnChecker;
  RefCountedInsideLambdaChecker refCountedInsideLambdaChecker;
  ExplicitOperatorBoolChecker explicitOperatorBoolChecker;
  MatchFinder astMatcher;
};

namespace {

std::string getDeclarationNamespace(const Decl *decl) {
  const DeclContext *DC = decl->getDeclContext()->getEnclosingNamespaceContext();
  const NamespaceDecl *ND = dyn_cast<NamespaceDecl>(DC);
  if (!ND) {
    return "";
  }

  while (const DeclContext *ParentDC = ND->getParent()) {
    if (!isa<NamespaceDecl>(ParentDC)) {
      break;
    }
    ND = cast<NamespaceDecl>(ParentDC);
  }

  const auto& name = ND->getName();
  return name;
}

bool isInIgnoredNamespaceForImplicitCtor(const Decl *decl) {
  std::string name = getDeclarationNamespace(decl);
  if (name == "") {
    return false;
  }

  return name == "std" ||              // standard C++ lib
         name == "__gnu_cxx" ||        // gnu C++ lib
         name == "boost" ||            // boost
         name == "webrtc" ||           // upstream webrtc
         name == "icu_52" ||           // icu
         name == "google" ||           // protobuf
         name == "google_breakpad" ||  // breakpad
         name == "soundtouch" ||       // libsoundtouch
         name == "stagefright" ||      // libstagefright
         name == "MacFileUtilities" || // MacFileUtilities
         name == "dwarf2reader" ||     // dwarf2reader
         name == "arm_ex_to_module" || // arm_ex_to_module
         name == "testing";            // gtest
}

bool isInIgnoredNamespaceForImplicitConversion(const Decl *decl) {
  std::string name = getDeclarationNamespace(decl);
  if (name == "") {
    return false;
  }

  return name == "std" ||              // standard C++ lib
         name == "__gnu_cxx" ||        // gnu C++ lib
         name == "google_breakpad" ||  // breakpad
         name == "testing";            // gtest
}

bool isIgnoredPathForImplicitCtor(const Decl *decl) {
  decl = decl->getCanonicalDecl();
  SourceLocation Loc = decl->getLocation();
  const SourceManager &SM = decl->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator begin = llvm::sys::path::rbegin(FileName),
                                    end   = llvm::sys::path::rend(FileName);
  for (; begin != end; ++begin) {
    if (begin->compare_lower(StringRef("skia")) == 0 ||
        begin->compare_lower(StringRef("angle")) == 0 ||
        begin->compare_lower(StringRef("harfbuzz")) == 0 ||
        begin->compare_lower(StringRef("hunspell")) == 0 ||
        begin->compare_lower(StringRef("scoped_ptr.h")) == 0 ||
        begin->compare_lower(StringRef("graphite2")) == 0) {
      return true;
    }
  }
  return false;
}

bool isIgnoredPathForImplicitConversion(const Decl *decl) {
  decl = decl->getCanonicalDecl();
  SourceLocation Loc = decl->getLocation();
  const SourceManager &SM = decl->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator begin = llvm::sys::path::rbegin(FileName),
                                    end   = llvm::sys::path::rend(FileName);
  for (; begin != end; ++begin) {
    if (begin->compare_lower(StringRef("graphite2")) == 0) {
      return true;
    }
  }
  return false;
}

bool isInterestingDeclForImplicitCtor(const Decl *decl) {
  return !isInIgnoredNamespaceForImplicitCtor(decl) &&
         !isIgnoredPathForImplicitCtor(decl);
}

bool isInterestingDeclForImplicitConversion(const Decl *decl) {
  return !isInIgnoredNamespaceForImplicitConversion(decl) &&
         !isIgnoredPathForImplicitConversion(decl);
}

}

class MozChecker : public ASTConsumer, public RecursiveASTVisitor<MozChecker> {
  DiagnosticsEngine &Diag;
  const CompilerInstance &CI;
  DiagnosticsMatcher matcher;
public:
  MozChecker(const CompilerInstance &CI) : Diag(CI.getDiagnostics()), CI(CI) {}

  ASTConsumerPtr getOtherConsumer() {
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
            !CI.getSema().IsOverload(*M, (*it), false)) {
          overridden = true;
          break;
        }
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

    if (!d->isAbstract() && isInterestingDeclForImplicitCtor(d)) {
      for (CXXRecordDecl::ctor_iterator ctor = d->ctor_begin(),
           e = d->ctor_end(); ctor != e; ++ctor) {
        // Ignore non-converting ctors
        if (!ctor->isConvertingConstructor(false)) {
          continue;
        }
        // Ignore copy or move constructors
        if (ctor->isCopyOrMoveConstructor()) {
          continue;
        }
        // Ignore deleted constructors
        if (ctor->isDeleted()) {
          continue;
        }
        // Ignore whitelisted constructors
        if (MozChecker::hasCustomAnnotation(*ctor, "moz_implicit")) {
          continue;
        }
        unsigned ctorID = Diag.getDiagnosticIDs()->getCustomDiagID(
          DiagnosticIDs::Error, "bad implicit conversion constructor for %0");
        unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
          DiagnosticIDs::Note, "consider adding the explicit keyword to the constructor");
        Diag.Report(ctor->getLocation(), ctorID) << d->getDeclName();
        Diag.Report(ctor->getLocation(), noteID);
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
  StackClass = 2,
  GlobalClass = 3
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
  // Base class: anyone with this annotation is obviously a global class
  if (MozChecker::hasCustomAnnotation(D, "moz_global_class"))
    return GlobalClass;

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
    } else if (super == GlobalClass) {
      inferredAllocCauses[D] = std::make_pair(
        base->getType()->getAsCXXRecordDecl(), GlobalClass);
      return GlobalClass;
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
    } else if (fieldType == GlobalClass) {
      inferredAllocCauses[D] = std::make_pair(*field, GlobalClass);
      return GlobalClass;
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

/// A cached data of whether classes are refcounted or not.
typedef DenseMap<const CXXRecordDecl *,
  std::pair<const Decl *, bool> > RefCountedMap;
RefCountedMap refCountedClasses;

bool classHasAddRefRelease(const CXXRecordDecl *D) {
  const RefCountedMap::iterator& it = refCountedClasses.find(D);
  if (it != refCountedClasses.end()) {
    return it->second.second;
  }

  bool seenAddRef = false;
  bool seenRelease = false;
  for (CXXRecordDecl::method_iterator method = D->method_begin();
       method != D->method_end(); ++method) {
    const auto &name = method->getName();
    if (name == "AddRef") {
      seenAddRef = true;
    } else if (name == "Release") {
      seenRelease = true;
    }
  }
  refCountedClasses[D] = std::make_pair(D, seenAddRef && seenRelease);
  return seenAddRef && seenRelease;
}

bool isClassRefCounted(QualType T);

bool isClassRefCounted(const CXXRecordDecl *D) {
  // Normalize so that D points to the definition if it exists.
  if (!D->hasDefinition())
    return false;
  D = D->getDefinition();
  // Base class: anyone with AddRef/Release is obviously a refcounted class.
  if (classHasAddRefRelease(D))
    return true;

  // Look through all base cases to figure out if the parent is a refcounted class.
  for (CXXRecordDecl::base_class_const_iterator base = D->bases_begin();
       base != D->bases_end(); ++base) {
    bool super = isClassRefCounted(base->getType());
    if (super) {
      return true;
    }
  }

  return false;
}

bool isClassRefCounted(QualType T) {
  while (const ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();
  return clazz ? isClassRefCounted(clazz) : RegularClass;
}

template<class T>
bool IsInSystemHeader(const ASTContext &AC, const T &D) {
  auto &SourceManager = AC.getSourceManager();
  auto ExpansionLoc = SourceManager.getExpansionLoc(D.getLocStart());
  if (ExpansionLoc.isInvalid()) {
    return false;
  }
  return SourceManager.isInSystemHeader(ExpansionLoc);
}

}

namespace clang {
namespace ast_matchers {

/// This matcher will match any class with the stack class assertion or an
/// array of such classes.
AST_MATCHER(QualType, stackClassAggregate) {
  return getClassAttrs(Node) == StackClass;
}

/// This matcher will match any class with the global class assertion or an
/// array of such classes.
AST_MATCHER(QualType, globalClassAggregate) {
  return getClassAttrs(Node) == GlobalClass;
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

/// This matcher will match any declaration that is marked as not accepting
/// arithmetic expressions in its arguments.
AST_MATCHER(Decl, noArithmeticExprInArgs) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_no_arith_expr_in_arg");
}

/// This matcher will match any C++ class that is marked as having a trivial
/// constructor and destructor.
AST_MATCHER(CXXRecordDecl, hasTrivialCtorDtor) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_trivial_ctor_dtor");
}

/// This matcher will match any function declaration that is marked to prohibit
/// calling AddRef or Release on its return value.
AST_MATCHER(FunctionDecl, hasNoAddRefReleaseOnReturnAttr) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_no_addref_release_on_return");
}

/// This matcher will match all arithmetic binary operators.
AST_MATCHER(BinaryOperator, binaryArithmeticOperator) {
  BinaryOperatorKind opcode = Node.getOpcode();
  return opcode == BO_Mul ||
         opcode == BO_Div ||
         opcode == BO_Rem ||
         opcode == BO_Add ||
         opcode == BO_Sub ||
         opcode == BO_Shl ||
         opcode == BO_Shr ||
         opcode == BO_And ||
         opcode == BO_Xor ||
         opcode == BO_Or ||
         opcode == BO_MulAssign ||
         opcode == BO_DivAssign ||
         opcode == BO_RemAssign ||
         opcode == BO_AddAssign ||
         opcode == BO_SubAssign ||
         opcode == BO_ShlAssign ||
         opcode == BO_ShrAssign ||
         opcode == BO_AndAssign ||
         opcode == BO_XorAssign ||
         opcode == BO_OrAssign;
}

/// This matcher will match all arithmetic unary operators.
AST_MATCHER(UnaryOperator, unaryArithmeticOperator) {
  UnaryOperatorKind opcode = Node.getOpcode();
  return opcode == UO_PostInc ||
         opcode == UO_PostDec ||
         opcode == UO_PreInc ||
         opcode == UO_PreDec ||
         opcode == UO_Plus ||
         opcode == UO_Minus ||
         opcode == UO_Not;
}

/// This matcher will match == and != binary operators.
AST_MATCHER(BinaryOperator, binaryEqualityOperator) {
  BinaryOperatorKind opcode = Node.getOpcode();
  return opcode == BO_EQ || opcode == BO_NE;
}

/// This matcher will match floating point types.
AST_MATCHER(QualType, isFloat) {
  return Node->isRealFloatingType();
}

/// This matcher will match locations in system headers.  This is adopted from
/// isExpansionInSystemHeader in newer clangs, but modified in order to work
/// with old clangs that we use on infra.
AST_MATCHER(BinaryOperator, isInSystemHeader) {
  return IsInSystemHeader(Finder->getASTContext(), Node);
}

/// This matcher will match locations in SkScalar.h.  This header contains a
/// known NaN-testing expression which we would like to whitelist.
AST_MATCHER(BinaryOperator, isInSkScalarDotH) {
  SourceLocation Loc = Node.getOperatorLoc();
  auto &SourceManager = Finder->getASTContext().getSourceManager();
  SmallString<1024> FileName = SourceManager.getFilename(Loc);
  return llvm::sys::path::rbegin(FileName)->equals("SkScalar.h");
}

/// This matcher will match all accesses to AddRef or Release methods.
AST_MATCHER(MemberExpr, isAddRefOrRelease) {
  ValueDecl *Member = Node.getMemberDecl();
  CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(Member);
  if (Method) {
    const auto &Name = Method->getName();
    return Name == "AddRef" || Name == "Release";
  }
  return false;
}

/// This matcher will select classes which are refcounted.
AST_MATCHER(QualType, isRefCounted) {
  return isClassRefCounted(Node);
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

DiagnosticsMatcher::DiagnosticsMatcher()
  : stackClassChecker(ScopeChecker::eLocal),
    globalClassChecker(ScopeChecker::eGlobal)
{
  // Stack class assertion: non-local variables of a stack class are forbidden
  // (non-localness checked in the callback)
  astMatcher.addMatcher(varDecl(hasType(stackClassAggregate())).bind("node"),
    &stackClassChecker);
  // Stack class assertion: new stack class is forbidden (unless placement new)
  astMatcher.addMatcher(newExpr(hasType(pointerType(
      pointee(stackClassAggregate())
    ))).bind("node"), &stackClassChecker);
  // Global class assertion: non-global variables of a global class are forbidden
  // (globalness checked in the callback)
  astMatcher.addMatcher(varDecl(hasType(globalClassAggregate())).bind("node"),
    &globalClassChecker);
  // Global class assertion: new global class is forbidden
  astMatcher.addMatcher(newExpr(hasType(pointerType(
      pointee(globalClassAggregate())
    ))).bind("node"), &globalClassChecker);
  // Non-heap class assertion: new non-heap class is forbidden (unless placement
  // new)
  astMatcher.addMatcher(newExpr(hasType(pointerType(
      pointee(nonheapClassAggregate())
    ))).bind("node"), &nonheapClassChecker);

  // Any heap allocation function that returns a non-heap or a stack class or
  // a global class is definitely doing something wrong
  astMatcher.addMatcher(callExpr(callee(functionDecl(allOf(heapAllocator(),
      returns(pointerType(pointee(nonheapClassAggregate()))))))).bind("node"),
    &nonheapClassChecker);
  astMatcher.addMatcher(callExpr(callee(functionDecl(allOf(heapAllocator(),
      returns(pointerType(pointee(stackClassAggregate()))))))).bind("node"),
    &stackClassChecker);

  astMatcher.addMatcher(callExpr(callee(functionDecl(allOf(heapAllocator(),
      returns(pointerType(pointee(globalClassAggregate()))))))).bind("node"),
    &globalClassChecker);

  astMatcher.addMatcher(callExpr(allOf(hasDeclaration(noArithmeticExprInArgs()),
          anyOf(
              hasDescendant(binaryOperator(allOf(binaryArithmeticOperator(),
                  hasLHS(hasDescendant(declRefExpr())),
                  hasRHS(hasDescendant(declRefExpr()))
              )).bind("node")),
              hasDescendant(unaryOperator(allOf(unaryArithmeticOperator(),
                  hasUnaryOperand(allOf(hasType(builtinType()),
                                        anyOf(hasDescendant(declRefExpr()), declRefExpr())))
              )).bind("node"))
          )
      )).bind("call"),
    &arithmeticArgChecker);
  astMatcher.addMatcher(constructExpr(allOf(hasDeclaration(noArithmeticExprInArgs()),
          anyOf(
              hasDescendant(binaryOperator(allOf(binaryArithmeticOperator(),
                  hasLHS(hasDescendant(declRefExpr())),
                  hasRHS(hasDescendant(declRefExpr()))
              )).bind("node")),
              hasDescendant(unaryOperator(allOf(unaryArithmeticOperator(),
                  hasUnaryOperand(allOf(hasType(builtinType()),
                                        anyOf(hasDescendant(declRefExpr()), declRefExpr())))
              )).bind("node"))
          )
      )).bind("call"),
    &arithmeticArgChecker);

  astMatcher.addMatcher(recordDecl(hasTrivialCtorDtor()).bind("node"),
    &trivialCtorDtorChecker);

  astMatcher.addMatcher(binaryOperator(allOf(binaryEqualityOperator(),
          hasLHS(has(declRefExpr(hasType(qualType((isFloat())))).bind("lhs"))),
          hasRHS(has(declRefExpr(hasType(qualType((isFloat())))).bind("rhs"))),
          unless(anyOf(isInSystemHeader(), isInSkScalarDotH()))
      )).bind("node"),
    &nanExprChecker);

  // First, look for direct parents of the MemberExpr.
  astMatcher.addMatcher(callExpr(callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
                                 hasParent(memberExpr(isAddRefOrRelease(),
                                                      hasParent(callExpr())).bind("member")
      )).bind("node"),
    &noAddRefReleaseOnReturnChecker);
  // Then, look for MemberExpr that need to be casted to the right type using
  // an intermediary CastExpr before we get to the CallExpr.
  astMatcher.addMatcher(callExpr(callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
                                 hasParent(castExpr(hasParent(memberExpr(isAddRefOrRelease(),
                                                                         hasParent(callExpr())).bind("member"))))
      ).bind("node"),
    &noAddRefReleaseOnReturnChecker);

  astMatcher.addMatcher(lambdaExpr(
            hasDescendant(declRefExpr(hasType(pointerType(pointee(isRefCounted())))).bind("node"))
        ),
    &refCountedInsideLambdaChecker);

  // Older clang versions such as the ones used on the infra recognize these
  // conversions as 'operator _Bool', but newer clang versions recognize these
  // as 'operator bool'.
  astMatcher.addMatcher(methodDecl(anyOf(hasName("operator bool"),
                                         hasName("operator _Bool"))).bind("node"),
    &explicitOperatorBoolChecker);
}

void DiagnosticsMatcher::ScopeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned stackID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Error, "variable of type %0 only valid on the stack");
  unsigned globalID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Error, "variable of type %0 only valid as global");
  unsigned errorID = (scope == eGlobal) ? globalID : stackID;
  if (const VarDecl *d = Result.Nodes.getNodeAs<VarDecl>("node")) {
    if (scope == eLocal) {
      // Ignore the match if it's a local variable.
      if (d->hasLocalStorage())
        return;
    } else if (scope == eGlobal) {
      // Ignore the match if it's a global variable or a static member of a
      // class.  The latter is technically not in the global scope, but for the
      // use case of classes that intend to avoid introducing static
      // initializers that is fine.
      if (d->hasGlobalStorage() && !d->isStaticLocal())
        return;
    }

    Diag.Report(d->getLocation(), errorID) << d->getType();
    noteInferred(d->getType(), Diag);
  } else if (const CXXNewExpr *expr =
      Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // If it's placement new, then this match doesn't count.
    if (scope == eLocal && isPlacementNew(expr))
      return;
    Diag.Report(expr->getStartLoc(), errorID) << expr->getAllocatedType();
    noteInferred(expr->getAllocatedType(), Diag);
  } else if (const CallExpr *expr =
      Result.Nodes.getNodeAs<CallExpr>("node")) {
    QualType badType = expr->getCallReturnType()->getPointeeType();
    Diag.Report(expr->getLocStart(), errorID) << badType;
    noteInferred(badType, Diag);
  }
}

void DiagnosticsMatcher::ScopeChecker::noteInferred(QualType T,
    DiagnosticsEngine &Diag) {
  unsigned inheritsID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note,
    "%0 is a %2 class because it inherits from a %2 class %1");
  unsigned memberID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note,
    "%0 is a %3 class because member %1 is a %3 class %2");
  const char* attribute = (scope == eGlobal) ?
    "moz_global_class" : "moz_stack_class";
  const char* type = (scope == eGlobal) ?
    "global" : "stack";

  // Find the CXXRecordDecl that is the local/global class of interest
  while (const ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();

  // Direct result, we're done.
  if (MozChecker::hasCustomAnnotation(clazz, attribute))
    return;

  const Decl *cause = inferredAllocCauses[clazz].first;
  if (const CXXRecordDecl *CRD = dyn_cast<CXXRecordDecl>(cause)) {
    Diag.Report(clazz->getLocation(), inheritsID) <<
      T << CRD->getDeclName() << type;
  } else if (const FieldDecl *FD = dyn_cast<FieldDecl>(cause)) {
    Diag.Report(FD->getLocation(), memberID) <<
      T << FD << FD->getType() << type;
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

void DiagnosticsMatcher::ArithmeticArgChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "cannot pass an arithmetic expression of built-in types to %0");
  const Expr *expr = Result.Nodes.getNodeAs<Expr>("node");
  if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("call")) {
    Diag.Report(expr->getLocStart(), errorID) << call->getDirectCallee();
  } else if (const CXXConstructExpr *ctr = Result.Nodes.getNodeAs<CXXConstructExpr>("call")) {
    Diag.Report(expr->getLocStart(), errorID) << ctr->getConstructor();
  }
}

void DiagnosticsMatcher::TrivialCtorDtorChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "class %0 must have trivial constructors and destructors");
  const CXXRecordDecl *node = Result.Nodes.getNodeAs<CXXRecordDecl>("node");

  bool badCtor = !node->hasTrivialDefaultConstructor();
  bool badDtor = !node->hasTrivialDestructor();
  if (badCtor || badDtor)
    Diag.Report(node->getLocStart(), errorID) << node;
}

void DiagnosticsMatcher::NaNExprChecker::run(
    const MatchFinder::MatchResult &Result) {
  if (!Result.Context->getLangOpts().CPlusPlus) {
    // mozilla::IsNaN is not usable in C, so there is no point in issuing these warnings.
    return;
  }

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "comparing a floating point value to itself for NaN checking can lead to incorrect results");
  unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider using mozilla::IsNaN instead");
  const BinaryOperator *expr = Result.Nodes.getNodeAs<BinaryOperator>("node");
  const DeclRefExpr *lhs = Result.Nodes.getNodeAs<DeclRefExpr>("lhs");
  const DeclRefExpr *rhs = Result.Nodes.getNodeAs<DeclRefExpr>("rhs");
  const ImplicitCastExpr *lhsExpr = dyn_cast<ImplicitCastExpr>(expr->getLHS());
  const ImplicitCastExpr *rhsExpr = dyn_cast<ImplicitCastExpr>(expr->getRHS());
  // The AST subtree that we are looking for will look like this:
  // -BinaryOperator ==/!=
  //  |-ImplicitCastExpr LValueToRValue
  //  | |-DeclRefExpr
  //  |-ImplicitCastExpr LValueToRValue
  //    |-DeclRefExpr
  // The check below ensures that we are dealing with the correct AST subtree shape, and
  // also that both of the found DeclRefExpr's point to the same declaration.
  if (lhs->getFoundDecl() == rhs->getFoundDecl() &&
      lhsExpr && rhsExpr &&
      std::distance(lhsExpr->child_begin(), lhsExpr->child_end()) == 1 &&
      std::distance(rhsExpr->child_begin(), rhsExpr->child_end()) == 1 &&
      *lhsExpr->child_begin() == lhs &&
      *rhsExpr->child_begin() == rhs) {
    Diag.Report(expr->getLocStart(), errorID);
    Diag.Report(expr->getLocStart(), noteID);
  }
}

void DiagnosticsMatcher::NoAddRefReleaseOnReturnChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "%1 cannot be called on the return value of %0");
  const Stmt *node = Result.Nodes.getNodeAs<Stmt>("node");
  const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  const MemberExpr *member = Result.Nodes.getNodeAs<MemberExpr>("member");
  const CXXMethodDecl *method = dyn_cast<CXXMethodDecl>(member->getMemberDecl());

  Diag.Report(node->getLocStart(), errorID) << func << method;
}

void DiagnosticsMatcher::RefCountedInsideLambdaChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Refcounted variable %0 of type %1 cannot be used inside a lambda");
  unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please consider using a smart pointer");
  const DeclRefExpr *node = Result.Nodes.getNodeAs<DeclRefExpr>("node");

  Diag.Report(node->getLocStart(), errorID) << node->getFoundDecl() <<
    node->getType()->getPointeeType();
  Diag.Report(node->getLocStart(), noteID);
}

void DiagnosticsMatcher::ExplicitOperatorBoolChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "bad implicit conversion operator for %0");
  unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider adding the explicit keyword to %0");
  const CXXConversionDecl *method = Result.Nodes.getNodeAs<CXXConversionDecl>("node");
  const CXXRecordDecl *clazz = method->getParent();

  if (!method->isExplicitSpecified() &&
      !MozChecker::hasCustomAnnotation(method, "moz_implicit") &&
      !IsInSystemHeader(method->getASTContext(), *method) &&
      isInterestingDeclForImplicitConversion(method)) {
    Diag.Report(method->getLocStart(), errorID) << clazz;
    Diag.Report(method->getLocStart(), noteID) << "'operator bool'";
  }
}

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumerPtr CreateASTConsumer(CompilerInstance &CI, StringRef fileName) override {
#if CLANG_VERSION_FULL >= 306
    std::unique_ptr<MozChecker> checker(make_unique<MozChecker>(CI));

    std::vector<std::unique_ptr<ASTConsumer>> consumers;
    consumers.push_back(std::move(checker));
    consumers.push_back(checker->getOtherConsumer());
    return make_unique<MultiplexConsumer>(std::move(consumers));
#else
    MozChecker *checker = new MozChecker(CI);

    ASTConsumer *consumers[] = { checker, checker->getOtherConsumer() };
    return new MultiplexConsumer(consumers);
#endif
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
}

static FrontendPluginRegistry::Add<MozCheckAction>
X("moz-check", "check moz action");
