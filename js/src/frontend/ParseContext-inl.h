/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ParseContext_inl_h
#define frontend_ParseContext_inl_h

#include "frontend/ParseContext.h"
#include "frontend/Parser.h"
#include "vm/JSContext.h"

namespace js {
namespace frontend {

template <>
inline bool ParseContext::Statement::is<ParseContext::LabelStatement>() const {
  return kind_ == StatementKind::Label;
}

template <>
inline bool ParseContext::Statement::is<ParseContext::ClassStatement>() const {
  return kind_ == StatementKind::Class;
}

template <typename T>
inline T& ParseContext::Statement::as() {
  MOZ_ASSERT(is<T>());
  return static_cast<T&>(*this);
}

inline ParseContext::Scope::BindingIter ParseContext::Scope::bindings(
    ParseContext* pc) {
  // In function scopes with parameter expressions, function special names
  // (like '.this') are declared as vars in the function scope, despite its
  // not being the var scope.
  return BindingIter(*this, pc->varScope_ == this ||
                                pc->functionScope_.ptrOr(nullptr) == this);
}

inline ParseContext::Scope::Scope(ParserBase* parser)
    : Nestable<Scope>(&parser->pc->innermostScope_),
      declared_(parser->context->frontendCollectionPool()),
      possibleAnnexBFunctionBoxes_(parser->context->frontendCollectionPool()),
      id_(parser->usedNames.nextScopeId()) {}

inline ParseContext::Scope::Scope(JSContext* cx, ParseContext* pc,
                                  UsedNameTracker& usedNames)
    : Nestable<Scope>(&pc->innermostScope_),
      declared_(cx->frontendCollectionPool()),
      possibleAnnexBFunctionBoxes_(cx->frontendCollectionPool()),
      id_(usedNames.nextScopeId()) {}

inline ParseContext::VarScope::VarScope(ParserBase* parser) : Scope(parser) {
  useAsVarScope(parser->pc);
}

inline ParseContext::VarScope::VarScope(JSContext* cx, ParseContext* pc,
                                        UsedNameTracker& usedNames)
    : Scope(cx, pc, usedNames) {
  useAsVarScope(pc);
}

inline JS::Result<Ok, ParseContext::BreakStatementError>
ParseContext::checkBreakStatement(PropertyName* label) {
  // Labeled 'break' statements target the nearest labeled statements (could
  // be any kind) with the same label. Unlabeled 'break' statements target
  // the innermost loop or switch statement.
  if (label) {
    auto hasSameLabel = [&label](ParseContext::LabelStatement* stmt) {
      MOZ_ASSERT(stmt);
      return stmt->label() == label;
    };

    if (!findInnermostStatement<ParseContext::LabelStatement>(hasSameLabel)) {
      return mozilla::Err(ParseContext::BreakStatementError::LabelNotFound);
    }

  } else {
    auto isBreakTarget = [](ParseContext::Statement* stmt) {
      return StatementKindIsUnlabeledBreakTarget(stmt->kind());
    };

    if (!findInnermostStatement(isBreakTarget)) {
      return mozilla::Err(ParseContext::BreakStatementError::ToughBreak);
    }
  }

  return Ok();
}

inline JS::Result<Ok, ParseContext::ContinueStatementError>
ParseContext::checkContinueStatement(PropertyName* label) {
  // Labeled 'continue' statements target the nearest labeled loop
  // statements with the same label. Unlabeled 'continue' statements target
  // the innermost loop statement.
  auto isLoop = [](ParseContext::Statement* stmt) {
    MOZ_ASSERT(stmt);
    return StatementKindIsLoop(stmt->kind());
  };

  if (!label) {
    // Unlabeled statement: we target the innermost loop, so make sure that
    // there is an innermost loop.
    if (!findInnermostStatement(isLoop)) {
      return mozilla::Err(ParseContext::ContinueStatementError::NotInALoop);
    }
    return Ok();
  }

  // Labeled statement: targest the nearest labeled loop with the same label.
  ParseContext::Statement* stmt = innermostStatement();
  bool foundLoop = false;  // True if we have encountered at least one loop.

  for (;;) {
    stmt = ParseContext::Statement::findNearest(stmt, isLoop);
    if (!stmt) {
      return foundLoop
                 ? mozilla::Err(
                       ParseContext::ContinueStatementError::LabelNotFound)
                 : mozilla::Err(
                       ParseContext::ContinueStatementError::NotInALoop);
    }

    foundLoop = true;

    // Is it labeled by our label?
    stmt = stmt->enclosing();
    while (stmt && stmt->is<ParseContext::LabelStatement>()) {
      if (stmt->as<ParseContext::LabelStatement>().label() == label) {
        return Ok();
      }

      stmt = stmt->enclosing();
    }
  }
}

}  // namespace frontend
}  // namespace js

#endif  // frontend_ParseContext_inl_h
