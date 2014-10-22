/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcquickstubs_h___
#define xpcquickstubs_h___

#include "XPCForwards.h"

/* XPCQuickStubs.h - Support functions used only by Web IDL bindings, for now. */

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx, JS::HandleObject src, const nsIID &iid,
                    void **ppArg);

#endif /* xpcquickstubs_h___ */
