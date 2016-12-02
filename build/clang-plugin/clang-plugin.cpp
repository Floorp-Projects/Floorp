/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file respects the LLVM coding standard described at
 * http://llvm.org/docs/CodingStandards.html */

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
#include <iterator>

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

#ifndef HAS_ACCEPTS_IGNORINGPARENIMPCASTS
#define hasIgnoringParenImpCasts(x) has(x)
#else
// Before clang 3.9 "has" would behave like has(ignoringParenImpCasts(x)),
// however doing that explicitly would not compile.
#define hasIgnoringParenImpCasts(x) has(ignoringParenImpCasts(x))
#endif

// Check if the given expression contains an assignment expression.
// This can either take the form of a Binary Operator or a
// Overloaded Operator Call.
bool hasSideEffectAssignment(const Expr *Expression) {
  if (auto OpCallExpr = dyn_cast_or_null<CXXOperatorCallExpr>(Expression)) {
    auto BinOp = OpCallExpr->getOperator();
    if (BinOp == OO_Equal || (BinOp >= OO_PlusEqual && BinOp <= OO_PipeEqual)) {
      return true;
    }
  } else if (auto BinOpExpr = dyn_cast_or_null<BinaryOperator>(Expression)) {
    if (BinOpExpr->isAssignmentOp()) {
      return true;
    }
  }

  // Recurse to children.
  for (const Stmt *SubStmt : Expression->children()) {
    auto ChildExpr = dyn_cast_or_null<Expr>(SubStmt);
    if (ChildExpr && hasSideEffectAssignment(ChildExpr)) {
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

  ASTConsumerPtr makeASTConsumer() { return AstMatcher.newASTConsumer(); }

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

  class KungFuDeathGripChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class SprintfLiteralChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class OverrideBaseCallChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  private:
    void evaluateExpression(const Stmt *StmtExpr,
        std::list<const CXXMethodDecl*> &MethodList);
    void getRequiredBaseMethod(const CXXMethodDecl* Method,
        std::list<const CXXMethodDecl*>& MethodsList);
    void findBaseMethodCall(const CXXMethodDecl* Method,
        std::list<const CXXMethodDecl*>& MethodsList);
    bool isRequiredBaseMethod(const CXXMethodDecl *Method);
  };

/*
 *  This is a companion checker for OverrideBaseCallChecker that rejects
 *  the usage of MOZ_REQUIRED_BASE_METHOD on non-virtual base methods.
 */
  class OverrideBaseCallUsageChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  class NonParamInsideFunctionDeclChecker : public MatchFinder::MatchCallback {
  public:
    virtual void run(const MatchFinder::MatchResult &Result);
  };

  ScopeChecker Scope;
  ArithmeticArgChecker ArithmeticArg;
  TrivialCtorDtorChecker TrivialCtorDtor;
  NaNExprChecker NaNExpr;
  NoAddRefReleaseOnReturnChecker NoAddRefReleaseOnReturn;
  RefCountedInsideLambdaChecker RefCountedInsideLambda;
  ExplicitOperatorBoolChecker ExplicitOperatorBool;
  NoDuplicateRefCntMemberChecker NoDuplicateRefCntMember;
  NeedsNoVTableTypeChecker NeedsNoVTableType;
  NonMemMovableTemplateArgChecker NonMemMovableTemplateArg;
  NonMemMovableMemberChecker NonMemMovableMember;
  ExplicitImplicitChecker ExplicitImplicit;
  NoAutoTypeChecker NoAutoType;
  NoExplicitMoveConstructorChecker NoExplicitMoveConstructor;
  RefCountedCopyConstructorChecker RefCountedCopyConstructor;
  AssertAssignmentChecker AssertAttribution;
  KungFuDeathGripChecker KungFuDeathGrip;
  SprintfLiteralChecker SprintfLiteral;
  OverrideBaseCallChecker OverrideBaseCall;
  OverrideBaseCallUsageChecker OverrideBaseCallUsage;
  NonParamInsideFunctionDeclChecker NonParamInsideFunctionDecl;
  MatchFinder AstMatcher;
};

namespace {

std::string getDeclarationNamespace(const Decl *Declaration) {
  const DeclContext *DC =
      Declaration->getDeclContext()->getEnclosingNamespaceContext();
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

  const auto &Name = ND->getName();
  return Name;
}

bool isInIgnoredNamespaceForImplicitCtor(const Decl *Declaration) {
  std::string Name = getDeclarationNamespace(Declaration);
  if (Name == "") {
    return false;
  }

  return Name == "std" ||               // standard C++ lib
         Name == "__gnu_cxx" ||         // gnu C++ lib
         Name == "boost" ||             // boost
         Name == "webrtc" ||            // upstream webrtc
         Name == "rtc" ||               // upstream webrtc 'base' package
         Name.substr(0, 4) == "icu_" || // icu
         Name == "google" ||            // protobuf
         Name == "google_breakpad" ||   // breakpad
         Name == "soundtouch" ||        // libsoundtouch
         Name == "stagefright" ||       // libstagefright
         Name == "MacFileUtilities" ||  // MacFileUtilities
         Name == "dwarf2reader" ||      // dwarf2reader
         Name == "arm_ex_to_module" ||  // arm_ex_to_module
         Name == "testing" ||           // gtest
         Name == "Json";                // jsoncpp
}

bool isInIgnoredNamespaceForImplicitConversion(const Decl *Declaration) {
  std::string Name = getDeclarationNamespace(Declaration);
  if (Name == "") {
    return false;
  }

  return Name == "std" ||             // standard C++ lib
         Name == "__gnu_cxx" ||       // gnu C++ lib
         Name == "google_breakpad" || // breakpad
         Name == "testing";           // gtest
}

bool isIgnoredPathForImplicitCtor(const Decl *Declaration) {
  SourceLocation Loc = Declaration->getLocation();
  const SourceManager &SM = Declaration->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator Begin = llvm::sys::path::rbegin(FileName),
                                    End = llvm::sys::path::rend(FileName);
  for (; Begin != End; ++Begin) {
    if (Begin->compare_lower(StringRef("skia")) == 0 ||
        Begin->compare_lower(StringRef("angle")) == 0 ||
        Begin->compare_lower(StringRef("harfbuzz")) == 0 ||
        Begin->compare_lower(StringRef("hunspell")) == 0 ||
        Begin->compare_lower(StringRef("scoped_ptr.h")) == 0 ||
        Begin->compare_lower(StringRef("graphite2")) == 0 ||
        Begin->compare_lower(StringRef("icu")) == 0) {
      return true;
    }
    if (Begin->compare_lower(StringRef("chromium")) == 0) {
      // Ignore security/sandbox/chromium but not ipc/chromium.
      ++Begin;
      return Begin != End && Begin->compare_lower(StringRef("sandbox")) == 0;
    }
  }
  return false;
}

bool isIgnoredPathForImplicitConversion(const Decl *Declaration) {
  Declaration = Declaration->getCanonicalDecl();
  SourceLocation Loc = Declaration->getLocation();
  const SourceManager &SM = Declaration->getASTContext().getSourceManager();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator Begin = llvm::sys::path::rbegin(FileName),
                                    End = llvm::sys::path::rend(FileName);
  for (; Begin != End; ++Begin) {
    if (Begin->compare_lower(StringRef("graphite2")) == 0) {
      return true;
    }
    if (Begin->compare_lower(StringRef("chromium")) == 0) {
      // Ignore security/sandbox/chromium but not ipc/chromium.
      ++Begin;
      return Begin != End && Begin->compare_lower(StringRef("sandbox")) == 0;
    }
  }
  return false;
}

bool isIgnoredPathForSprintfLiteral(const CallExpr *Call, const SourceManager &SM) {
  SourceLocation Loc = Call->getLocStart();
  SmallString<1024> FileName = SM.getFilename(Loc);
  llvm::sys::fs::make_absolute(FileName);
  llvm::sys::path::reverse_iterator Begin = llvm::sys::path::rbegin(FileName),
                                    End = llvm::sys::path::rend(FileName);
  for (; Begin != End; ++Begin) {
    if (Begin->compare_lower(StringRef("angle")) == 0 ||
        Begin->compare_lower(StringRef("chromium")) == 0 ||
        Begin->compare_lower(StringRef("crashreporter")) == 0 ||
        Begin->compare_lower(StringRef("google-breakpad")) == 0 ||
        Begin->compare_lower(StringRef("harfbuzz")) == 0 ||
        Begin->compare_lower(StringRef("libstagefright")) == 0 ||
        Begin->compare_lower(StringRef("mtransport")) == 0 ||
        Begin->compare_lower(StringRef("protobuf")) == 0 ||
        Begin->compare_lower(StringRef("skia")) == 0 ||
        // Gtest uses snprintf as GTEST_SNPRINTF_ with sizeof
        Begin->compare_lower(StringRef("testing")) == 0) {
      return true;
    }
    if (Begin->compare_lower(StringRef("webrtc")) == 0) {
      // Ignore trunk/webrtc, but not media/webrtc
      ++Begin;
      return Begin != End && Begin->compare_lower(StringRef("trunk")) == 0;
    }
  }
  return false;
}

bool isInterestingDeclForImplicitConversion(const Decl *Declaration) {
  return !isInIgnoredNamespaceForImplicitConversion(Declaration) &&
         !isIgnoredPathForImplicitConversion(Declaration);
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

template<typename T>
StringRef getNameChecked(const T& D) {
  return D->getIdentifier() ? D->getName() : "";
}

bool typeIsRefPtr(QualType Q) {
  CXXRecordDecl *D = Q->getAsCXXRecordDecl();
  if (!D || !D->getIdentifier()) {
    return false;
  }

  StringRef name = D->getName();
  if (name == "RefPtr" || name == "nsCOMPtr") {
    return true;
  }
  return false;
}

// The method defined in clang for ignoring implicit nodes doesn't work with
// some AST trees. To get around this, we define our own implementation of
// IgnoreTrivials.
const Stmt *IgnoreTrivials(const Stmt *s) {
  while (true) {
    if (auto *ewc = dyn_cast<ExprWithCleanups>(s)) {
      s = ewc->getSubExpr();
    } else if (auto *mte = dyn_cast<MaterializeTemporaryExpr>(s)) {
      s = mte->GetTemporaryExpr();
    } else if (auto *bte = dyn_cast<CXXBindTemporaryExpr>(s)) {
      s = bte->getSubExpr();
    } else if (auto *ice = dyn_cast<ImplicitCastExpr>(s)) {
      s = ice->getSubExpr();
    } else if (auto *pe = dyn_cast<ParenExpr>(s)) {
      s = pe->getSubExpr();
    } else {
      break;
    }
  }

  return s;
}

const Expr *IgnoreTrivials(const Expr *e) {
  return cast<Expr>(IgnoreTrivials(static_cast<const Stmt *>(e)));
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
static CustomTypeAnnotation NonParam =
    CustomTypeAnnotation("moz_non_param", "non-param");

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
      StringRef Name = getNameChecked(D);
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
  DiagnosticsMatcher Matcher;

public:
  MozChecker(const CompilerInstance &CI) : Diag(CI.getDiagnostics()), CI(CI) {}

  ASTConsumerPtr getOtherConsumer() { return Matcher.makeASTConsumer(); }

  virtual void HandleTranslationUnit(ASTContext &Ctx) override {
    TraverseDecl(Ctx.getTranslationUnitDecl());
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

  void handleUnusedExprResult(const Stmt *Statement) {
    const Expr *E = dyn_cast_or_null<Expr>(Statement);
    if (E) {
      E = E->IgnoreImplicit(); // Ignore ExprWithCleanup etc. implicit wrappers
      QualType T = E->getType();
      if (MustUse.hasEffectiveAnnotation(T) && !isIgnoredExprForMustUse(E)) {
        unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error, "Unused value of must-use type %0");

        Diag.Report(E->getLocStart(), ErrorID) << T;
        MustUse.dumpAnnotationReason(Diag, T, E->getLocStart());
      }
    }
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *D) {
    // We need definitions, not declarations
    if (!D->isThisDeclarationADefinition())
      return true;

    // Look through all of our immediate bases to find methods that need to be
    // overridden
    typedef std::vector<CXXMethodDecl *> OverridesVector;
    OverridesVector MustOverrides;
    for (CXXRecordDecl::base_class_iterator Base = D->bases_begin(),
                                            E = D->bases_end();
         Base != E; ++Base) {
      // The base is either a class (CXXRecordDecl) or it's a templated class...
      CXXRecordDecl *Parent = Base->getType()
                                  .getDesugaredType(D->getASTContext())
                                  ->getAsCXXRecordDecl();
      // The parent might not be resolved to a type yet. In this case, we can't
      // do any checking here. For complete correctness, we should visit
      // template instantiations, but this case is likely to be rare, so we will
      // ignore it until it becomes important.
      if (!Parent) {
        continue;
      }
      Parent = Parent->getDefinition();
      for (CXXRecordDecl::method_iterator M = Parent->method_begin();
           M != Parent->method_end(); ++M) {
        if (hasCustomAnnotation(*M, "moz_must_override"))
          MustOverrides.push_back(*M);
      }
    }

    for (OverridesVector::iterator It = MustOverrides.begin();
         It != MustOverrides.end(); ++It) {
      bool Overridden = false;
      for (CXXRecordDecl::method_iterator M = D->method_begin();
           !Overridden && M != D->method_end(); ++M) {
        // The way that Clang checks if a method M overrides its parent method
        // is if the method has the same name but would not overload.
        if (getNameChecked(M) == getNameChecked(*It) &&
            !CI.getSema().IsOverload(*M, (*It), false)) {
          Overridden = true;
          break;
        }
      }
      if (!Overridden) {
        unsigned OverrideID = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error, "%0 must override %1");
        unsigned OverrideNote = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note, "function to override is here");
        Diag.Report(D->getLocation(), OverrideID) << D->getDeclName()
                                                  << (*It)->getDeclName();
        Diag.Report((*It)->getLocation(), OverrideNote);
      }
    }

    return true;
  }

  bool VisitSwitchCase(SwitchCase *Statement) {
    handleUnusedExprResult(Statement->getSubStmt());
    return true;
  }
  bool VisitCompoundStmt(CompoundStmt *Statement) {
    for (CompoundStmt::body_iterator It = Statement->body_begin(),
                                     E = Statement->body_end();
         It != E; ++It) {
      handleUnusedExprResult(*It);
    }
    return true;
  }
  bool VisitIfStmt(IfStmt *Statement) {
    handleUnusedExprResult(Statement->getThen());
    handleUnusedExprResult(Statement->getElse());
    return true;
  }
  bool VisitWhileStmt(WhileStmt *Statement) {
    handleUnusedExprResult(Statement->getBody());
    return true;
  }
  bool VisitDoStmt(DoStmt *Statement) {
    handleUnusedExprResult(Statement->getBody());
    return true;
  }
  bool VisitForStmt(ForStmt *Statement) {
    handleUnusedExprResult(Statement->getBody());
    handleUnusedExprResult(Statement->getInit());
    handleUnusedExprResult(Statement->getInc());
    return true;
  }
  bool VisitBinComma(BinaryOperator *Op) {
    handleUnusedExprResult(Op->getLHS());
    return true;
  }
};

/// A cached data of whether classes are refcounted or not.
typedef DenseMap<const CXXRecordDecl *, std::pair<const Decl *, bool>>
    RefCountedMap;
RefCountedMap RefCountedClasses;

bool classHasAddRefRelease(const CXXRecordDecl *D) {
  const RefCountedMap::iterator &It = RefCountedClasses.find(D);
  if (It != RefCountedClasses.end()) {
    return It->second.second;
  }

  bool SeenAddRef = false;
  bool SeenRelease = false;
  for (CXXRecordDecl::method_iterator Method = D->method_begin();
       Method != D->method_end(); ++Method) {
    const auto &Name = getNameChecked(Method);
    if (Name == "AddRef") {
      SeenAddRef = true;
    } else if (Name == "Release") {
      SeenRelease = true;
    }
  }
  RefCountedClasses[D] = std::make_pair(D, SeenAddRef && SeenRelease);
  return SeenAddRef && SeenRelease;
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
  for (CXXRecordDecl::base_class_const_iterator Base = D->bases_begin();
       Base != D->bases_end(); ++Base) {
    bool Super = isClassRefCounted(Base->getType());
    if (Super) {
      return true;
    }
  }

  return false;
}

bool isClassRefCounted(QualType T) {
  while (const clang::ArrayType *ArrTy = T->getAsArrayTypeUnsafe())
    T = ArrTy->getElementType();
  CXXRecordDecl *Clazz = T->getAsCXXRecordDecl();
  return Clazz ? isClassRefCounted(Clazz) : false;
}

template <class T> bool ASTIsInSystemHeader(const ASTContext &AC, const T &D) {
  auto &SourceManager = AC.getSourceManager();
  auto ExpansionLoc = SourceManager.getExpansionLoc(D.getLocStart());
  if (ExpansionLoc.isInvalid()) {
    return false;
  }
  return SourceManager.isInSystemHeader(ExpansionLoc);
}

const FieldDecl *getClassRefCntMember(const CXXRecordDecl *D) {
  for (RecordDecl::field_iterator Field = D->field_begin(), E = D->field_end();
       Field != E; ++Field) {
    if (getNameChecked(Field) == "mRefCnt") {
      return *Field;
    }
  }
  return 0;
}

const FieldDecl *getBaseRefCntMember(QualType T);

const FieldDecl *getBaseRefCntMember(const CXXRecordDecl *D) {
  const FieldDecl *RefCntMember = getClassRefCntMember(D);
  if (RefCntMember && isClassRefCounted(D)) {
    return RefCntMember;
  }

  for (CXXRecordDecl::base_class_const_iterator Base = D->bases_begin(),
                                                E = D->bases_end();
       Base != E; ++Base) {
    RefCntMember = getBaseRefCntMember(Base->getType());
    if (RefCntMember) {
      return RefCntMember;
    }
  }
  return 0;
}

const FieldDecl *getBaseRefCntMember(QualType T) {
  while (const clang::ArrayType *ArrTy = T->getAsArrayTypeUnsafe())
    T = ArrTy->getElementType();
  CXXRecordDecl *Clazz = T->getAsCXXRecordDecl();
  return Clazz ? getBaseRefCntMember(Clazz) : 0;
}

bool typeHasVTable(QualType T) {
  while (const clang::ArrayType *ArrTy = T->getAsArrayTypeUnsafe())
    T = ArrTy->getElementType();
  CXXRecordDecl *Offender = T->getAsCXXRecordDecl();
  return Offender && Offender->hasDefinition() && Offender->isDynamicClass();
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
  BinaryOperatorKind OpCode = Node.getOpcode();
  return OpCode == BO_Mul || OpCode == BO_Div || OpCode == BO_Rem ||
         OpCode == BO_Add || OpCode == BO_Sub || OpCode == BO_Shl ||
         OpCode == BO_Shr || OpCode == BO_And || OpCode == BO_Xor ||
         OpCode == BO_Or || OpCode == BO_MulAssign || OpCode == BO_DivAssign ||
         OpCode == BO_RemAssign || OpCode == BO_AddAssign ||
         OpCode == BO_SubAssign || OpCode == BO_ShlAssign ||
         OpCode == BO_ShrAssign || OpCode == BO_AndAssign ||
         OpCode == BO_XorAssign || OpCode == BO_OrAssign;
}

/// This matcher will match all arithmetic unary operators.
AST_MATCHER(UnaryOperator, unaryArithmeticOperator) {
  UnaryOperatorKind OpCode = Node.getOpcode();
  return OpCode == UO_PostInc || OpCode == UO_PostDec || OpCode == UO_PreInc ||
         OpCode == UO_PreDec || OpCode == UO_Plus || OpCode == UO_Minus ||
         OpCode == UO_Not;
}

/// This matcher will match == and != binary operators.
AST_MATCHER(BinaryOperator, binaryEqualityOperator) {
  BinaryOperatorKind OpCode = Node.getOpcode();
  return OpCode == BO_EQ || OpCode == BO_NE;
}

/// This matcher will match floating point types.
AST_MATCHER(QualType, isFloat) { return Node->isRealFloatingType(); }

/// This matcher will match locations in system headers.  This is adopted from
/// isExpansionInSystemHeader in newer clangs, but modified in order to work
/// with old clangs that we use on infra.
AST_MATCHER(BinaryOperator, isInSystemHeader) {
  return ASTIsInSystemHeader(Finder->getASTContext(), Node);
}

/// This matcher will match a list of files.  These files contain
/// known NaN-testing expressions which we would like to whitelist.
AST_MATCHER(BinaryOperator, isInWhitelistForNaNExpr) {
  const char* whitelist[] = {
    "SkScalar.h",
    "json_writer.cpp"
  };

  SourceLocation Loc = Node.getOperatorLoc();
  auto &SourceManager = Finder->getASTContext().getSourceManager();
  SmallString<1024> FileName = SourceManager.getFilename(Loc);

  for (auto itr = std::begin(whitelist); itr != std::end(whitelist); itr++) {
    if (llvm::sys::path::rbegin(FileName)->equals(*itr)) {
      return true;
    }
  }

  return false;
}

/// This matcher will match all accesses to AddRef or Release methods.
AST_MATCHER(MemberExpr, isAddRefOrRelease) {
  ValueDecl *Member = Node.getMemberDecl();
  CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(Member);
  if (Method) {
    const auto &Name = getNameChecked(Method);
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
  const CXXConstructorDecl *Declaration = Node.getCanonicalDecl();
  return
      // Skip constructors in system headers
      !ASTIsInSystemHeader(Declaration->getASTContext(), *Declaration) &&
      // Skip ignored namespaces and paths
      !isInIgnoredNamespaceForImplicitCtor(Declaration) &&
      !isIgnoredPathForImplicitCtor(Declaration) &&
      // We only want Converting constructors
      Declaration->isConvertingConstructor(false) &&
      // We don't want copy of move constructors, as those are allowed to be
      // implicit
      !Declaration->isCopyOrMoveConstructor() &&
      // We don't want deleted constructors.
      !Declaration->isDeleted();
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
  static const std::string AssertName = "MOZ_AssertAssignmentTest";
  const FunctionDecl *Method = Node.getDirectCallee();

  return Method
      && Method->getDeclName().isIdentifier()
      && Method->getName() == AssertName;
}

AST_MATCHER(CallExpr, isSnprintfLikeFunc) {
  static const std::string Snprintf = "snprintf";
  static const std::string Vsnprintf = "vsnprintf";
  const FunctionDecl *Func = Node.getDirectCallee();

  if (!Func || isa<CXXMethodDecl>(Func)) {
    return false;
  }

  StringRef Name = getNameChecked(Func);
  if (Name != Snprintf && Name != Vsnprintf) {
    return false;
  }

  return !isIgnoredPathForSprintfLiteral(&Node, Finder->getASTContext().getSourceManager());
}

AST_MATCHER(CXXRecordDecl, isLambdaDecl) {
  return Node.isLambda();
}

AST_MATCHER(QualType, isRefPtr) {
  return typeIsRefPtr(Node);
}

AST_MATCHER(CXXRecordDecl, hasBaseClasses) {
  const CXXRecordDecl *Decl = Node.getCanonicalDecl();

  // Must have definition and should inherit other classes
  return Decl && Decl->hasDefinition() && Decl->getNumBases();
}

AST_MATCHER(CXXMethodDecl, isRequiredBaseMethod) {
  const CXXMethodDecl *Decl = Node.getCanonicalDecl();
  return Decl
      && MozChecker::hasCustomAnnotation(Decl, "moz_required_base_method");
}

AST_MATCHER(CXXMethodDecl, isNonVirtual) {
  const CXXMethodDecl *Decl = Node.getCanonicalDecl();
  return Decl && !Decl->isVirtual();
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
      const CXXRecordDecl *Declaration = T->getAsCXXRecordDecl();
      assert(Declaration && "This type should be a C++ class");

      Diag.Report(Declaration->getLocation(), InheritsID) << Pretty << T
                                                   << Reason.Type;
      break;
    }
    case RK_Field:
      Diag.Report(Reason.Field->getLocation(), MemberID)
          << Pretty << T << Reason.Field << Reason.Type;
      break;
    case RK_TemplateInherited: {
      const CXXRecordDecl *Declaration = T->getAsCXXRecordDecl();
      assert(Declaration && "This type should be a C++ class");

      Diag.Report(Declaration->getLocation(), TemplID) << Pretty << T
                                                   << Reason.Type;
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

  // Recurse into Base classes
  if (const CXXRecordDecl *Declaration = T->getAsCXXRecordDecl()) {
    if (Declaration->hasDefinition()) {
      Declaration = Declaration->getDefinition();

      for (const CXXBaseSpecifier &Base : Declaration->bases()) {
        if (hasEffectiveAnnotation(Base.getType())) {
          AnnotationReason Reason = {Base.getType(), RK_BaseClass, nullptr};
          Cache[Key] = Reason;
          return Reason;
        }
      }

      // Recurse into members
      for (const FieldDecl *Field : Declaration->fields()) {
        if (hasEffectiveAnnotation(Field->getType())) {
          AnnotationReason Reason = {Field->getType(), RK_Field, Field};
          Cache[Key] = Reason;
          return Reason;
        }
      }

      // Recurse into template arguments if the annotation
      // MOZ_INHERIT_TYPE_ANNOTATIONS_FROM_TEMPLATE_ARGS is present
      if (MozChecker::hasCustomAnnotation(
              Declaration, "moz_inherit_type_annotations_from_template_args")) {
        const ClassTemplateSpecializationDecl *Spec =
            dyn_cast<ClassTemplateSpecializationDecl>(Declaration);
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

bool isPlacementNew(const CXXNewExpr *Expression) {
  // Regular new expressions aren't placement new
  if (Expression->getNumPlacementArgs() == 0)
    return false;
  const FunctionDecl *Declaration = Expression->getOperatorNew();
  if (Declaration && MozChecker::hasCustomAnnotation(Declaration,
                 "moz_heap_allocator")) {
    return false;
  }
  return true;
}

DiagnosticsMatcher::DiagnosticsMatcher() {
  AstMatcher.addMatcher(varDecl().bind("node"), &Scope);
  AstMatcher.addMatcher(cxxNewExpr().bind("node"), &Scope);
  AstMatcher.addMatcher(materializeTemporaryExpr().bind("node"), &Scope);
  AstMatcher.addMatcher(
      callExpr(callee(functionDecl(heapAllocator()))).bind("node"),
      &Scope);
  AstMatcher.addMatcher(parmVarDecl().bind("parm_vardecl"), &Scope);

  AstMatcher.addMatcher(
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
      &ArithmeticArg);
  AstMatcher.addMatcher(
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
      &ArithmeticArg);

  AstMatcher.addMatcher(cxxRecordDecl(hasTrivialCtorDtor()).bind("node"),
                        &TrivialCtorDtor);

  AstMatcher.addMatcher(
      binaryOperator(
          allOf(binaryEqualityOperator(),
                hasLHS(hasIgnoringParenImpCasts(
                    declRefExpr(hasType(qualType((isFloat())))).bind("lhs"))),
                hasRHS(hasIgnoringParenImpCasts(
                    declRefExpr(hasType(qualType((isFloat())))).bind("rhs"))),
                unless(anyOf(isInSystemHeader(), isInWhitelistForNaNExpr()))))
          .bind("node"),
      &NaNExpr);

  // First, look for direct parents of the MemberExpr.
  AstMatcher.addMatcher(
      callExpr(
          callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
          hasParent(memberExpr(isAddRefOrRelease(), hasParent(callExpr()))
                        .bind("member")))
          .bind("node"),
      &NoAddRefReleaseOnReturn);
  // Then, look for MemberExpr that need to be casted to the right type using
  // an intermediary CastExpr before we get to the CallExpr.
  AstMatcher.addMatcher(
      callExpr(
          callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
          hasParent(castExpr(
              hasParent(memberExpr(isAddRefOrRelease(), hasParent(callExpr()))
                            .bind("member")))))
          .bind("node"),
      &NoAddRefReleaseOnReturn);

  // We want to reject any code which captures a pointer to an object of a
  // refcounted type, and then lets that value escape. As a primitive analysis,
  // we reject any occurances of the lambda as a template parameter to a class
  // (which could allow it to escape), as well as any presence of such a lambda
  // in a return value (either from lambdas, or in c++14, auto functions).
  //
  // We check these lambdas' capture lists for raw pointers to refcounted types.
  AstMatcher.addMatcher(
      functionDecl(returns(recordType(hasDeclaration(cxxRecordDecl(
        isLambdaDecl()).bind("decl"))))),
      &RefCountedInsideLambda);
  AstMatcher.addMatcher(lambdaExpr().bind("lambdaExpr"),
      &RefCountedInsideLambda);
  AstMatcher.addMatcher(
      classTemplateSpecializationDecl(hasAnyTemplateArgument(refersToType(
        recordType(hasDeclaration(cxxRecordDecl(
          isLambdaDecl()).bind("decl")))))),
      &RefCountedInsideLambda);

  // Older clang versions such as the ones used on the infra recognize these
  // conversions as 'operator _Bool', but newer clang versions recognize these
  // as 'operator bool'.
  AstMatcher.addMatcher(
      cxxMethodDecl(anyOf(hasName("operator bool"), hasName("operator _Bool")))
          .bind("node"),
      &ExplicitOperatorBool);

  AstMatcher.addMatcher(cxxRecordDecl().bind("decl"), &NoDuplicateRefCntMember);

  AstMatcher.addMatcher(
      classTemplateSpecializationDecl(
          allOf(hasAnyTemplateArgument(refersToType(hasVTable())),
                hasNeedsNoVTableTypeAttr()))
          .bind("node"),
      &NeedsNoVTableType);

  // Handle non-mem-movable template specializations
  AstMatcher.addMatcher(
      classTemplateSpecializationDecl(
          allOf(needsMemMovableTemplateArg(),
                hasAnyTemplateArgument(refersToType(isNonMemMovable()))))
          .bind("specialization"),
      &NonMemMovableTemplateArg);

  // Handle non-mem-movable members
  AstMatcher.addMatcher(
      cxxRecordDecl(needsMemMovableMembers())
          .bind("decl"),
      &NonMemMovableMember);

  AstMatcher.addMatcher(cxxConstructorDecl(isInterestingImplicitCtor(),
                                           ofClass(allOf(isConcreteClass(),
                                                         decl().bind("class"))),
                                           unless(isMarkedImplicit()))
                            .bind("ctor"),
                        &ExplicitImplicit);

  AstMatcher.addMatcher(varDecl(hasType(autoNonAutoableType())).bind("node"),
                        &NoAutoType);

  AstMatcher.addMatcher(
      cxxConstructorDecl(isExplicitMoveConstructor()).bind("node"),
      &NoExplicitMoveConstructor);

  AstMatcher.addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(isCompilerProvidedCopyConstructor(),
                                            ofClass(hasRefCntMember()))))
          .bind("node"),
      &RefCountedCopyConstructor);

  AstMatcher.addMatcher(
      callExpr(isAssertAssignmentTestFunc()).bind("funcCall"),
      &AssertAttribution);

  AstMatcher.addMatcher(varDecl(hasType(isRefPtr())).bind("decl"),
                        &KungFuDeathGrip);

  AstMatcher.addMatcher(
      callExpr(isSnprintfLikeFunc(),
        allOf(hasArgument(0, ignoringParenImpCasts(declRefExpr().bind("buffer"))),
                             anyOf(hasArgument(1, sizeOfExpr(hasIgnoringParenImpCasts(declRefExpr().bind("size")))),
                                   hasArgument(1, integerLiteral().bind("immediate")),
                                   hasArgument(1, declRefExpr(to(varDecl(hasType(isConstQualified()),
                                                                         hasInitializer(integerLiteral().bind("constant")))))))))
        .bind("funcCall"),
      &SprintfLiteral
  );

  AstMatcher.addMatcher(cxxRecordDecl(hasBaseClasses()).bind("class"),
      &OverrideBaseCall);

  AstMatcher.addMatcher(
      cxxMethodDecl(isNonVirtual(), isRequiredBaseMethod()).bind("method"),
      &OverrideBaseCallUsage);

  AstMatcher.addMatcher(
      functionDecl(anyOf(allOf(isDefinition(),
                               hasAncestor(classTemplateSpecializationDecl()
                                               .bind("spec"))),
                         isDefinition()))
          .bind("func"),
      &NonParamInsideFunctionDecl);
  AstMatcher.addMatcher(
      lambdaExpr().bind("lambda"),
      &NonParamInsideFunctionDecl);
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
    if (D->hasUnparsedDefaultArg() || D->hasUninstantiatedDefaultArg()) {
      return;
    }
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
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "cannot pass an arithmetic expression of built-in types to %0");
  const Expr *Expression = Result.Nodes.getNodeAs<Expr>("node");
  if (const CallExpr *Call = Result.Nodes.getNodeAs<CallExpr>("call")) {
    Diag.Report(Expression->getLocStart(), ErrorID) << Call->getDirectCallee();
  } else if (const CXXConstructExpr *Ctr =
                 Result.Nodes.getNodeAs<CXXConstructExpr>("call")) {
    Diag.Report(Expression->getLocStart(), ErrorID) << Ctr->getConstructor();
  }
}

void DiagnosticsMatcher::TrivialCtorDtorChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "class %0 must have trivial constructors and destructors");
  const CXXRecordDecl *Node = Result.Nodes.getNodeAs<CXXRecordDecl>("node");

  // We need to accept non-constexpr trivial constructors as well. This occurs
  // when a struct contains pod members, which will not be initialized. As
  // constexpr values are initialized, the constructor is non-constexpr.
  bool BadCtor = !(Node->hasConstexprDefaultConstructor() ||
                   Node->hasTrivialDefaultConstructor());
  bool BadDtor = !Node->hasTrivialDestructor();
  if (BadCtor || BadDtor)
    Diag.Report(Node->getLocStart(), ErrorID) << Node;
}

