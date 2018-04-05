/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMClassInfo_h___
#define nsDOMClassInfo_h___

#include "mozilla/Attributes.h"
#include "nsIXPCScriptable.h"
#include "nsIScriptGlobalObject.h"
#include "js/Class.h"
#include "js/Id.h"
#include "nsIXPConnect.h"

class nsDOMClassInfo
{
public:
  static nsresult Init();
  static void ShutDown();

protected:
  static bool sIsInitialized;
};

#endif /* nsDOMClassInfo_h___ */
