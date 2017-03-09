/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StmtToBlockMap_h__
#define StmtToBlockMap_h__

#include "Utils.h"

// This method is copied from clang-tidy's ExprSequence.cpp.
//
// Returns the Stmt nodes that are parents of 'S', skipping any potential
// intermediate non-Stmt nodes.
//
// In almost all cases, this function returns a single parent or no parents at
// all.
inline SmallVector<const Stmt *, 1> getParentStmts(const Stmt *S,
                                                   ASTContext *Context) {
  SmallVector<const Stmt *, 1> Result;

  ASTContext::DynTypedNodeList Parents = Context->getParents(*S);

  SmallVector<ast_type_traits::DynTypedNode, 1> NodesToProcess(Parents.begin(),
                                                               Parents.end());

  while (!NodesToProcess.empty()) {
    ast_type_traits::DynTypedNode Node = NodesToProcess.back();
    NodesToProcess.pop_back();

    if (const auto *S = Node.get<Stmt>()) {
      Result.push_back(S);
    } else {
      Parents = Context->getParents(Node);
      NodesToProcess.append(Parents.begin(), Parents.end());
    }
  }

  return Result;
}

// This class is a modified version of the class from clang-tidy's ExprSequence.cpp
//
// Maps `Stmt`s to the `CFGBlock` that contains them. Some `Stmt`s may be
// contained in more than one `CFGBlock`; in this case, they are mapped to the
// innermost block (i.e. the one that is furthest from the root of the tree).
// An optional outparameter provides the index into the block where the `Stmt`
// was found.
class StmtToBlockMap {
public:
  // Initializes the map for the given `CFG`.
  StmtToBlockMap(const CFG *TheCFG, ASTContext *TheContext) : Context(TheContext) {
    for (const auto *B : *TheCFG) {
      for (size_t I = 0; I < B->size(); ++I) {
        if (Optional<CFGStmt> S = (*B)[I].getAs<CFGStmt>()) {
          Map[S->getStmt()] = std::make_pair(B, I);
        }
      }
    }
  }

  // Returns the block that S is contained in. Some `Stmt`s may be contained
  // in more than one `CFGBlock`; in this case, this function returns the
  // innermost block (i.e. the one that is furthest from the root of the tree).
  //
  // The optional outparameter `Index` is set to the index into the block where
  // the `Stmt` was found.
  const CFGBlock *blockContainingStmt(const Stmt *S, size_t *Index = nullptr) const {
    while (!Map.count(S)) {
      SmallVector<const Stmt *, 1> Parents = getParentStmts(S, Context);
      if (Parents.empty())
        return nullptr;
      S = Parents[0];
    }

    const auto &E = Map.lookup(S);
    if (Index) *Index = E.second;
    return E.first;
  }

private:
  ASTContext *Context;

  llvm::DenseMap<const Stmt *, std::pair<const CFGBlock *, size_t>> Map;
};

#endif // StmtToBlockMap_h__