void DiagnosticsMatcher::NaNExprChecker::run(
    const MatchFinder::MatchResult &Result) {
  if (!Result.Context->getLangOpts().CPlusPlus) {
    // mozilla::IsNaN is not usable in C, so there is no point in issuing these
    // warnings.
    return;
  }

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "comparing a floating point value to itself for "
                            "NaN checking can lead to incorrect results");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider using mozilla::IsNaN instead");
  const BinaryOperator *Expression = Result.Nodes.getNodeAs<BinaryOperator>(
    "node");
  const DeclRefExpr *LHS = Result.Nodes.getNodeAs<DeclRefExpr>("lhs");
  const DeclRefExpr *RHS = Result.Nodes.getNodeAs<DeclRefExpr>("rhs");
  const ImplicitCastExpr *LHSExpr = dyn_cast<ImplicitCastExpr>(
    Expression->getLHS());
  const ImplicitCastExpr *RHSExpr = dyn_cast<ImplicitCastExpr>(
    Expression->getRHS());
  // The AST subtree that we are looking for will look like this:
  // -BinaryOperator ==/!=
  //  |-ImplicitCastExpr LValueToRValue
  //  | |-DeclRefExpr
  //  |-ImplicitCastExpr LValueToRValue
  //    |-DeclRefExpr
  // The check below ensures that we are dealing with the correct AST subtree
  // shape, and
  // also that both of the found DeclRefExpr's point to the same declaration.
  if (LHS->getFoundDecl() == RHS->getFoundDecl() && LHSExpr && RHSExpr &&
      std::distance(LHSExpr->child_begin(), LHSExpr->child_end()) == 1 &&
      std::distance(RHSExpr->child_begin(), RHSExpr->child_end()) == 1 &&
      *LHSExpr->child_begin() == LHS && *RHSExpr->child_begin() == RHS) {
    Diag.Report(Expression->getLocStart(), ErrorID);
    Diag.Report(Expression->getLocStart(), NoteID);
  }
}

