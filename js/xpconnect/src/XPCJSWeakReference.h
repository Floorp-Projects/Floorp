/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcjsweakreference_h___
#define xpcjsweakreference_h___

#include "xpcIJSWeakReference.h"
#include "nsIWeakReferenceUtils.h"
#include "mozilla/Attributes.h"

class xpcJSWeakReference final : public xpcIJSWeakReference {
  ~xpcJSWeakReference() = default;

 public:
  xpcJSWeakReference();
  nsresult Init(JSContext* cx, const JS::Value& object);

  NS_DECL_ISUPPORTS
  NS_DECL_XPCIJSWEAKREFERENCE

 private:
  nsWeakPtr mReferent;
};

#endif  // xpcjsweakreference_h___
