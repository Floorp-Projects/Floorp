/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
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

#ifndef HAVE_NEW_ASTMATCHER_NAMES
// In clang 3.8, a number of AST matchers were renamed to better match the
// respective AST node.  We use the new names, and #define them to the old
// ones for compatibility with older versions.
#define cxxConstructExpr constructExpr
#define cxxConstructorDecl constructorDecl
#define cxxMethodDecl methodDecl
#define cxxNewExpr newExpr
#define cxxRecordDecl recordDecl
#endif

// Check if the given expression contains an assignment expression.
// This can either take the form of a Binary Operator or a
// Overloaded Operator Call.
bool HasSideEffectAssignment(const Expr *expr) {
  if (auto opCallExpr = dyn_cast_or_null<CXXOperatorCallExpr>(expr)) {
    auto binOp = opCallExpr->getOperator();
    if (binOp == OO_Equal || (binOp >= OO_PlusEqual && binOp <= OO_PipeEqual)) {
      return true;
    }
  } else if (auto binOpExpr = dyn_cast_or_null<BinaryOperator>(expr)) {
    if (binOpExpr->isAssignmentOp()) {
      return true;
    }
  }

  // Recurse to children.
  for (const Stmt *SubStmt : expr->children()) {
    auto childExpr = dyn_cast_or_null<Expr>(SubStmt);
    if (childExpr && HasSideEffectAssignment(childExpr)) {
      return true;
    }
  }

  return false;
}

namespace {

using namespace clang::ast_matchers;
class DiagnosticsMatcher {
public:
  DiagnosticsMatcher();

  ASTConsumerPtr makeASTConsumer() { return astMatcher.newASTConsumer(); }

private:
  class ScopeChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
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

  class NoDuplicateRefCntMemberChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NeedsNoVTableTypeChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NonMemMovableTemplateArgChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NonMemMovableMemberChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class ExplicitImplicitChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NoAutoTypeChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NoExplicitMoveConstructorChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class RefCountedCopyConstructorChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class AssertAssignmentChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  ScopeChecker scopeChecker;
  ArithmeticArgChecker arithmeticArgChecker;
  TrivialCtorDtorChecker trivialCtorDtorChecker;
  NaNExprChecker nanExprChecker;
  NoAddRefReleaseOnReturnChecker noAddRefReleaseOnReturnChecker;
  RefCountedInsideLambdaChecker refCountedInsideLambdaChecker;
  ExplicitOperatorBoolChecker explicitOperatorBoolChecker;
  NoDuplicateRefCntMemberChecker noDuplicateRefCntMemberChecker;
  NeedsNoVTableTypeChecker needsNoVTableTypeChecker;
  NonMemMovableTemplateArgChecker nonMemMovableTemplateArgChecker;
  NonMemMovableMemberChecker nonMemMovableMemberChecker;
  ExplicitImplicitChecker explicitImplicitChecker;
  NoAutoTypeChecker noAutoTypeChecker;
  NoExplicitMoveConstructorChecker noExplicitMoveConstructorChecker;
  RefCountedCopyConstructorChecker refCountedCopyConstructorChecker;
  AssertAssignmentChecker assertAttributionChecker;
  MatchFinder astMatcher;
};

namespace {

std::string getDeclarationNamespace(const Decl *decl) {
  const DeclContext *DC =
      decl->getDeclContext()->getEnclosingNamespaceContext();
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

  const auto &name = ND->getName();
  return name;
}

bool isInIgnoredNamespaceForImplicitCtor(const Decl *decl) {
  std::string name = getDeclarationNamespace(decl);
  if (name == "") {
    return false;
  }

  return name == "std" ||               // standard C++ lib
         name == "__gnu_cxx" ||         // gnu C++ lib
         name == "boost" ||             // boost
         name == "webrtc" ||            // upstream webrtc
         name == "rtc" ||               // upstream webrtc 'base' package
         name.substr(0, 4) == "icu_" || // icu
         name == "google" ||            // protobuf
         name == "google_breakpad" ||   // breakpad
         name == "soundtouch" ||        // libsoundtouch
         name == "stagefright" ||       // libstagefright
         name == "MacFileUtilities" ||  // MacFileUtilities
         name == "dwarf2reader" ||      // dwarf2reader
         name == "arm_ex_to_module" ||  // arm_ex_to_module
         name == "testing";             // gtest
}

bool isInIgnoredNamespaceForImplicitConversion(const Decl *decl) {
  std::string name = getDeclarationNamespace(decl);
  if (name == "") {
    return false;
  }

  return name == "std" ||             // standard C++ lib
         name == "__gnu_cxx" ||       // gnu C++ lib
         name == "google_breakpad" || // breakpad
         name == "testing";           // gtest
}

bool isIgnoredPathForImplicitCtor(const Decl *decl) {
  SourceLocation Loc = decl->getLocation();
  const SourceManager &SM = decl->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator begin = llvm::sys::path::rbegin(FileName),
                                    end = llvm::sys::path::rend(FileName);
  for (; begin != end; ++begin) {
    if (begin->compare_lower(StringRef("skia")) == 0 ||
        begin->compare_lower(StringRef("angle")) == 0 ||
        begin->compare_lower(StringRef("harfbuzz")) == 0 ||
        begin->compare_lower(StringRef("hunspell")) == 0 ||
        begin->compare_lower(StringRef("scoped_ptr.h")) == 0 ||
        begin->compare_lower(StringRef("graphite2")) == 0) {
      return true;
    }
    if (begin->compare_lower(StringRef("chromium")) == 0) {
      // Ignore security/sandbox/chromium but not ipc/chromium.
      ++begin;
      return begin != end && begin->compare_lower(StringRef("sandbox")) == 0;
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
                                    end = llvm::sys::path::rend(FileName);
  for (; begin != end; ++begin) {
    if (begin->compare_lower(StringRef("graphite2")) == 0) {
      return true;
    }
  }
  return false;
}

bool isInterestingDeclForImplicitConversion(const Decl *decl) {
  return !isInIgnoredNamespaceForImplicitConversion(decl) &&
         !isIgnoredPathForImplicitConversion(decl);
}

bool isIgnoredExprForMustUse(const Expr *E) {
  if (const CXXOperatorCallExpr *OpCall = dyn_cast<CXXOperatorCallExpr>(E)) {
    switch (OpCall->getOperator()) {
    case OO_Equal:
    case OO_PlusEqual:
    case OO_MinusEqual:
    case OO_StarEqual:
    case OO_SlashEqual:
    case OO_PercentEqual:
    case OO_CaretEqual:
    case OO_AmpEqual:
    case OO_PipeEqual:
    case OO_LessLessEqual:
    case OO_GreaterGreaterEqual:
      return true;
    default:
      return false;
    }
  }

  if (const BinaryOperator *Op = dyn_cast<BinaryOperator>(E)) {
    return Op->isAssignmentOp();
  }

  return false;
}
}

class CustomTypeAnnotation {
  enum ReasonKind {
    RK_None,
    RK_Direct,
    RK_ArrayElement,
    RK_BaseClass,
    RK_Field,
    RK_TemplateInherited,
  };
  struct AnnotationReason {
    QualType Type;
    ReasonKind Kind;
    const FieldDecl *Field;

    bool valid() const { return Kind != RK_None; }
  };
  typedef DenseMap<void *, AnnotationReason> ReasonCache;

  const char *Spelling;
  const char *Pretty;
  ReasonCache Cache;

public:
  CustomTypeAnnotation(const char *Spelling, const char *Pretty)
      : Spelling(Spelling), Pretty(Pretty){};

  virtual ~CustomTypeAnnotation() {}

  // Checks if this custom annotation "effectively affects" the given type.
  bool hasEffectiveAnnotation(QualType T) {
    return directAnnotationReason(T).valid();
  }
  void dumpAnnotationReason(DiagnosticsEngine &Diag, QualType T,
                            SourceLocation Loc);

  void reportErrorIfPresent(DiagnosticsEngine &Diag, QualType T,
                            SourceLocation Loc, unsigned ErrorID,
                            unsigned NoteID) {
    if (hasEffectiveAnnotation(T)) {
      Diag.Report(Loc, ErrorID) << T;
      Diag.Report(Loc, NoteID);
      dumpAnnotationReason(Diag, T, Loc);
    }
  }

private:
  bool hasLiteralAnnotation(QualType T) const;
  AnnotationReason directAnnotationReason(QualType T);
  AnnotationReason tmplArgAnnotationReason(ArrayRef<TemplateArgument> Args);

protected:
  // Allow subclasses to apply annotations to external code:
  virtual bool hasFakeAnnotation(const TagDecl *D) const { return false; }
};

static CustomTypeAnnotation StackClass =
    CustomTypeAnnotation("moz_stack_class", "stack");
static CustomTypeAnnotation GlobalClass =
    CustomTypeAnnotation("moz_global_class", "global");
static CustomTypeAnnotation NonHeapClass =
    CustomTypeAnnotation("moz_nonheap_class", "non-heap");
static CustomTypeAnnotation HeapClass =
    CustomTypeAnnotation("moz_heap_class", "heap");
static CustomTypeAnnotation NonTemporaryClass =
    CustomTypeAnnotation("moz_non_temporary_class", "non-temporary");
static CustomTypeAnnotation MustUse =
    CustomTypeAnnotation("moz_must_use_type", "must-use");

class MemMoveAnnotation final : public CustomTypeAnnotation {
public:
  MemMoveAnnotation()
      : CustomTypeAnnotation("moz_non_memmovable", "non-memmove()able") {}

  virtual ~MemMoveAnnotation() {}

protected:
  bool hasFakeAnnotation(const TagDecl *D) const override {
    // Annotate everything in ::std, with a few exceptions; see bug
    // 1201314 for discussion.
    if (getDeclarationNamespace(D) == "std") {
      // This doesn't check that it's really ::std::pair and not
      // ::std::something_else::pair, but should be good enough.
      StringRef Name = D->getName();
      if (Name == "pair" || Name == "atomic" || Name == "__atomic_base") {
        return false;
      }
      return true;
    }
    return false;
  }
};

static MemMoveAnnotation NonMemMovable = MemMoveAnnotation();

class MozChecker : public ASTConsumer, public RecursiveASTVisitor<MozChecker> {
  DiagnosticsEngine &Diag;
  const CompilerInstance &CI;
  DiagnosticsMatcher matcher;

public:
  MozChecker(const CompilerInstance &CI) : Diag(CI.getDiagnostics()), CI(CI) {}

  ASTConsumerPtr getOtherConsumer() { return matcher.makeASTConsumer(); }

  virtual void HandleTranslationUnit(ASTContext &ctx) {
    TraverseDecl(ctx.getTranslationUnitDecl());
  }

  static bool hasCustomAnnotation(const Decl *D, const char *Spelling) {
    iterator_range<specific_attr_iterator<AnnotateAttr>> Attrs =
        D->specific_attrs<AnnotateAttr>();

    for (AnnotateAttr *Attr : Attrs) {
      if (Attr->getAnnotation() == Spelling) {
        return true;
      }
    }

    return false;
  }

  void HandleUnusedExprResult(const Stmt *stmt) {
    const Expr *E = dyn_cast_or_null<Expr>(stmt);
    if (E) {
      E = E->IgnoreImplicit(); // Ignore ExprWithCleanup etc. implicit wrappers
      QualType T = E->getType();
      if (MustUse.hasEffectiveAnnotation(T) && !isIgnoredExprForMustUse(E)) {
        unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error, "Unused value of must-use type %0");

        Diag.Report(E->getLocStart(), errorID) << T;
        MustUse.dumpAnnotationReason(Diag, T, E->getLocStart());
      }
    }
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *d) {
    // We need definitions, not declarations
    if (!d->isThisDeclarationADefinition())
      return true;

    // Look through all of our immediate bases to find methods that need to be
    // overridden
    typedef std::vector<CXXMethodDecl *> OverridesVector;
    OverridesVector must_overrides;
    for (CXXRecordDecl::base_class_iterator base = d->bases_begin(),
                                            e = d->bases_end();
         base != e; ++base) {
      // The base is either a class (CXXRecordDecl) or it's a templated class...
      CXXRecordDecl *parent = base->getType()
                                  .getDesugaredType(d->getASTContext())
                                  ->getAsCXXRecordDecl();
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
        Diag.Report(d->getLocation(), overrideID) << d->getDeclName()
                                                  << (*it)->getDeclName();
        Diag.Report((*it)->getLocation(), overrideNote);
      }
    }

    return true;
  }

