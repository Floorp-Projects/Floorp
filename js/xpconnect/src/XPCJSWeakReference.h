/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcjsweakreference_h___
#define xpcjsweakreference_h___

#include "xpcIJSWeakReference.h"
#include "nsIWeakReference.h"
#include "mozilla/Attributes.h"

class xpcJSWeakReference MOZ_FINAL : public xpcIJSWeakReference
{
    ~xpcJSWeakReference() {}

public:
    xpcJSWeakReference();
    nsresult Init(JSContext* cx, const JS::Value& object);

    NS_DECL_ISUPPORTS
    NS_DECL_XPCIJSWEAKREFERENCE

private:
    nsCOMPtr<nsIWeakReference> mReferent;
};

#endif // xpcjsweakreference_h___
