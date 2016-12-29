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
}
}

#endif