  bool VisitSwitchCase(SwitchCase *stmt) {
    HandleUnusedExprResult(stmt->getSubStmt());
    return true;
  }
  bool VisitCompoundStmt(CompoundStmt *stmt) {
    for (CompoundStmt::body_iterator it = stmt->body_begin(),
                                     e = stmt->body_end();
         it != e; ++it) {
      HandleUnusedExprResult(*it);
    }
    return true;
  }
  bool VisitIfStmt(IfStmt *Stmt) {
    HandleUnusedExprResult(Stmt->getThen());
    HandleUnusedExprResult(Stmt->getElse());
    return true;
  }
  bool VisitWhileStmt(WhileStmt *Stmt) {
    HandleUnusedExprResult(Stmt->getBody());
    return true;
  }
  bool VisitDoStmt(DoStmt *Stmt) {
    HandleUnusedExprResult(Stmt->getBody());
    return true;
  }
  bool VisitForStmt(ForStmt *Stmt) {
    HandleUnusedExprResult(Stmt->getBody());
    HandleUnusedExprResult(Stmt->getInit());
    HandleUnusedExprResult(Stmt->getInc());
    return true;
  }
  bool VisitBinComma(BinaryOperator *Op) {
    HandleUnusedExprResult(Op->getLHS());
    return true;
  }
};

/// A cached data of whether classes are refcounted or not.
typedef DenseMap<const CXXRecordDecl *, std::pair<const Decl *, bool>>
    RefCountedMap;
RefCountedMap refCountedClasses;

bool classHasAddRefRelease(const CXXRecordDecl *D) {
  const RefCountedMap::iterator &it = refCountedClasses.find(D);
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

  // Look through all base cases to figure out if the parent is a refcounted
  // class.
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
  while (const clang::ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();
  return clazz ? isClassRefCounted(clazz) : false;
}

template <class T> bool IsInSystemHeader(const ASTContext &AC, const T &D) {
  auto &SourceManager = AC.getSourceManager();
  auto ExpansionLoc = SourceManager.getExpansionLoc(D.getLocStart());
  if (ExpansionLoc.isInvalid()) {
    return false;
  }
  return SourceManager.isInSystemHeader(ExpansionLoc);
}

const FieldDecl *getClassRefCntMember(const CXXRecordDecl *D) {
  for (RecordDecl::field_iterator field = D->field_begin(), e = D->field_end();
       field != e; ++field) {
    if (field->getName() == "mRefCnt") {
      return *field;
    }
  }
  return 0;
}

const FieldDecl *getBaseRefCntMember(QualType T);

const FieldDecl *getBaseRefCntMember(const CXXRecordDecl *D) {
  const FieldDecl *refCntMember = getClassRefCntMember(D);
  if (refCntMember && isClassRefCounted(D)) {
    return refCntMember;
  }

  for (CXXRecordDecl::base_class_const_iterator base = D->bases_begin(),
                                                e = D->bases_end();
       base != e; ++base) {
    refCntMember = getBaseRefCntMember(base->getType());
    if (refCntMember) {
      return refCntMember;
    }
  }
  return 0;
}

const FieldDecl *getBaseRefCntMember(QualType T) {
  while (const clang::ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *clazz = T->getAsCXXRecordDecl();
  return clazz ? getBaseRefCntMember(clazz) : 0;
}

bool typeHasVTable(QualType T) {
  while (const clang::ArrayType *arrTy = T->getAsArrayTypeUnsafe())
    T = arrTy->getElementType();
  CXXRecordDecl *offender = T->getAsCXXRecordDecl();
  return offender && offender->hasDefinition() && offender->isDynamicClass();
}
}

namespace clang {
namespace ast_matchers {

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
  return MozChecker::hasCustomAnnotation(&Node,
                                         "moz_no_addref_release_on_return");
}

/// This matcher will match all arithmetic binary operators.
AST_MATCHER(BinaryOperator, binaryArithmeticOperator) {
  BinaryOperatorKind opcode = Node.getOpcode();
  return opcode == BO_Mul || opcode == BO_Div || opcode == BO_Rem ||
         opcode == BO_Add || opcode == BO_Sub || opcode == BO_Shl ||
         opcode == BO_Shr || opcode == BO_And || opcode == BO_Xor ||
         opcode == BO_Or || opcode == BO_MulAssign || opcode == BO_DivAssign ||
         opcode == BO_RemAssign || opcode == BO_AddAssign ||
         opcode == BO_SubAssign || opcode == BO_ShlAssign ||
         opcode == BO_ShrAssign || opcode == BO_AndAssign ||
         opcode == BO_XorAssign || opcode == BO_OrAssign;
}

/// This matcher will match all arithmetic unary operators.
AST_MATCHER(UnaryOperator, unaryArithmeticOperator) {
  UnaryOperatorKind opcode = Node.getOpcode();
  return opcode == UO_PostInc || opcode == UO_PostDec || opcode == UO_PreInc ||
         opcode == UO_PreDec || opcode == UO_Plus || opcode == UO_Minus ||
         opcode == UO_Not;
}

/// This matcher will match == and != binary operators.
AST_MATCHER(BinaryOperator, binaryEqualityOperator) {
  BinaryOperatorKind opcode = Node.getOpcode();
  return opcode == BO_EQ || opcode == BO_NE;
}

/// This matcher will match floating point types.
AST_MATCHER(QualType, isFloat) { return Node->isRealFloatingType(); }

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
AST_MATCHER(CXXRecordDecl, hasRefCntMember) {
  return isClassRefCounted(&Node) && getClassRefCntMember(&Node);
}

AST_MATCHER(QualType, hasVTable) { return typeHasVTable(Node); }

AST_MATCHER(CXXRecordDecl, hasNeedsNoVTableTypeAttr) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_needs_no_vtable_type");
}

/// This matcher will select classes which are non-memmovable
AST_MATCHER(QualType, isNonMemMovable) {
  return NonMemMovable.hasEffectiveAnnotation(Node);
}

/// This matcher will select classes which require a memmovable template arg
AST_MATCHER(CXXRecordDecl, needsMemMovableTemplateArg) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_needs_memmovable_type");
}

/// This matcher will select classes which require all members to be memmovable
AST_MATCHER(CXXRecordDecl, needsMemMovableMembers) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_needs_memmovable_members");
}