void DiagnosticsMatcher::NoAddRefReleaseOnReturnChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "%1 cannot be called on the return value of %0");
  const Stmt *Node = Result.Nodes.getNodeAs<Stmt>("node");
  const FunctionDecl *Func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  const MemberExpr *Member = Result.Nodes.getNodeAs<MemberExpr>("member");
  const CXXMethodDecl *Method =
      dyn_cast<CXXMethodDecl>(Member->getMemberDecl());

  Diag.Report(Node->getLocStart(), ErrorID) << Func << Method;
}

void DiagnosticsMatcher::RefCountedInsideLambdaChecker::run(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const CXXRecordDecl*> CheckedDecls;

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Refcounted variable %0 of type %1 cannot be captured by a lambda");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please consider using a smart pointer");

  const CXXRecordDecl *Lambda = Result.Nodes.getNodeAs<CXXRecordDecl>("decl");

  if (const LambdaExpr *OuterLambda =
    Result.Nodes.getNodeAs<LambdaExpr>("lambdaExpr")) {
    const CXXMethodDecl *OpCall = OuterLambda->getCallOperator();
    QualType ReturnTy = OpCall->getReturnType();
    if (const CXXRecordDecl *Record = ReturnTy->getAsCXXRecordDecl()) {
      Lambda = Record;
    }
  }

  if (!Lambda || !Lambda->isLambda()) {
    return;
  }

  // Don't report errors on the same declarations more than once.
  if (CheckedDecls.count(Lambda)) {
    return;
  }
  CheckedDecls.insert(Lambda);

  for (const LambdaCapture Capture : Lambda->captures()) {
    if (Capture.capturesVariable() && Capture.getCaptureKind() != LCK_ByRef) {
      QualType Pointee = Capture.getCapturedVar()->getType()->getPointeeType();

      if (!Pointee.isNull() && isClassRefCounted(Pointee)) {
        Diag.Report(Capture.getLocation(), ErrorID) << Capture.getCapturedVar()
                                                    << Pointee;
        Diag.Report(Capture.getLocation(), NoteID);
        return;
      }
    }
  }
}

