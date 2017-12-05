/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsversion_h
#define jsversion_h

/*
 * JS Capability Macros.
 */
#define JS_HAS_OBJ_PROTO_PROP   1       /* has o.__proto__ etc. */
#define JS_HAS_TOSOURCE         1       /* has Object/Array toSource method */
#define JS_HAS_CATCH_GUARD      1       /* has exception handling catch guard */
#define JS_HAS_UNEVAL           1       /* has uneval() top-level function */

#ifndef NIGHTLY_BUILD
#define JS_HAS_EXPR_CLOSURES    1       /* has function (formals) listexpr */
#endif

/*
 * Feature for Object.prototype.__{define,lookup}{G,S}etter__ legacy support;
 * support likely to be made opt-in at some future time.
 */
#define JS_OLD_GETTER_SETTER_METHODS    1

#endif /* jsversion_h */