AST_MATCHER(CXXConstructorDecl, isInterestingImplicitCtor) {
  const CXXConstructorDecl *decl = Node.getCanonicalDecl();
  return
      // Skip ignored namespaces and paths
      !isInIgnoredNamespaceForImplicitCtor(decl) &&
      !isIgnoredPathForImplicitCtor(decl) &&
      // We only want Converting constructors
      decl->isConvertingConstructor(false) &&
      // We don't want copy of move constructors, as those are allowed to be
      // implicit
      !decl->isCopyOrMoveConstructor() &&
      // We don't want deleted constructors.
      !decl->isDeleted();
}

// We can't call this "isImplicit" since it clashes with an existing matcher in
// clang.
AST_MATCHER(CXXConstructorDecl, isMarkedImplicit) {
  return MozChecker::hasCustomAnnotation(&Node, "moz_implicit");
}

AST_MATCHER(CXXRecordDecl, isConcreteClass) { return !Node.isAbstract(); }

AST_MATCHER(QualType, autoNonAutoableType) {
  if (const AutoType *T = Node->getContainedAutoType()) {
    if (const CXXRecordDecl *Rec = T->getAsCXXRecordDecl()) {
      return MozChecker::hasCustomAnnotation(Rec, "moz_non_autoable");
    }
  }
  return false;
}

AST_MATCHER(CXXConstructorDecl, isExplicitMoveConstructor) {
  return Node.isExplicit() && Node.isMoveConstructor();
}

AST_MATCHER(CXXConstructorDecl, isCompilerProvidedCopyConstructor) {
  return !Node.isUserProvided() && Node.isCopyConstructor();
}

AST_MATCHER(CallExpr, isAssertAssignmentTestFunc) {
  static const std::string assertName = "MOZ_AssertAssignmentTest";
  const FunctionDecl *method = Node.getDirectCallee();

  return method
      && method->getDeclName().isIdentifier()
      && method->getName() == assertName;
}
}
}

namespace {

void CustomTypeAnnotation::dumpAnnotationReason(DiagnosticsEngine &Diag,
                                                QualType T,
                                                SourceLocation Loc) {
  unsigned InheritsID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "%1 is a %0 type because it inherits from a %0 type %2");
  unsigned MemberID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "%1 is a %0 type because member %2 is a %0 type %3");
  unsigned ArrayID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "%1 is a %0 type because it is an array of %0 type %2");
  unsigned TemplID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "%1 is a %0 type because it has a template argument %0 type %2");

  AnnotationReason Reason = directAnnotationReason(T);
  for (;;) {
    switch (Reason.Kind) {
    case RK_ArrayElement:
      Diag.Report(Loc, ArrayID) << Pretty << T << Reason.Type;
      break;
    case RK_BaseClass: {
      const CXXRecordDecl *Decl = T->getAsCXXRecordDecl();
      assert(Decl && "This type should be a C++ class");

      Diag.Report(Decl->getLocation(), InheritsID) << Pretty << T
                                                   << Reason.Type;
      break;
    }
    case RK_Field:
      Diag.Report(Reason.Field->getLocation(), MemberID)
          << Pretty << T << Reason.Field << Reason.Type;
      break;
    case RK_TemplateInherited: {
      const CXXRecordDecl *Decl = T->getAsCXXRecordDecl();
      assert(Decl && "This type should be a C++ class");

      Diag.Report(Decl->getLocation(), TemplID) << Pretty << T << Reason.Type;
      break;
    }
    default:
      // FIXME (bug 1203263): note the original annotation.
      return;
    }

    T = Reason.Type;
    Reason = directAnnotationReason(T);
  }
}

