/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptExternalNameSet_h__
#define nsIScriptExternalNameSet_h__

#include "nsISupports.h"

#define NS_ISCRIPTEXTERNALNAMESET_IID \
  {0xa6cf90da, 0x15b3, 0x11d2,        \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIScriptContext;

/**
 * This interface represents a set of names or symbols that should be
 * added to the "global" namespace of a script context. A name set is
 * registered with the name set registry (see
 * <code>nsIScriptNameSetRegistry</code>) at component registration
 * time using the category manager.
 */

class nsIScriptExternalNameSet : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTEXTERNALNAMESET_IID)

  /**
   * Called to tell the name set to do any initialization it needs to
   */
  NS_IMETHOD InitializeNameSet(nsIScriptContext* aScriptContext) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptExternalNameSet,
                              NS_ISCRIPTEXTERNALNAMESET_IID)

#endif /* nsIScriptExternalNameSet_h__ */