void DiagnosticsMatcher::ExplicitOperatorBoolChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "bad implicit conversion operator for %0");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider adding the explicit keyword to %0");
  const CXXConversionDecl *Method =
      Result.Nodes.getNodeAs<CXXConversionDecl>("node");
  const CXXRecordDecl *Clazz = Method->getParent();

  if (!Method->isExplicitSpecified() &&
      !MozChecker::hasCustomAnnotation(Method, "moz_implicit") &&
      !ASTIsInSystemHeader(Method->getASTContext(), *Method) &&
      isInterestingDeclForImplicitConversion(Method)) {
    Diag.Report(Method->getLocStart(), ErrorID) << Clazz;
    Diag.Report(Method->getLocStart(), NoteID) << "'operator bool'";
  }
}

void DiagnosticsMatcher::NoDuplicateRefCntMemberChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  const CXXRecordDecl *D = Result.Nodes.getNodeAs<CXXRecordDecl>("decl");
  const FieldDecl *RefCntMember = getClassRefCntMember(D);
  const FieldDecl *FoundRefCntBase = nullptr;

  if (!D->hasDefinition())
    return;
  D = D->getDefinition();

  // If we don't have an mRefCnt member, and we have less than 2 superclasses,
  // we don't have to run this loop, as neither case will ever apply.
  if (!RefCntMember && D->getNumBases() < 2) {
    return;
  }

  // Check every superclass for whether it has a base with a refcnt member, and
  // warn for those which do
  for (auto &Base : D->bases()) {
    // Determine if this base class has an mRefCnt member
    const FieldDecl *BaseRefCntMember = getBaseRefCntMember(Base.getType());

    if (BaseRefCntMember) {
      if (RefCntMember) {
        // We have an mRefCnt, and superclass has an mRefCnt
        unsigned Error = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error,
            "Refcounted record %0 has multiple mRefCnt members");
        unsigned Note1 = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note, "Superclass %0 also has an mRefCnt member");
        unsigned Note2 = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note,
            "Consider using the _INHERITED macros for AddRef and Release here");

        Diag.Report(D->getLocStart(), Error) << D;
        Diag.Report(BaseRefCntMember->getLocStart(), Note1)
          << BaseRefCntMember->getParent();
        Diag.Report(RefCntMember->getLocStart(), Note2);
      }

      if (FoundRefCntBase) {
        unsigned Error = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Error,
            "Refcounted record %0 has multiple superclasses with mRefCnt members");
        unsigned Note = Diag.getDiagnosticIDs()->getCustomDiagID(
            DiagnosticIDs::Note,
            "Superclass %0 has an mRefCnt member");

        // superclass has mRefCnt, and another superclass also has an mRefCnt
        Diag.Report(D->getLocStart(), Error) << D;
        Diag.Report(BaseRefCntMember->getLocStart(), Note)
          << BaseRefCntMember->getParent();
        Diag.Report(FoundRefCntBase->getLocStart(), Note)
          << FoundRefCntBase->getParent();
      }

      // Record that we've found a base with a mRefCnt member
      FoundRefCntBase = BaseRefCntMember;
    }
  }
}