bool CustomTypeAnnotation::hasLiteralAnnotation(QualType T) const {
#if CLANG_VERSION_FULL >= 306
  if (const TagDecl *D = T->getAsTagDecl()) {
#else
  if (const CXXRecordDecl *D = T->getAsCXXRecordDecl()) {
#endif
    return hasFakeAnnotation(D) || MozChecker::hasCustomAnnotation(D, Spelling);
  }
  return false;
}

CustomTypeAnnotation::AnnotationReason
CustomTypeAnnotation::directAnnotationReason(QualType T) {
  if (hasLiteralAnnotation(T)) {
    AnnotationReason Reason = {T, RK_Direct, nullptr};
    return Reason;
  }

  // Check if we have a cached answer
  void *Key = T.getAsOpaquePtr();
  ReasonCache::iterator Cached = Cache.find(T.getAsOpaquePtr());
  if (Cached != Cache.end()) {
    return Cached->second;
  }

  // Check if we have a type which we can recurse into
  if (const clang::ArrayType *Array = T->getAsArrayTypeUnsafe()) {
    if (hasEffectiveAnnotation(Array->getElementType())) {
      AnnotationReason Reason = {Array->getElementType(), RK_ArrayElement,
                                 nullptr};
      Cache[Key] = Reason;
      return Reason;
    }
  }

  // Recurse into base classes
  if (const CXXRecordDecl *Decl = T->getAsCXXRecordDecl()) {
    if (Decl->hasDefinition()) {
      Decl = Decl->getDefinition();

      for (const CXXBaseSpecifier &Base : Decl->bases()) {
        if (hasEffectiveAnnotation(Base.getType())) {
          AnnotationReason Reason = {Base.getType(), RK_BaseClass, nullptr};
          Cache[Key] = Reason;
          return Reason;
        }
      }

      // Recurse into members
      for (const FieldDecl *Field : Decl->fields()) {
        if (hasEffectiveAnnotation(Field->getType())) {
          AnnotationReason Reason = {Field->getType(), RK_Field, Field};
          Cache[Key] = Reason;
          return Reason;
        }
      }

      // Recurse into template arguments if the annotation
      // MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS is present
      if (MozChecker::hasCustomAnnotation(
              Decl, "moz_inherit_type_annotations_from_template_args")) {
        const ClassTemplateSpecializationDecl *Spec =
            dyn_cast<ClassTemplateSpecializationDecl>(Decl);
        if (Spec) {
          const TemplateArgumentList &Args = Spec->getTemplateArgs();

          AnnotationReason Reason = tmplArgAnnotationReason(Args.asArray());
          if (Reason.Kind != RK_None) {
            Cache[Key] = Reason;
            return Reason;
          }
        }
      }
    }
  }

  AnnotationReason Reason = {QualType(), RK_None, nullptr};
  Cache[Key] = Reason;
  return Reason;
}

CustomTypeAnnotation::AnnotationReason
CustomTypeAnnotation::tmplArgAnnotationReason(ArrayRef<TemplateArgument> Args) {
  for (const TemplateArgument &Arg : Args) {
    if (Arg.getKind() == TemplateArgument::Type) {
      QualType Type = Arg.getAsType();
      if (hasEffectiveAnnotation(Type)) {
        AnnotationReason Reason = {Type, RK_TemplateInherited, nullptr};
        return Reason;
      }
    } else if (Arg.getKind() == TemplateArgument::Pack) {
      AnnotationReason Reason = tmplArgAnnotationReason(Arg.getPackAsArray());
      if (Reason.Kind != RK_None) {
        return Reason;
      }
    }
  }

  AnnotationReason Reason = {QualType(), RK_None, nullptr};
  return Reason;
}

bool isPlacementNew(const CXXNewExpr *Expr) {
  // Regular new expressions aren't placement new
  if (Expr->getNumPlacementArgs() == 0)
    return false;
  const FunctionDecl *Decl = Expr->getOperatorNew();
  if (Decl && MozChecker::hasCustomAnnotation(Decl, "moz_heap_allocator")) {
    return false;
  }
  return true;
}

DiagnosticsMatcher::DiagnosticsMatcher() {
  astMatcher.addMatcher(varDecl().bind("node"), &scopeChecker);
  astMatcher.addMatcher(cxxNewExpr().bind("node"), &scopeChecker);
  astMatcher.addMatcher(materializeTemporaryExpr().bind("node"), &scopeChecker);
  astMatcher.addMatcher(
      callExpr(callee(functionDecl(heapAllocator()))).bind("node"),
      &scopeChecker);
  astMatcher.addMatcher(parmVarDecl().bind("parm_vardecl"), &scopeChecker);

  astMatcher.addMatcher(
      callExpr(allOf(hasDeclaration(noArithmeticExprInArgs()),
                     anyOf(hasDescendant(
                               binaryOperator(
                                   allOf(binaryArithmeticOperator(),
                                         hasLHS(hasDescendant(declRefExpr())),
                                         hasRHS(hasDescendant(declRefExpr()))))
                                   .bind("node")),
                           hasDescendant(
                               unaryOperator(
                                   allOf(unaryArithmeticOperator(),
                                         hasUnaryOperand(allOf(
                                             hasType(builtinType()),
                                             anyOf(hasDescendant(declRefExpr()),
                                                   declRefExpr())))))
                                   .bind("node")))))
          .bind("call"),
      &arithmeticArgChecker);
  astMatcher.addMatcher(
      cxxConstructExpr(
          allOf(hasDeclaration(noArithmeticExprInArgs()),
                anyOf(hasDescendant(
                          binaryOperator(
                              allOf(binaryArithmeticOperator(),
                                    hasLHS(hasDescendant(declRefExpr())),
                                    hasRHS(hasDescendant(declRefExpr()))))
                              .bind("node")),
                      hasDescendant(
                          unaryOperator(
                              allOf(unaryArithmeticOperator(),
                                    hasUnaryOperand(allOf(
                                        hasType(builtinType()),
                                        anyOf(hasDescendant(declRefExpr()),
                                              declRefExpr())))))
                              .bind("node")))))
          .bind("call"),
      &arithmeticArgChecker);

  astMatcher.addMatcher(cxxRecordDecl(hasTrivialCtorDtor()).bind("node"),
                        &trivialCtorDtorChecker);

  astMatcher.addMatcher(
      binaryOperator(
          allOf(binaryEqualityOperator(),
                hasLHS(has(
                    declRefExpr(hasType(qualType((isFloat())))).bind("lhs"))),
                hasRHS(has(
                    declRefExpr(hasType(qualType((isFloat())))).bind("rhs"))),
                unless(anyOf(isInSystemHeader(), isInSkScalarDotH()))))
          .bind("node"),
      &nanExprChecker);

  // First, look for direct parents of the MemberExpr.
  astMatcher.addMatcher(
      callExpr(
          callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
          hasParent(memberExpr(isAddRefOrRelease(), hasParent(callExpr()))
                        .bind("member")))
          .bind("node"),
      &noAddRefReleaseOnReturnChecker);
  // Then, look for MemberExpr that need to be casted to the right type using
  // an intermediary CastExpr before we get to the CallExpr.
  astMatcher.addMatcher(
      callExpr(
          callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
          hasParent(castExpr(
              hasParent(memberExpr(isAddRefOrRelease(), hasParent(callExpr()))
                            .bind("member")))))
          .bind("node"),
      &noAddRefReleaseOnReturnChecker);

  // Match declrefs with type "pointer to object of ref-counted type" inside a
  // lambda, where the declaration they reference is not inside the lambda.
  // This excludes arguments and local variables, leaving only captured
  // variables.
  astMatcher.addMatcher(lambdaExpr().bind("lambda"),
                        &refCountedInsideLambdaChecker);

  // Older clang versions such as the ones used on the infra recognize these
  // conversions as 'operator _Bool', but newer clang versions recognize these
  // as 'operator bool'.
  astMatcher.addMatcher(
      cxxMethodDecl(anyOf(hasName("operator bool"), hasName("operator _Bool")))
          .bind("node"),
      &explicitOperatorBoolChecker);

  astMatcher.addMatcher(
      cxxRecordDecl(allOf(decl().bind("decl"), hasRefCntMember())),
      &noDuplicateRefCntMemberChecker);

  astMatcher.addMatcher(
      classTemplateSpecializationDecl(
          allOf(hasAnyTemplateArgument(refersToType(hasVTable())),
                hasNeedsNoVTableTypeAttr()))
          .bind("node"),
      &needsNoVTableTypeChecker);

  // Handle non-mem-movable template specializations
  astMatcher.addMatcher(
      classTemplateSpecializationDecl(
          allOf(needsMemMovableTemplateArg(),
                hasAnyTemplateArgument(refersToType(isNonMemMovable()))))
          .bind("specialization"),
      &nonMemMovableTemplateArgChecker);

  // Handle non-mem-movable members
  astMatcher.addMatcher(
      cxxRecordDecl(needsMemMovableMembers())
          .bind("decl"),
      &nonMemMovableMemberChecker);

  astMatcher.addMatcher(cxxConstructorDecl(isInterestingImplicitCtor(),
                                           ofClass(allOf(isConcreteClass(),
                                                         decl().bind("class"))),
                                           unless(isMarkedImplicit()))
                            .bind("ctor"),
                        &explicitImplicitChecker);

  astMatcher.addMatcher(varDecl(hasType(autoNonAutoableType())).bind("node"),
                        &noAutoTypeChecker);

  astMatcher.addMatcher(
      cxxConstructorDecl(isExplicitMoveConstructor()).bind("node"),
      &noExplicitMoveConstructorChecker);

  astMatcher.addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(isCompilerProvidedCopyConstructor(),
                                            ofClass(hasRefCntMember()))))
          .bind("node"),
      &refCountedCopyConstructorChecker);

  astMatcher.addMatcher(
      callExpr(isAssertAssignmentTestFunc()).bind("funcCall"),
      &assertAttributionChecker);
}

