/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIScriptObject_h__
#define nsIScriptObject_h__

#include "nsISupports.h"
#include "jsapi.h"

#define NS_ISCRIPTOBJECT_IID \
{ /* 8f6bca7c-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7c, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} } 

/**
 * A java script specific interface. 
 * <P>
 * It's implemented by an object that has to execute specific java script
 * behavior like the array semantic.
 * <P>
 * It is used by the script runtime to collect information about an object
 */
class nsIScriptObject : public nsISupports {
public:
  virtual PRBool    AddProperty(JSContext *aContext, jsval aID, jsval *aVp) = 0;
  virtual PRBool    DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp) = 0;
  virtual PRBool    GetProperty(JSContext *aContext, jsval aID, jsval *aVp) = 0;
  virtual PRBool    SetProperty(JSContext *aContext, jsval aID, jsval *aVp) = 0;
  virtual PRBool    EnumerateProperty(JSContext *aContext) = 0;
  virtual PRBool    Resolve(JSContext *aContext, jsval aID) = 0;
  virtual PRBool    Convert(JSContext *aContext, jsval aID) = 0;
  virtual void      Finalize(JSContext *aContext) = 0;
};

#endif // nsIScriptObject_h__
