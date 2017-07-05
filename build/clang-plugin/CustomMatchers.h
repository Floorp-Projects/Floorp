/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomMatchers_h__
#define CustomMatchers_h__

#include "MemMoveAnnotation.h"
#include "Utils.h"
#include "VariableUsageHelpers.h"

namespace clang {
namespace ast_matchers {

/// This matcher will match any function declaration that is declared as a heap
/// allocator.
AST_MATCHER(FunctionDecl, heapAllocator) {
  return hasCustomAnnotation(&Node, "moz_heap_allocator");
}

/// This matcher will match any declaration that is marked as not accepting
/// arithmetic expressions in its arguments.
AST_MATCHER(Decl, noArithmeticExprInArgs) {
  return hasCustomAnnotation(&Node, "moz_no_arith_expr_in_arg");
}

/// This matcher will match any C++ class that is marked as having a trivial
/// constructor and destructor.
AST_MATCHER(CXXRecordDecl, hasTrivialCtorDtor) {
  return hasCustomAnnotation(&Node, "moz_trivial_ctor_dtor");
}

/// This matcher will match lvalue-ref-qualified methods.
AST_MATCHER(CXXMethodDecl, isLValueRefQualified) {
  return Node.getRefQualifier() == RQ_LValue;
}

/// This matcher will match rvalue-ref-qualified methods.
AST_MATCHER(CXXMethodDecl, isRValueRefQualified) {
  return Node.getRefQualifier() == RQ_RValue;
}

/// This matcher will match any method declaration that is marked as returning
/// a pointer deleted by the destructor of the class.
AST_MATCHER(CXXMethodDecl, noDanglingOnTemporaries) {
  return hasCustomAnnotation(&Node, "moz_no_dangling_on_temporaries");
}

/// This matcher will match an expression if it escapes the scope of the callee
/// of a parent call expression (skipping trivial parents).
/// The first inner matcher matches the statement where the escape happens, and
/// the second inner matcher corresponds to the declaration through which it
/// happens.
AST_MATCHER_P2(Expr, escapesParentFunctionCall, \
               internal::Matcher<Stmt>, EscapeStmtMatcher, \
               internal::Matcher<Decl>, EscapeDeclMatcher) {
  auto Call =
      IgnoreParentTrivials(Node, &Finder->getASTContext()).get<CallExpr>();
  if (!Call) {
    return false;
  }

  auto FunctionEscapeData = escapesFunction(&Node, Call);
  assert(FunctionEscapeData && "escapesFunction() returned NoneType: there is a"
                               " logic bug in the matcher");

  const Stmt* EscapeStmt;
  const Decl* EscapeDecl;
  std::tie(EscapeStmt, EscapeDecl) = *FunctionEscapeData;

  return EscapeStmt && EscapeDecl
      && EscapeStmtMatcher.matches(*EscapeStmt, Finder, Builder)
      && EscapeDeclMatcher.matches(*EscapeDecl, Finder, Builder);
}

/// This is the custom matcher class corresponding to hasNonTrivialParent.
template <typename T, typename ParentT>
class HasNonTrivialParentMatcher : public internal::WrapperMatcherInterface<T> {
  static_assert(internal::IsBaseType<ParentT>::value,
                "has parent only accepts base type matcher");

public:
  explicit HasNonTrivialParentMatcher(
      const internal::Matcher<ParentT> &NonTrivialParentMatcher)
      : HasNonTrivialParentMatcher::WrapperMatcherInterface(
            NonTrivialParentMatcher) {}

  bool matches(const T &Node, internal::ASTMatchFinder *Finder,
               internal::BoundNodesTreeBuilder *Builder) const override {
    auto NewNode = IgnoreParentTrivials(Node, &Finder->getASTContext());

    // We return the result of the inner matcher applied to the new node.
    return this->InnerMatcher.matches(NewNode, Finder, Builder);
  }
};

/// This matcher acts like hasParent, except it skips trivial constructs by
/// traversing the AST tree upwards.
const internal::ArgumentAdaptingMatcherFunc<
    HasNonTrivialParentMatcher,
    internal::TypeList<Decl, NestedNameSpecifierLoc, Stmt, TypeLoc>,
    internal::TypeList<Decl, NestedNameSpecifierLoc, Stmt, TypeLoc>>
    LLVM_ATTRIBUTE_UNUSED hasNonTrivialParent = {};

/// This matcher will match any function declaration that is marked to prohibit
/// calling AddRef or Release on its return value.
AST_MATCHER(FunctionDecl, hasNoAddRefReleaseOnReturnAttr) {
  return hasCustomAnnotation(&Node,
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

/// This matcher will match comma operator.
AST_MATCHER(BinaryOperator, binaryCommaOperator) {
  BinaryOperatorKind OpCode = Node.getOpcode();
  return OpCode == BO_Comma;
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
    "json_writer.cpp",
    "State.cpp"
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
  return hasCustomAnnotation(&Node, "moz_needs_no_vtable_type");
}

/// This matcher will select classes which are non-memmovable
AST_MATCHER(QualType, isNonMemMovable) {
  return NonMemMovable.hasEffectiveAnnotation(Node);
}

/// This matcher will select classes which require a memmovable template arg
AST_MATCHER(CXXRecordDecl, needsMemMovableTemplateArg) {
  return hasCustomAnnotation(&Node, "moz_needs_memmovable_type");
}

/// This matcher will select classes which require all members to be memmovable
AST_MATCHER(CXXRecordDecl, needsMemMovableMembers) {
  return hasCustomAnnotation(&Node, "moz_needs_memmovable_members");
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
  return hasCustomAnnotation(&Node, "moz_implicit");
}

AST_MATCHER(CXXRecordDecl, isConcreteClass) { return !Node.isAbstract(); }

AST_MATCHER(QualType, autoNonAutoableType) {
  if (const AutoType *T = Node->getContainedAutoType()) {
    if (const CXXRecordDecl *Rec = T->getAsCXXRecordDecl()) {
      return hasCustomAnnotation(Rec, "moz_non_autoable");
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
      && hasCustomAnnotation(Decl, "moz_required_base_method");
}

AST_MATCHER(CXXMethodDecl, isNonVirtual) {
  const CXXMethodDecl *Decl = Node.getCanonicalDecl();
  return Decl && !Decl->isVirtual();
}

AST_MATCHER(FunctionDecl, isMozMustReturnFromCaller) {
  const FunctionDecl *Decl = Node.getCanonicalDecl();
  return Decl && hasCustomAnnotation(Decl, "moz_must_return_from_caller");
}
}
}

#endif