// These enum variants determine whether an allocation has occured in the code.
enum AllocationVariety {
  AV_None,
  AV_Global,
  AV_Automatic,
  AV_Temporary,
  AV_Heap,
};

// XXX Currently the Decl* in the AutomaticTemporaryMap is unused, but it
// probably will be used at some point in the future, in order to produce better
// error messages.
typedef DenseMap<const MaterializeTemporaryExpr *, const Decl *>
    AutomaticTemporaryMap;
AutomaticTemporaryMap AutomaticTemporaries;

void DiagnosticsMatcher::ScopeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();

  // There are a variety of different reasons why something could be allocated
  AllocationVariety Variety = AV_None;
  SourceLocation Loc;
  QualType T;

  if (const ParmVarDecl *D =
          Result.Nodes.getNodeAs<ParmVarDecl>("parm_vardecl")) {
    if (const Expr *Default = D->getDefaultArg()) {
      if (const MaterializeTemporaryExpr *E =
              dyn_cast<MaterializeTemporaryExpr>(Default)) {
        // We have just found a ParmVarDecl which has, as its default argument,
        // a MaterializeTemporaryExpr. We mark that MaterializeTemporaryExpr as
        // automatic, by adding it to the AutomaticTemporaryMap.
        // Reporting on this type will occur when the MaterializeTemporaryExpr
        // is matched against.
        AutomaticTemporaries[E] = D;
      }
    }
    return;
  }

  // Determine the type of allocation which we detected
  if (const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("node")) {
    if (D->hasGlobalStorage()) {
      Variety = AV_Global;
    } else {
      Variety = AV_Automatic;
    }
    T = D->getType();
    Loc = D->getLocStart();
  } else if (const CXXNewExpr *E = Result.Nodes.getNodeAs<CXXNewExpr>("node")) {
    // New allocates things on the heap.
    // We don't consider placement new to do anything, as it doesn't actually
    // allocate the storage, and thus gives us no useful information.
    if (!isPlacementNew(E)) {
      Variety = AV_Heap;
      T = E->getAllocatedType();
      Loc = E->getLocStart();
    }
  } else if (const MaterializeTemporaryExpr *E =
                 Result.Nodes.getNodeAs<MaterializeTemporaryExpr>("node")) {
    // Temporaries can actually have varying storage durations, due to temporary
    // lifetime extension. We consider the allocation variety of this temporary
    // to be the same as the allocation variety of its lifetime.

    // XXX We maybe should mark these lifetimes as being due to a temporary
    // which has had its lifetime extended, to improve the error messages.
    switch (E->getStorageDuration()) {
    case SD_FullExpression: {
      // Check if this temporary is allocated as a default argument!
      // if it is, we want to pretend that it is automatic.
      AutomaticTemporaryMap::iterator AutomaticTemporary =
          AutomaticTemporaries.find(E);
      if (AutomaticTemporary != AutomaticTemporaries.end()) {
        Variety = AV_Automatic;
      } else {
        Variety = AV_Temporary;
      }
    } break;
    case SD_Automatic:
      Variety = AV_Automatic;
      break;
    case SD_Thread:
    case SD_Static:
      Variety = AV_Global;
      break;
    case SD_Dynamic:
      assert(false && "I don't think that this ever should occur...");
      Variety = AV_Heap;
      break;
    }
    T = E->getType().getUnqualifiedType();
    Loc = E->getLocStart();
  } else if (const CallExpr *E = Result.Nodes.getNodeAs<CallExpr>("node")) {
    T = E->getType()->getPointeeType();
    if (!T.isNull()) {
      // This will always allocate on the heap, as the heapAllocator() check
      // was made in the matcher
      Variety = AV_Heap;
      Loc = E->getLocStart();
    }
  }

  // Error messages for incorrect allocations.
  unsigned StackID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 only valid on the stack");
  unsigned GlobalID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 only valid as global");
  unsigned HeapID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 only valid on the heap");
  unsigned NonHeapID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 is not valid on the heap");
  unsigned NonTemporaryID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "variable of type %0 is not valid in a temporary");

  unsigned StackNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "value incorrectly allocated in an automatic variable");
  unsigned GlobalNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "value incorrectly allocated in a global variable");
  unsigned HeapNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "value incorrectly allocated on the heap");
  unsigned TemporaryNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "value incorrectly allocated in a temporary");

  // Report errors depending on the annotations on the input types.
  switch (Variety) {
  case AV_None:
    return;

  case AV_Global:
    StackClass.reportErrorIfPresent(Diag, T, Loc, StackID, GlobalNoteID);
    HeapClass.reportErrorIfPresent(Diag, T, Loc, HeapID, GlobalNoteID);
    break;

  case AV_Automatic:
    GlobalClass.reportErrorIfPresent(Diag, T, Loc, GlobalID, StackNoteID);
    HeapClass.reportErrorIfPresent(Diag, T, Loc, HeapID, StackNoteID);
    break;

  case AV_Temporary:
    GlobalClass.reportErrorIfPresent(Diag, T, Loc, GlobalID, TemporaryNoteID);
    HeapClass.reportErrorIfPresent(Diag, T, Loc, HeapID, TemporaryNoteID);
    NonTemporaryClass.reportErrorIfPresent(Diag, T, Loc, NonTemporaryID,
                                           TemporaryNoteID);
    break;

  case AV_Heap:
    GlobalClass.reportErrorIfPresent(Diag, T, Loc, GlobalID, HeapNoteID);
    StackClass.reportErrorIfPresent(Diag, T, Loc, StackID, HeapNoteID);
    NonHeapClass.reportErrorIfPresent(Diag, T, Loc, NonHeapID, HeapNoteID);
    break;
  }
}

