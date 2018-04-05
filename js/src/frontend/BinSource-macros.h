/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinSource_macros_h
#define frontend_BinSource_macros_h


// Evaluate an expression EXPR, checking that the result is not falsy.
//
// Throw `cx->alreadyReportedError()` if it returns 0/nullptr.
#define BINJS_TRY(EXPR) \
    do { \
        if (!EXPR) \
            return cx_->alreadyReportedError(); \
    } while(false)


// Evaluate an expression EXPR, checking that the result is not falsy.
// In case of success, assign the result to VAR.
//
// Throw `cx->alreadyReportedError()` if it returns 0/nullptr.
#define BINJS_TRY_VAR(VAR, EXPR) \
    do { \
        VAR = EXPR; \
        if (!VAR) \
            return cx_->alreadyReportedError(); \
    } while (false)

// Evaluate an expression EXPR, checking that the result is not falsy.
// In case of success, assign the result to a new variable VAR.
//
// Throw `cx->alreadyReportedError()` if it returns 0/nullptr.
#define BINJS_TRY_DECL(VAR, EXPR) \
    auto VAR = EXPR; \
    if (!VAR) \
        return cx_->alreadyReportedError();

#define BINJS_TRY_EMPL(VAR, EXPR) \
    do { \
        auto _tryEmplResult = EXPR; \
        if (!_tryEmplResult) \
            return cx_->alreadyReportedError(); \
        VAR.emplace(_tryEmplResult.unwrap()); \
    } while (false)

#define BINJS_MOZ_TRY_EMPLACE(VAR, EXPR) \
    do { \
        auto _tryEmplResult = EXPR; \
        if (_tryEmplResult.isErr()) \
            return ::mozilla::Err(_tryEmplResult.unwrapErr()); \
        VAR.emplace(_tryEmplResult.unwrap()); \
    } while (false)

// Evaluate an expression EXPR, checking that the result is a success.
// In case of success, unwrap and assign the result to a new variable VAR.
//
// In case of error, propagate the error.
#define BINJS_MOZ_TRY_DECL(VAR, EXPR) \
    auto _##VAR = EXPR; \
    if (_##VAR.isErr()) \
        return ::mozilla::Err(_##VAR.unwrapErr()); \
    auto VAR = _##VAR.unwrap();

#endif // frontend_BinSource_macros_h
