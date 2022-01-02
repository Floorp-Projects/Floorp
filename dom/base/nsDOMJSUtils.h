/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMJSUtils_h__
#define nsDOMJSUtils_h__

#include "js/TypeDecls.h"
#include "nscore.h"

class nsIJSArgArray;

// A factory function for turning a JS::Value argv into an nsIArray
// but also supports an effecient way of extracting the original argv.
// The resulting object will take a copy of the array, and ensure each
// element is rooted.
// Optionally, aArgv may be nullptr, in which case the array is allocated and
// rooted, but all items remain nullptr.  This presumably means the caller
// will then QI us for nsIJSArgArray, and set our array elements.
nsresult NS_CreateJSArgv(JSContext* aContext, uint32_t aArgc,
                         const JS::Value* aArgv, nsIJSArgArray** aArray);

#endif  // nsDOMJSUtils_h__