void DiagnosticsMatcher::ArithmeticArgChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "cannot pass an arithmetic expression of built-in types to %0");
  const Expr *expr = Result.Nodes.getNodeAs<Expr>("node");
  if (const CallExpr *call = Result.Nodes.getNodeAs<CallExpr>("call")) {
    Diag.Report(expr->getLocStart(), errorID) << call->getDirectCallee();
  } else if (const CXXConstructExpr *ctr =
                 Result.Nodes.getNodeAs<CXXConstructExpr>("call")) {
    Diag.Report(expr->getLocStart(), errorID) << ctr->getConstructor();
  }
}

void DiagnosticsMatcher::TrivialCtorDtorChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "class %0 must have trivial constructors and destructors");
  const CXXRecordDecl *node = Result.Nodes.getNodeAs<CXXRecordDecl>("node");

  // We need to accept non-constexpr trivial constructors as well. This occurs
  // when a struct contains pod members, which will not be initialized. As
  // constexpr values are initialized, the constructor is non-constexpr.
  bool badCtor = !(node->hasConstexprDefaultConstructor() ||
                   node->hasTrivialDefaultConstructor());
  bool badDtor = !node->hasTrivialDestructor();
  if (badCtor || badDtor)
    Diag.Report(node->getLocStart(), errorID) << node;
}

void DiagnosticsMatcher::NaNExprChecker::run(
    const MatchFinder::MatchResult &Result) {
  if (!Result.Context->getLangOpts().CPlusPlus) {
    // mozilla::IsNaN is not usable in C, so there is no point in issuing these
    // warnings.
    return;
  }

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "comparing a floating point value to itself for "
                            "NaN checking can lead to incorrect results");
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
  // The check below ensures that we are dealing with the correct AST subtree
  // shape, and
  // also that both of the found DeclRefExpr's point to the same declaration.
  if (lhs->getFoundDecl() == rhs->getFoundDecl() && lhsExpr && rhsExpr &&
      std::distance(lhsExpr->child_begin(), lhsExpr->child_end()) == 1 &&
      std::distance(rhsExpr->child_begin(), rhsExpr->child_end()) == 1 &&
      *lhsExpr->child_begin() == lhs && *rhsExpr->child_begin() == rhs) {
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
  const CXXMethodDecl *method =
      dyn_cast<CXXMethodDecl>(member->getMemberDecl());

  Diag.Report(node->getLocStart(), errorID) << func << method;
}

void DiagnosticsMatcher::RefCountedInsideLambdaChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Refcounted variable %0 of type %1 cannot be captured by a lambda");
  unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please consider using a smart pointer");
  const LambdaExpr *Lambda = Result.Nodes.getNodeAs<LambdaExpr>("lambda");

  for (const LambdaCapture Capture : Lambda->captures()) {
    if (Capture.capturesVariable() && Capture.getCaptureKind() != LCK_ByRef) {
      QualType Pointee = Capture.getCapturedVar()->getType()->getPointeeType();

      if (!Pointee.isNull() && isClassRefCounted(Pointee)) {
        Diag.Report(Capture.getLocation(), errorID) << Capture.getCapturedVar()
                                                    << Pointee;
        Diag.Report(Capture.getLocation(), noteID);
      }
    }
  }
}

void DiagnosticsMatcher::ExplicitOperatorBoolChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "bad implicit conversion operator for %0");
  unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider adding the explicit keyword to %0");
  const CXXConversionDecl *method =
      Result.Nodes.getNodeAs<CXXConversionDecl>("node");
  const CXXRecordDecl *clazz = method->getParent();

  if (!method->isExplicitSpecified() &&
      !MozChecker::hasCustomAnnotation(method, "moz_implicit") &&
      !IsInSystemHeader(method->getASTContext(), *method) &&
      isInterestingDeclForImplicitConversion(method)) {
    Diag.Report(method->getLocStart(), errorID) << clazz;
    Diag.Report(method->getLocStart(), noteID) << "'operator bool'";
  }
}

void DiagnosticsMatcher::NoDuplicateRefCntMemberChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned warningID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Refcounted record %0 has multiple mRefCnt members");
  unsigned note1ID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Superclass %0 also has an mRefCnt member");
  unsigned note2ID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "Consider using the _INHERITED macros for AddRef and Release here");

  const CXXRecordDecl *decl = Result.Nodes.getNodeAs<CXXRecordDecl>("decl");
  const FieldDecl *refCntMember = getClassRefCntMember(decl);
  assert(refCntMember &&
         "The matcher checked to make sure we have a refCntMember");

  // Check every superclass for whether it has a base with a refcnt member, and
  // warn for those which do
  for (CXXRecordDecl::base_class_const_iterator base = decl->bases_begin(),
                                                e = decl->bases_end();
       base != e; ++base) {
    const FieldDecl *baseRefCntMember = getBaseRefCntMember(base->getType());
    if (baseRefCntMember) {
      Diag.Report(decl->getLocStart(), warningID) << decl;
      Diag.Report(baseRefCntMember->getLocStart(), note1ID)
          << baseRefCntMember->getParent();
      Diag.Report(refCntMember->getLocStart(), note2ID);
    }
  }
}