void DiagnosticsMatcher::NeedsNoVTableTypeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "%0 cannot be instantiated because %1 has a VTable");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "bad instantiation of %0 requested here");

  const ClassTemplateSpecializationDecl *Specialization =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("node");

  // Get the offending template argument
  QualType Offender;
  const TemplateArgumentList &Args =
      Specialization->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < Args.size(); ++i) {
    Offender = Args[i].getAsType();
    if (typeHasVTable(Offender)) {
      break;
    }
  }

  Diag.Report(Specialization->getLocStart(), ErrorID) << Specialization
                                                      << Offender;
  Diag.Report(Specialization->getPointOfInstantiation(), NoteID)
      << Specialization;
}

void DiagnosticsMatcher::NonMemMovableTemplateArgChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Cannot instantiate %0 with non-memmovable template argument %1");
  unsigned Note1ID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "instantiation of %0 requested here");

  // Get the specialization
  const ClassTemplateSpecializationDecl *Specialization =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("specialization");
  SourceLocation RequestLoc = Specialization->getPointOfInstantiation();

  // Report an error for every template argument which is non-memmovable
  const TemplateArgumentList &Args =
      Specialization->getTemplateInstantiationArgs();
  for (unsigned i = 0; i < Args.size(); ++i) {
    QualType ArgType = Args[i].getAsType();
    if (NonMemMovable.hasEffectiveAnnotation(ArgType)) {
      Diag.Report(Specialization->getLocation(), ErrorID) << Specialization
                                                          << ArgType;
      // XXX It would be really nice if we could get the instantiation stack
      // information
      // from Sema such that we could print a full template instantiation stack,
      // however,
      // it seems as though that information is thrown out by the time we get
      // here so we
      // can only report one level of template specialization (which in many
      // cases won't
      // be useful)
      Diag.Report(RequestLoc, Note1ID) << Specialization;
      NonMemMovable.dumpAnnotationReason(Diag, ArgType, RequestLoc);
    }
  }
}

