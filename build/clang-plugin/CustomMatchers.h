/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CustomMatchers_h__
#define CustomMatchers_h__

#include "MemMoveAnnotation.h"
#include "Utils.h"

namespace clang {
namespace ast_matchers {

/// This matcher will match any function declaration that is declared as a heap
/// allocator.
AST_MATCHER(FunctionDecl, heapAllocator) {
  return hasCustomAttribute<moz_heap_allocator>(&Node);
}

/// This matcher will match any declaration that is marked as not accepting
/// arithmetic expressions in its arguments.
AST_MATCHER(Decl, noArithmeticExprInArgs) {
  return hasCustomAttribute<moz_no_arith_expr_in_arg>(&Node);
}

/// This matcher will match any C++ class that is marked as having a trivial
/// constructor and destructor.
AST_MATCHER(CXXRecordDecl, hasTrivialCtorDtor) {
  return hasCustomAttribute<moz_trivial_ctor_dtor>(&Node);
}

AST_MATCHER(CXXConstructExpr, allowsTemporary) {
  return hasCustomAttribute<moz_allow_temporary>(Node.getConstructor());
}

/// This matcher will match lvalue-ref-qualified methods.
AST_MATCHER(CXXMethodDecl, isLValueRefQualified) {
  return Node.getRefQualifier() == RQ_LValue;
}

/// This matcher will match rvalue-ref-qualified methods.
AST_MATCHER(CXXMethodDecl, isRValueRefQualified) {
  return Node.getRefQualifier() == RQ_RValue;
}

AST_POLYMORPHIC_MATCHER(isFirstParty,
                        AST_POLYMORPHIC_SUPPORTED_TYPES(Decl, Stmt)) {
  return !inThirdPartyPath(&Node, &Finder->getASTContext()) &&
         !ASTIsInSystemHeader(Finder->getASTContext(), Node);
}

/// This matcher will match temporary expressions.
/// We need this matcher for compatibility with clang 3.* (clang 4 and above
/// insert a MaterializeTemporaryExpr everywhere).
AST_MATCHER(Expr, isTemporary) {
  return Node.isRValue() || Node.isXValue() ||
         isa<MaterializeTemporaryExpr>(&Node);
}

/// This matcher will match any method declaration that is marked as returning
/// a pointer deleted by the destructor of the class.
AST_MATCHER(CXXMethodDecl, noDanglingOnTemporaries) {
  return hasCustomAttribute<moz_no_dangling_on_temporaries>(&Node);
}

/// This matcher will match any function declaration that is marked to prohibit
/// calling AddRef or Release on its return value.
AST_MATCHER(FunctionDecl, hasNoAddRefReleaseOnReturnAttr) {
  return hasCustomAttribute<moz_no_addref_release_on_return>(&Node);
}

/// This matcher will match any function declaration that is marked as being
/// allowed to run script.
AST_MATCHER(FunctionDecl, hasCanRunScriptAnnotation) {
  return hasCustomAttribute<moz_can_run_script>(&Node);
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

/// This matcher will match the unary dereference operator
AST_MATCHER(UnaryOperator, unaryDereferenceOperator) {
  UnaryOperatorKind OpCode = Node.getOpcode();
  return OpCode == UO_Deref;
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
AST_POLYMORPHIC_MATCHER(isInSystemHeader,                                      \
                        AST_POLYMORPHIC_SUPPORTED_TYPES(Decl, Stmt)) {
  return ASTIsInSystemHeader(Finder->getASTContext(), Node);
}

/// This matcher will match a list of files.  These files contain
/// known NaN-testing expressions which we would like to whitelist.
AST_MATCHER(BinaryOperator, isInWhitelistForNaNExpr) {
  const char *whitelist[] = {"SkScalar.h", "json_writer.cpp", "State.cpp"};

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

/// This matcher will select classes which are refcounted AND have an mRefCnt
/// member.
AST_MATCHER(CXXRecordDecl, hasRefCntMember) {
  return isClassRefCounted(&Node) && getClassRefCntMember(&Node);
}

/// This matcher will select classes which are refcounted.
AST_MATCHER(CXXRecordDecl, isRefCounted) { return isClassRefCounted(&Node); }

AST_MATCHER(QualType, hasVTable) { return typeHasVTable(Node); }

AST_MATCHER(CXXRecordDecl, hasNeedsNoVTableTypeAttr) {
  return hasCustomAttribute<moz_needs_no_vtable_type>(&Node);
}

/// This matcher will select classes which are non-memmovable
AST_MATCHER(QualType, isNonMemMovable) {
  return NonMemMovable.hasEffectiveAnnotation(Node);
}

/// This matcher will select classes which require a memmovable template arg
AST_MATCHER(CXXRecordDecl, needsMemMovableTemplateArg) {
  return hasCustomAttribute<moz_needs_memmovable_type>(&Node);
}

/// This matcher will select classes which require all members to be memmovable
AST_MATCHER(CXXRecordDecl, needsMemMovableMembers) {
  return hasCustomAttribute<moz_needs_memmovable_members>(&Node);
}

AST_MATCHER(CXXConstructorDecl, isInterestingImplicitCtor) {
  const CXXConstructorDecl *Declaration = Node.getCanonicalDecl();
  return
      // Skip constructors in system headers
      !ASTIsInSystemHeader(Declaration->getASTContext(), *Declaration) &&
      // Skip ignored namespaces and paths
      !isInIgnoredNamespaceForImplicitCtor(Declaration) &&
      !inThirdPartyPath(Declaration) &&
      // We only want Converting constructors
      Declaration->isConvertingConstructor(false) &&
      // We don't want copy of move constructors, as those are allowed to be
      // implicit
      !Declaration->isCopyOrMoveConstructor() &&
      // We don't want deleted constructors.
      !Declaration->isDeleted();
}

AST_MATCHER_P(Expr, ignoreTrivials, internal::Matcher<Expr>, InnerMatcher) {
  return InnerMatcher.matches(*IgnoreTrivials(&Node), Finder, Builder);
}

// Takes two matchers: the first one is a condition; the second is a matcher to be
// applied once we are done unwrapping trivials.  While the condition does not match
// and we're looking at a trivial, will keep unwrapping the trivial and trying again.
// Once the condition matches, we will go ahead and unwrap all trivials and apply the
// inner matcher to the result.
//
// The expected use here is if we want to condition a match on some typecheck but
// apply the match to only non-trivials, because there are trivials (e.g. casts) that
// can change types.
AST_MATCHER_P2(Expr, ignoreTrivialsConditional,
               internal::Matcher<Expr>, Condition,
               internal::Matcher<Expr>, InnerMatcher) {
  const Expr *node = &Node;
  while (true) {
    if (Condition.matches(*node, Finder, Builder)) {
      return InnerMatcher.matches(*IgnoreTrivials(node), Finder, Builder);
    }
    const Expr *newNode = MaybeSkipOneTrivial(node);
    if (newNode == node) {
      return false;
    }
    node = newNode;
  }
}

// We can't call this "isImplicit" since it clashes with an existing matcher in
// clang.
AST_MATCHER(CXXConstructorDecl, isMarkedImplicit) {
  return hasCustomAttribute<moz_implicit>(&Node);
}

AST_MATCHER(CXXRecordDecl, isConcreteClass) { return !Node.isAbstract(); }

AST_MATCHER(QualType, autoNonAutoableType) {
  if (const AutoType *T = Node->getContainedAutoType()) {
    if (const CXXRecordDecl *Rec = T->getAsCXXRecordDecl()) {
      return hasCustomAttribute<moz_non_autoable>(Rec);
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

  return Method && Method->getDeclName().isIdentifier() &&
         Method->getName() == AssertName;
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

  return !inThirdPartyPath(Node.getBeginLoc(),
                           Finder->getASTContext().getSourceManager()) &&
         !isIgnoredPathForSprintfLiteral(
             &Node, Finder->getASTContext().getSourceManager());
}

AST_MATCHER(CXXRecordDecl, isLambdaDecl) { return Node.isLambda(); }

AST_MATCHER(QualType, isRefPtr) { return typeIsRefPtr(Node); }

AST_MATCHER(QualType, isSmartPtrToRefCounted) {
  auto *D = getNonTemplateSpecializedCXXRecordDecl(Node);
  if (!D) {
    return false;
  }

  D = D->getCanonicalDecl();

  return D && hasCustomAttribute<moz_is_smartptr_to_refcounted>(D);
}

AST_MATCHER(CXXRecordDecl, hasBaseClasses) {
  const CXXRecordDecl *Decl = Node.getCanonicalDecl();

  // Must have definition and should inherit other classes
  return Decl && Decl->hasDefinition() && Decl->getNumBases();
}

AST_MATCHER(CXXMethodDecl, isRequiredBaseMethod) {
  const CXXMethodDecl *Decl = Node.getCanonicalDecl();
  return Decl && hasCustomAttribute<moz_required_base_method>(Decl);
}

AST_MATCHER(CXXMethodDecl, isNonVirtual) {
  const CXXMethodDecl *Decl = Node.getCanonicalDecl();
  return Decl && !Decl->isVirtual();
}

AST_MATCHER(FunctionDecl, isMozMustReturnFromCaller) {
  const FunctionDecl *Decl = Node.getCanonicalDecl();
  return Decl && hasCustomAttribute<moz_must_return_from_caller>(Decl);
}

/// This matcher will select default args which have nullptr as the value.
AST_MATCHER(CXXDefaultArgExpr, isNullDefaultArg) {
  const Expr *Expr = Node.getExpr();
  return Expr && Expr->isNullPointerConstant(Finder->getASTContext(),
                                             Expr::NPC_NeverValueDependent);
}

#if CLANG_VERSION_FULL < 309
/// DISCLAIMER: This is a copy/paste from the Clang source code starting from
/// Clang 3.9, so that this matcher is supported in lower versions.
///
/// \brief Matches declaration of the function the statement belongs to
///
/// Given:
/// \code
/// F& operator=(const F& o) {
///   std::copy_if(o.begin(), o.end(), begin(), [](V v) { return v > 0; });
///   return *this;
/// }
/// \endcode
/// returnStmt(forFunction(hasName("operator=")))
///   matches 'return *this'
///   but does match 'return > 0'
AST_MATCHER_P(Stmt, forFunction, internal::Matcher<FunctionDecl>,
              InnerMatcher) {
  const auto &Parents = Finder->getASTContext().getParents(Node);

  llvm::SmallVector<ast_type_traits::DynTypedNode, 8> Stack(Parents.begin(),
                                                            Parents.end());
  while (!Stack.empty()) {
    const auto &CurNode = Stack.back();
    Stack.pop_back();
    if (const auto *FuncDeclNode = CurNode.get<FunctionDecl>()) {
      if (InnerMatcher.matches(*FuncDeclNode, Finder, Builder)) {
        return true;
      }
    } else if (const auto *LambdaExprNode = CurNode.get<LambdaExpr>()) {
      if (InnerMatcher.matches(*LambdaExprNode->getCallOperator(), Finder,
                               Builder)) {
        return true;
      }
    } else {
      for (const auto &Parent : Finder->getASTContext().getParents(CurNode))
        Stack.push_back(Parent);
    }
  }
  return false;
}
#endif

} // namespace ast_matchers
} // namespace clang

#endif
