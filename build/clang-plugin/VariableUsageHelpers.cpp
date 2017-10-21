#include "VariableUsageHelpers.h"
#include "Utils.h"

std::vector<const Stmt *> getUsageAsRvalue(const ValueDecl *ValueDeclaration,
                                           const FunctionDecl *FuncDecl) {
  std::vector<const Stmt *> UsageStatements;

  // We check the function declaration has a body.
  auto Body = FuncDecl->getBody();
  if (!Body) {
    return std::vector<const Stmt *>();
  }

  // We build a Control Flow Graph (CFG) fron the body of the function
  // declaration.
  std::unique_ptr<CFG> StatementCFG = CFG::buildCFG(
      FuncDecl, Body, &FuncDecl->getASTContext(), CFG::BuildOptions());

  // We iterate through all the CFGBlocks, which basically means that we go over
  // all the possible branches of the code and therefore cover all statements.
  for (auto &Block : *StatementCFG) {
    // We iterate through all the statements of the block.
    for (auto &BlockItem : *Block) {
      Optional<CFGStmt> CFGStatement = BlockItem.getAs<CFGStmt>();
      if (!CFGStatement) {
        continue;
      }

      // FIXME: Right now this function/if chain is very basic and only covers
      // the cases we need for escapesFunction()
      if (auto BinOp = dyn_cast<BinaryOperator>(CFGStatement->getStmt())) {
        // We only care about assignments.
        if (BinOp->getOpcode() != BO_Assign) {
          continue;
        }

        // We want our declaration to be used on the right hand side of the
        // assignment.
        auto DeclRef = dyn_cast<DeclRefExpr>(IgnoreTrivials(BinOp->getRHS()));
        if (!DeclRef) {
          continue;
        }

        if (DeclRef->getDecl() != ValueDeclaration) {
          continue;
        }
      } else if (auto Return = dyn_cast<ReturnStmt>(CFGStatement->getStmt())) {
        // We want our declaration to be used as the expression of the return
        // statement.
        auto DeclRef = dyn_cast_or_null<DeclRefExpr>(
            IgnoreTrivials(Return->getRetValue()));
        if (!DeclRef) {
          continue;
        }

        if (DeclRef->getDecl() != ValueDeclaration) {
          continue;
        }
      } else {
        continue;
      }

      // We didn't early-continue, so we add the statement to the list.
      UsageStatements.push_back(CFGStatement->getStmt());
    }
  }

  return UsageStatements;
}

// We declare our EscapesFunctionError enum to be an error code enum.
namespace std {
template <> struct is_error_code_enum<EscapesFunctionError> : true_type {};
} // namespace std

// We define the EscapesFunctionErrorCategory which contains the error messages
// corresponding to each enum variant.
namespace {
struct EscapesFunctionErrorCategory : std::error_category {
  const char *name() const noexcept override;
  std::string message(int ev) const override;
};

const char *EscapesFunctionErrorCategory::name() const noexcept {
  return "escapes function";
}

std::string EscapesFunctionErrorCategory::message(int ev) const {
  switch (static_cast<EscapesFunctionError>(ev)) {
  case EscapesFunctionError::ConstructorDeclNotFound:
    return "constructor declaration not found";

  case EscapesFunctionError::FunctionDeclNotFound:
    return "function declaration not found";

  case EscapesFunctionError::FunctionIsBuiltin:
    return "function is builtin";

  case EscapesFunctionError::FunctionIsVariadic:
    return "function is variadic";

  case EscapesFunctionError::ExprNotInCall:
    return "expression is not in call";

  case EscapesFunctionError::NoParamForArg:
    return "no parameter for argument";

  case EscapesFunctionError::ArgAndParamNotPointers:
    return "argument and parameter are not pointers";
  }
}

const EscapesFunctionErrorCategory TheEscapesFunctionErrorCategory{};
} // namespace

std::error_code make_error_code(EscapesFunctionError e) {
  return {static_cast<int>(e), TheEscapesFunctionErrorCategory};
}

ErrorOr<std::tuple<const Stmt *, const Decl *>>
escapesFunction(const Expr *Arg, const CXXConstructExpr *Construct) {
  // We get the function declaration corresponding to the call.
  auto CtorDecl = Construct->getConstructor();
  if (!CtorDecl) {
    return EscapesFunctionError::ConstructorDeclNotFound;
  }

  return escapesFunction(Arg, CtorDecl, Construct->getArgs(),
                         Construct->getNumArgs());
}

ErrorOr<std::tuple<const Stmt *, const Decl *>>
escapesFunction(const Expr *Arg, const CallExpr *Call) {
  // We get the function declaration corresponding to the call.
  auto FuncDecl = Call->getDirectCallee();
  if (!FuncDecl) {
    return EscapesFunctionError::FunctionDeclNotFound;
  }

  return escapesFunction(Arg, FuncDecl, Call->getArgs(), Call->getNumArgs());
}

