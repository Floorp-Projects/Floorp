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

#ifndef nsIScriptObjectOwner_h__
#define nsIScriptObjectOwner_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIScriptContext.h"

class nsIScriptContext;

#define NS_ISCRIPTOBJECTOWNER_IID \
{ /* 8f6bca7e-ce42-11d1-b724-00600891d8c9 */ \
0x8f6bca7e, 0xce42, 0x11d1, \
  {0xb7, 0x24, 0x00, 0x60, 0x08, 0x91, 0xd8, 0xc9} } \

/**
 * Creates a link between the script object and its native implementation
 *<P>
 * Every object that wants to be exposed in a script environment should
 * implement this interface. This interface should guarantee that the same
 * script object is returned in the context of the same script.
 * <P><I>It does have a bit too much java script information now, that
 * should be removed in a short time. Ideally this interface will be
 * language neutral</I>
 */
class nsIScriptObjectOwner : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISCRIPTOBJECTOWNER_IID; return iid; }

  /**
   * Return the script object associated with this object.
   * Create a script object if not present.
   *
   * @param aContext the context the script object has to be created in
   * @param aScriptObject on return will contain the script object
   *
   * @return nsresult NS_OK if the script object is successfully returned
   *
   **/
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject) = 0;

  /**
   * Set the script object associated with this object.
   * Often used to either reset the object to null or initially
   * set it in cases where the object comes before the owner.
   *
   * @param aScriptObject the script object to set
   *
   * @return nsresult NS_OK if the script object is successfully set
   *
   **/
  NS_IMETHOD SetScriptObject(void* aScriptObject) = 0;
};

#endif // nsIScriptObjectOwner_h__
