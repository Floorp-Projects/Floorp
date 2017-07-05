#include "VariableUsageHelpers.h"
#include "Utils.h"

std::vector<const Stmt*>
getUsageAsRvalue(const ValueDecl* ValueDeclaration,
                 const FunctionDecl* FuncDecl) {
  std::vector<const Stmt*> UsageStatements;

  // We check the function declaration has a body.
  auto Body = FuncDecl->getBody();
  if (!Body) {
    return std::vector<const Stmt*>();
  }

  // We build a Control Flow Graph (CFG) fron the body of the function
  // declaration.
  std::unique_ptr<CFG> StatementCFG
      = CFG::buildCFG(FuncDecl, Body, &FuncDecl->getASTContext(),
                      CFG::BuildOptions());

  // We iterate through all the CFGBlocks, which basically means that we go over
  // all the possible branches of the code and therefore cover all statements.
  for (auto& Block : *StatementCFG) {
    // We iterate through all the statements of the block.
    for (auto& BlockItem : *Block) {
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

Optional<std::tuple<const Stmt*, const Decl*>>
escapesFunction(const Expr* Arg, const CallExpr* Call) {
  // We get the function declaration corresponding to the call.
  auto FuncDecl = Call->getDirectCallee();
  if (!FuncDecl) {
    return NoneType();
  }

  // We find the argument number corresponding to the Arg expression.
  unsigned ArgNum = 0;
  for (auto CallArg : Call->arguments()) {
    if (IgnoreTrivials(Arg) == IgnoreTrivials(CallArg)) {
      break;
    }
    ++ArgNum;
  }
  // If we don't find it, we early-return NoneType.
  if (ArgNum >= Call->getNumArgs()) {
    return NoneType();
  }

  // Now we get the associated parameter.
  if (ArgNum >= FuncDecl->getNumParams()) {
    return NoneType();
  }
  auto Param = FuncDecl->getParamDecl(ArgNum);

  // We want both the argument and the parameter to be of pointer type.
  // FIXME: this is enough for the DanglingOnTemporaryChecker, because the
  // analysed methods only return pointers, but more cases should probably be
  // handled when we want to use this function more broadly.
  if (!Arg->getType()->isPointerType()
      || !Param->getType()->isPointerType()) {
    return NoneType();
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

        return {{Usage, ParamDeclaration}};
      } else if (auto VarDeclaration = dyn_cast<VarDecl>(DeclRef->getDecl())) {
        // This is the case where the parameter escapes through a global/static
        // variable.
        if (!VarDeclaration->hasGlobalStorage()) {
          continue;
        }

        return {{Usage, VarDeclaration}};
      }
    } else if (auto Return = dyn_cast<ReturnStmt>(Usage)) {
      // This is the case where the parameter escapes through the return value
      // of the function.
      if (!FuncDecl->getReturnType()->isPointerType()
          && !FuncDecl->getReturnType()->isReferenceType()) {
        continue;
      }

      return {{Usage, FuncDecl}};
    }
  }

  // No early-return, this means that we haven't found any case of funciton
  // escaping and that therefore the parameter remains in the function scope.
  return {{nullptr, nullptr}};
}