ErrorOr<std::tuple<const Stmt *, const Decl *>>
escapesFunction(const Expr *Arg, const CXXOperatorCallExpr *OpCall) {
  // We get the function declaration corresponding to the operator call.
  auto FuncDecl = OpCall->getDirectCallee();
  if (!FuncDecl) {
    return EscapesFunctionError::FunctionDeclNotFound;
  }

  auto Args = OpCall->getArgs();
  auto NumArgs = OpCall->getNumArgs();
  // If this is an infix binary operator defined as a one-param method, we
  // remove the first argument as it is inserted explicitly and creates a
  // mismatch with the parameters of the method declaration.
  if (isInfixBinaryOp(OpCall) && FuncDecl->getNumParams() == 1) {
    Args++;
    NumArgs--;
  }

  return escapesFunction(Arg, FuncDecl, Args, NumArgs);
}

ErrorOr<std::tuple<const Stmt *, const Decl *>>
escapesFunction(const Expr *Arg, const FunctionDecl *FuncDecl,
                const Expr *const *Arguments, unsigned NumArgs) {
  if (!NumArgs) {
    return std::make_tuple((const Stmt *)nullptr, (const Decl *)nullptr);
  }

  if (FuncDecl->getBuiltinID() != 0 ||
      ASTIsInSystemHeader(FuncDecl->getASTContext(), *FuncDecl)) {
    return EscapesFunctionError::FunctionIsBuiltin;
  }

  // FIXME: should probably be handled at some point, but it's too annoying
  // for now.
  if (FuncDecl->isVariadic()) {
    return EscapesFunctionError::FunctionIsVariadic;
  }

  // We find the argument number corresponding to the Arg expression.
  unsigned ArgNum = 0;
  for (unsigned i = 0; i < NumArgs; i++) {
    if (IgnoreTrivials(Arg) == IgnoreTrivials(Arguments[i])) {
      break;
    }
    ++ArgNum;
  }
  // If we don't find it, we early-return NoneType.
  if (ArgNum >= NumArgs) {
    return EscapesFunctionError::ExprNotInCall;
  }

  // Now we get the associated parameter.
  if (ArgNum >= FuncDecl->getNumParams()) {
    return EscapesFunctionError::NoParamForArg;
  }
  auto Param = FuncDecl->getParamDecl(ArgNum);

  // We want both the argument and the parameter to be of pointer type.
  // FIXME: this is enough for the DanglingOnTemporaryChecker, because the
  // analysed methods only return pointers, but more cases should probably be
  // handled when we want to use this function more broadly.
  if ((!Arg->getType().getNonReferenceType()->isPointerType() &&
       Arg->getType().getNonReferenceType()->isBuiltinType()) ||
      (!Param->getType().getNonReferenceType()->isPointerType() &&
       Param->getType().getNonReferenceType()->isBuiltinType())) {
    return EscapesFunctionError::ArgAndParamNotPointers;
  }

  // We retrieve the usages of the parameter in the function.
  auto Usages = getUsageAsRvalue(Param, FuncDecl);

  // For each usage, we check if it doesn't allow the parameter to escape the
  // function scope.
  for (auto Usage : Usages) {
    // In the case of an assignment.
    if (auto BinOp = dyn_cast<BinaryOperator>(Usage)) {
      // We retrieve the declaration the parameter is assigned to.
      auto DeclRef = dyn_cast<DeclRefExpr>(BinOp->getLHS());
      if (!DeclRef) {
        continue;
      }

      if (auto ParamDeclaration = dyn_cast<ParmVarDecl>(DeclRef->getDecl())) {
        // This is the case where the parameter escapes through another
        // parameter.

        // FIXME: for now we only care about references because we only detect
        // trivial LHS with just a DeclRefExpr, and not more complex cases like:
        // void func(Type* param1, Type** param2) {
        //   *param2 = param1;
        // }
        // This should be fixed when we have better/more helper functions to
        // help deal with this kind of lvalue expressions.
        if (!ParamDeclaration->getType()->isReferenceType()) {
          continue;
        }

        return std::make_tuple(Usage, (const Decl *)ParamDeclaration);
      } else if (auto VarDeclaration = dyn_cast<VarDecl>(DeclRef->getDecl())) {
        // This is the case where the parameter escapes through a global/static
        // variable.
        if (!VarDeclaration->hasGlobalStorage()) {
          continue;
        }

        return std::make_tuple(Usage, (const Decl *)VarDeclaration);
      } else if (auto FieldDeclaration =
                     dyn_cast<FieldDecl>(DeclRef->getDecl())) {
        // This is the case where the parameter escapes through a field.

        return std::make_tuple(Usage, (const Decl *)FieldDeclaration);
      }
    } else if (isa<ReturnStmt>(Usage)) {
      // This is the case where the parameter escapes through the return value
      // of the function.
      if (!FuncDecl->getReturnType()->isPointerType() &&
          !FuncDecl->getReturnType()->isReferenceType()) {
        continue;
      }

      return std::make_tuple(Usage, (const Decl *)FuncDecl);
    }
  }

  // No early-return, this means that we haven't found any case of funciton
  // escaping and that therefore the parameter remains in the function scope.
  return std::make_tuple((const Stmt *)nullptr, (const Decl *)nullptr);
}
