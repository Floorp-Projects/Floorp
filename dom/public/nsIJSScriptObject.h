/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsIJSScriptObject_h__
#define nsIJSScriptObject_h__

#include "nsISupports.h"
#include "nsIScriptObjectOwner.h"
#include "jsapi.h"

#define NS_IJSSCRIPTOBJECT_IID \
{ /* 8f6bca7c-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7c, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} } 

/**
 * A JavaScript specific interface. 
 * <P>
 * It's implemented by an object that has to execute specific java script
 * behavior like the array semantic.
 * <P>
 * It is used by the script runtime to collect information about an object
 */
class nsIJSScriptObject : public nsIScriptObjectOwner {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IJSSCRIPTOBJECT_IID; return iid; }
  virtual PRBool    AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, 
                                jsval *aVp) = 0;
  virtual PRBool    DeleteProperty(JSContext *aContext, JSObject *aObj, 
                                   jsval aID, jsval *aVp) = 0;
  virtual PRBool    GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, 
                                jsval *aVp) = 0;
  virtual PRBool    SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, 
                                jsval *aVp) = 0;
  virtual PRBool    EnumerateProperty(JSContext *aContext, JSObject *aObj) = 0;
  virtual PRBool    Resolve(JSContext *aContext, JSObject *aObj,
                            jsval aID, PRBool *aDidDefineProperty) = 0;
  virtual PRBool    Convert(JSContext *aContext, JSObject *aObj, jsval aID) = 0;
  virtual void      Finalize(JSContext *aContext, JSObject *aObj) = 0;
};

#endif // nsIJSScriptObject_h__