void DiagnosticsMatcher::NonMemMovableMemberChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "class %0 cannot have non-memmovable member %1 of type %2");

  // Get the specialization
  const CXXRecordDecl* Declaration =
      Result.Nodes.getNodeAs<CXXRecordDecl>("decl");

  // Report an error for every member which is non-memmovable
  for (const FieldDecl *Field : Declaration->fields()) {
    QualType Type = Field->getType();
    if (NonMemMovable.hasEffectiveAnnotation(Type)) {
      Diag.Report(Field->getLocation(), ErrorID) << Declaration
                                                 << Field
                                                 << Type;
      NonMemMovable.dumpAnnotationReason(Diag, Type, Declaration->getLocation());
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
  const CXXRecordDecl *Declaration =
      Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  Diag.Report(Ctor->getLocation(), ErrorID) << Declaration->getDeclName();
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
  unsigned AssignInsteadOfComp = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Forbidden assignment in assert expression");
  const CallExpr *FuncCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  if (FuncCall && hasSideEffectAssignment(FuncCall)) {
    Diag.Report(FuncCall->getLocStart(), AssignInsteadOfComp);
  }
}

void DiagnosticsMatcher::KungFuDeathGripChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Unused \"kungFuDeathGrip\" %0 objects constructed from %1 are prohibited");

  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "Please switch all accesses to this %0 to go through '%1', or explicitly pass '%1' to `mozilla::Unused`");

  const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("decl");
  if (D->isReferenced() || !D->hasLocalStorage() || !D->hasInit()) {
    return;
  }

  // Not interested in parameters.
  if (isa<ImplicitParamDecl>(D) || isa<ParmVarDecl>(D)) {
    return;
  }

  const Expr *E = IgnoreTrivials(D->getInit());
  const CXXConstructExpr *CE = dyn_cast<CXXConstructExpr>(E);
  if (CE && CE->getNumArgs() == 0) {
    // We don't report an error when we construct and don't use a nsCOMPtr /
    // nsRefPtr with no arguments. We don't report it because the error is not
    // related to the current check. In the future it may be reported through a
    // more generic mechanism.
    return;
  }

  // We don't want to look at the single argument conversion constructors
  // which are inbetween the declaration and the actual object which we are
  // assigning into the nsCOMPtr/RefPtr. To do this, we repeatedly
  // IgnoreTrivials, then look at the expression. If it is one of these
  // conversion constructors, we ignore it and continue to dig.
  while ((CE = dyn_cast<CXXConstructExpr>(E)) && CE->getNumArgs() == 1) {
    E = IgnoreTrivials(CE->getArg(0));
  }

  // We allow taking a kungFuDeathGrip of `this` because it cannot change
  // beneath us, so calling directly through `this` is OK. This is the same
  // for local variable declarations.
  //
  // We also don't complain about unused RefPtrs which are constructed from
  // the return value of a new expression, as these are required in order to
  // immediately destroy the value created (which was presumably created for
  // its side effects), and are not used as a death grip.
  if (isa<CXXThisExpr>(E) || isa<DeclRefExpr>(E) || isa<CXXNewExpr>(E)) {
    return;
  }

  // These types are assigned into nsCOMPtr and RefPtr for their side effects,
  // and not as a kungFuDeathGrip. We don't want to consider RefPtr and nsCOMPtr
  // types which are initialized with these types as errors.
  const TagDecl *TD = E->getType()->getAsTagDecl();
  if (TD && TD->getIdentifier()) {
    static const char *IgnoreTypes[] = {
      "already_AddRefed",
      "nsGetServiceByCID",
      "nsGetServiceByCIDWithError",
      "nsGetServiceByContractID",
      "nsGetServiceByContractIDWithError",
      "nsCreateInstanceByCID",
      "nsCreateInstanceByContractID",
      "nsCreateInstanceFromFactory",
    };

    for (uint32_t i = 0; i < sizeof(IgnoreTypes) / sizeof(IgnoreTypes[0]); ++i) {
      if (TD->getName() == IgnoreTypes[i]) {
        return;
      }
    }
  }

  // Report the error
  const char *ErrThing;
  const char *NoteThing;
  if (isa<MemberExpr>(E)) {
    ErrThing  = "members";
    NoteThing = "member";
  } else {
    ErrThing = "temporary values";
    NoteThing = "value";
  }

  // We cannot provide the note if we don't have an initializer
  Diag.Report(D->getLocStart(), ErrorID) << D->getType() << ErrThing;
  Diag.Report(E->getLocStart(), NoteID) << NoteThing << getNameChecked(D);
}