void DiagnosticsMatcher::NeedsNoVTableTypeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "%0 cannot be instantiated because %1 has a VTable");
  unsigned noteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "bad instantiation of %0 requested here");

  const ClassTemplateSpecializationDecl *specialization =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("node");

  // Get the offending template argument
  QualType offender;
  const TemplateArgumentList &args =
      specialization->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < args.size(); ++i) {
    offender = args[i].getAsType();
    if (typeHasVTable(offender)) {
      break;
    }
  }

  Diag.Report(specialization->getLocStart(), errorID) << specialization
                                                      << offender;
  Diag.Report(specialization->getPointOfInstantiation(), noteID)
      << specialization;
}

void DiagnosticsMatcher::NonMemMovableTemplateArgChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Cannot instantiate %0 with non-memmovable template argument %1");
  unsigned note1ID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "instantiation of %0 requested here");

  // Get the specialization
  const ClassTemplateSpecializationDecl *specialization =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("specialization");
  SourceLocation requestLoc = specialization->getPointOfInstantiation();

  // Report an error for every template argument which is non-memmovable
  const TemplateArgumentList &args =
      specialization->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < args.size(); ++i) {
    QualType argType = args[i].getAsType();
    if (NonMemMovable.hasEffectiveAnnotation(argType)) {
      Diag.Report(specialization->getLocation(), errorID) << specialization
                                                          << argType;
      // XXX It would be really nice if we could get the instantiation stack
      // information
      // from Sema such that we could print a full template instantiation stack,
      // however,
      // it seems as though that information is thrown out by the time we get
      // here so we
      // can only report one level of template specialization (which in many
      // cases won't
      // be useful)
      Diag.Report(requestLoc, note1ID) << specialization;
      NonMemMovable.dumpAnnotationReason(Diag, argType, requestLoc);
    }
  }
}

void DiagnosticsMatcher::NonMemMovableMemberChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned errorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "class %0 cannot have non-memmovable member %1 of type %2");

  // Get the specialization
  const CXXRecordDecl* Decl =
      Result.Nodes.getNodeAs<CXXRecordDecl>("decl");

  // Report an error for every member which is non-memmovable
  for (const FieldDecl *Field : Decl->fields()) {
    QualType Type = Field->getType();
    if (NonMemMovable.hasEffectiveAnnotation(Type)) {
      Diag.Report(Field->getLocation(), errorID) << Decl
                                                 << Field
                                                 << Type;
      NonMemMovable.dumpAnnotationReason(Diag, Type, Decl->getLocation());
    }
  }
}

void DiagnosticsMatcher::ExplicitImplicitChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "bad implicit conversion constructor for %0");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "consider adding the explicit keyword to the constructor");

  // We've already checked everything in the matcher, so we just have to report
  // the error.

  const CXXConstructorDecl *Ctor =
      Result.Nodes.getNodeAs<CXXConstructorDecl>("ctor");
  const CXXRecordDecl *Decl = Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  Diag.Report(Ctor->getLocation(), ErrorID) << Decl->getDeclName();
  Diag.Report(Ctor->getLocation(), NoteID);
}

void DiagnosticsMatcher::NoAutoTypeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Cannot use auto to declare a variable of type %0");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please write out this type explicitly");

  const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("node");

  Diag.Report(D->getLocation(), ErrorID) << D->getType();
  Diag.Report(D->getLocation(), NoteID);
}

void DiagnosticsMatcher::NoExplicitMoveConstructorChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Move constructors may not be marked explicit");

  // Everything we needed to know was checked in the matcher - we just report
  // the error here
  const CXXConstructorDecl *D =
      Result.Nodes.getNodeAs<CXXConstructorDecl>("node");

  Diag.Report(D->getLocation(), ErrorID);
}

void DiagnosticsMatcher::RefCountedCopyConstructorChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Invalid use of compiler-provided copy constructor "
                            "on refcounted type");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "The default copy constructor also copies the "
      "default mRefCnt property, leading to reference "
      "count imbalance issues. Please provide your own "
      "copy constructor which only copies the fields which "
      "need to be copied");

  // Everything we needed to know was checked in the matcher - we just report
  // the error here
  const CXXConstructExpr *E = Result.Nodes.getNodeAs<CXXConstructExpr>("node");

  Diag.Report(E->getLocation(), ErrorID);
  Diag.Report(E->getLocation(), NoteID);
}

void DiagnosticsMatcher::AssertAssignmentChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned assignInsteadOfComp = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Forbidden assignment in assert expression");
  const CallExpr *funcCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  if (funcCall && HasSideEffectAssignment(funcCall)) {
    Diag.Report(funcCall->getLocStart(), assignInsteadOfComp);
  }
}

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumerPtr CreateASTConsumer(CompilerInstance &CI,
                                   StringRef fileName) override {
#if CLANG_VERSION_FULL >= 306
    std::unique_ptr<MozChecker> checker(llvm::make_unique<MozChecker>(CI));
    ASTConsumerPtr other(checker->getOtherConsumer());

    std::vector<ASTConsumerPtr> consumers;
    consumers.push_back(std::move(checker));
    consumers.push_back(std::move(other));
    return llvm::make_unique<MultiplexConsumer>(std::move(consumers));
#else
    MozChecker *checker = new MozChecker(CI);

    ASTConsumer *consumers[] = {checker, checker->getOtherConsumer()};
    return new MultiplexConsumer(consumers);
#endif
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }
};
}

static FrontendPluginRegistry::Add<MozCheckAction> X("moz-check",
                                                     "check moz action");
// Export the registry on Windows.
#ifdef LLVM_EXPORT_REGISTRY
LLVM_EXPORT_REGISTRY(FrontendPluginRegistry)
#endif