void DiagnosticsMatcher::SprintfLiteralChecker::run(
    const MatchFinder::MatchResult &Result) {
  if (!Result.Context->getLangOpts().CPlusPlus) {
    // SprintfLiteral is not usable in C, so there is no point in issuing these
    // warnings.
    return;
  }

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Error, "Use %1 instead of %0 when writing into a character array.");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
    DiagnosticIDs::Note, "This will prevent passing in the wrong size to %0 accidentally.");

  const CallExpr *D = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  StringRef Name = D->getDirectCallee()->getName();
  const char *Replacement;
  if (Name == "snprintf") {
    Replacement = "SprintfLiteral";
  } else {
    assert(Name == "vsnprintf");
    Replacement = "VsprintfLiteral";
  }

  const DeclRefExpr *Buffer = Result.Nodes.getNodeAs<DeclRefExpr>("buffer");
  const DeclRefExpr *Size = Result.Nodes.getNodeAs<DeclRefExpr>("size");
  if (Size) {
    // Match calls like snprintf(x, sizeof(x), ...).
    if (Buffer->getFoundDecl() != Size->getFoundDecl()) {
      return;
    }

    Diag.Report(D->getLocStart(), ErrorID) << Name << Replacement;
    Diag.Report(D->getLocStart(), NoteID) << Name;
    return;
  }

  const QualType QType = Buffer->getType();
  const ConstantArrayType *Type = dyn_cast<ConstantArrayType>(QType.getTypePtrOrNull());
  if (Type) {
    // Match calls like snprintf(x, 100, ...), where x is int[100];
    const IntegerLiteral *Literal = Result.Nodes.getNodeAs<IntegerLiteral>("immediate");
    if (!Literal) {
      // Match calls like: const int y = 100; snprintf(x, y, ...);
      Literal = Result.Nodes.getNodeAs<IntegerLiteral>("constant");
    }

    if (Type->getSize().ule(Literal->getValue())) {
      Diag.Report(D->getLocStart(), ErrorID) << Name << Replacement;
      Diag.Report(D->getLocStart(), NoteID) << Name;
    }
  }
}

bool DiagnosticsMatcher::OverrideBaseCallChecker::isRequiredBaseMethod(
    const CXXMethodDecl *Method) {
  return MozChecker::hasCustomAnnotation(Method, "moz_required_base_method");
}

void DiagnosticsMatcher::OverrideBaseCallChecker::evaluateExpression(
    const Stmt *StmtExpr, std::list<const CXXMethodDecl*> &MethodList) {
  // Continue while we have methods in our list
  if (!MethodList.size()) {
    return;
  }

  if (auto MemberFuncCall = dyn_cast<CXXMemberCallExpr>(StmtExpr)) {
    if (auto Method = dyn_cast<CXXMethodDecl>(
        MemberFuncCall->getDirectCallee())) {
      findBaseMethodCall(Method, MethodList);
    }
  }

  for (auto S : StmtExpr->children()) {
    if (S) {
      evaluateExpression(S, MethodList);
    }
  }
}

void DiagnosticsMatcher::OverrideBaseCallChecker::getRequiredBaseMethod(
    const CXXMethodDecl *Method,
    std::list<const CXXMethodDecl*>& MethodsList) {

  if (isRequiredBaseMethod(Method)) {
    MethodsList.push_back(Method);
  } else {
    // Loop through all it's base methods.
    for (auto BaseMethod = Method->begin_overridden_methods();
        BaseMethod != Method->end_overridden_methods(); BaseMethod++) {
      getRequiredBaseMethod(*BaseMethod, MethodsList);
    }
  }
}

void DiagnosticsMatcher::OverrideBaseCallChecker::findBaseMethodCall(
    const CXXMethodDecl* Method,
    std::list<const CXXMethodDecl*>& MethodsList) {

  MethodsList.remove(Method);
  // Loop also through all it's base methods;
  for (auto BaseMethod = Method->begin_overridden_methods();
      BaseMethod != Method->end_overridden_methods(); BaseMethod++) {
    findBaseMethodCall(*BaseMethod, MethodsList);
  }
}

void DiagnosticsMatcher::OverrideBaseCallChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned OverrideBaseCallCheckID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "Method %0 must be called in all overrides, but is not called in "
      "this override defined for class %1");
  const CXXRecordDecl *Decl = Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  // Loop through the methods and look for the ones that are overridden.
  for (auto Method : Decl->methods()) {
    // If this method doesn't override other methods or it doesn't have a body,
    // continue to the next declaration.
    if (!Method->size_overridden_methods() || !Method->hasBody()) {
      continue;
    }

    // Preferred the usage of list instead of vector in order to avoid
    // calling erase-remove when deleting items
    std::list<const CXXMethodDecl*> MethodsList;
    // For each overridden method push it to a list if it meets our
    // criteria
    for (auto BaseMethod = Method->begin_overridden_methods();
        BaseMethod != Method->end_overridden_methods(); BaseMethod++) {
      getRequiredBaseMethod(*BaseMethod, MethodsList);
    }

    // If no method has been found then no annotation was used
    // so checking is not needed
    if (!MethodsList.size()) {
      continue;
    }

    // Loop through the body of our method and search for calls to
    // base methods
    evaluateExpression(Method->getBody(), MethodsList);

    // If list is not empty pop up errors
    for (auto BaseMethod : MethodsList) {
      Diag.Report(Method->getLocation(), OverrideBaseCallCheckID)
          << BaseMethod->getQualifiedNameAsString()
          << Decl->getName();
    }
  }
}

void DiagnosticsMatcher::OverrideBaseCallUsageChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "MOZ_REQUIRED_BASE_METHOD can be used only on virtual methods");
  const CXXMethodDecl *Method = Result.Nodes.getNodeAs<CXXMethodDecl>("method");

  Diag.Report(Method->getLocation(), ErrorID);
}

void DiagnosticsMatcher::NonParamInsideFunctionDeclChecker::run(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const FunctionDecl*> CheckedFunctionDecls;

  const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  if (!func) {
    const LambdaExpr *lambda = Result.Nodes.getNodeAs<LambdaExpr>("lambda");
    if (lambda) {
      func = lambda->getCallOperator();
    }
  }

  if (!func) {
    return;
  }

  if (func->isDeleted()) {
    return;
  }

  // Don't report errors on the same declarations more than once.
  if (CheckedFunctionDecls.count(func)) {
    return;
  }
  CheckedFunctionDecls.insert(func);

  const ClassTemplateSpecializationDecl *Spec =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("spec");

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Type %0 must not be used as parameter");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please consider passing a const reference instead");
  unsigned SpecNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "The bad argument was passed to %0 here");

  for (ParmVarDecl *p : func->parameters()) {
    QualType T = p->getType().withoutLocalFastQualifiers();
    if (NonParam.hasEffectiveAnnotation(T)) {
      Diag.Report(p->getLocation(), ErrorID) << T;
      Diag.Report(p->getLocation(), NoteID);

      if (Spec) {
        Diag.Report(Spec->getPointOfInstantiation(), SpecNoteID)
          << Spec->getSpecializedTemplate();
      }
    }
  }
}

class MozCheckAction : public PluginASTAction {
public:
  ASTConsumerPtr CreateASTConsumer(CompilerInstance &CI,
                                   StringRef FileName) override {
#if CLANG_VERSION_FULL >= 306
    std::unique_ptr<MozChecker> Checker(llvm::make_unique<MozChecker>(CI));
    ASTConsumerPtr Other(Checker->getOtherConsumer());

    std::vector<ASTConsumerPtr> Consumers;
    Consumers.push_back(std::move(Checker));
    Consumers.push_back(std::move(Other));
    return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
#else
    MozChecker *Checker = new MozChecker(CI);

    ASTConsumer *Consumers[] = {Checker, Checker->getOtherConsumer()};
    return new MultiplexConsumer(Consumers);
#endif
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &Args) override {
    return true;
  }
};
}

static FrontendPluginRegistry::Add<MozCheckAction> X("moz-check",
                                                     "check moz action");
