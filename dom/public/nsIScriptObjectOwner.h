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

#ifndef nsIScriptObjectOwner_h__
#define nsIScriptObjectOwner_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsIScriptContext.h"
#include "nsAReadableString.h"

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
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTOBJECTOWNER_IID)
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

class nsIAtom;

#define NS_ISCRIPTEVENTHANDLEROWNER_IID \
{ /* 2ad54ae0-a839-11d3-ba97-00104ba02d3d */ \
0x2ad54ae0, 0xa839, 0x11d3, \
  {0xba, 0x97, 0x00, 0x10, 0x4b, 0xa0, 0x2d, 0x3d} }

/**
 * Associate a compiled event handler with its target object, which owns it
 * This is an adjunct to nsIScriptObjectOwner that nsIEventListenerManager's
 * implementation queries for, in order to avoid recompiling a recurrent or
 * prototype-inherited event handler.
 */
class nsIScriptEventHandlerOwner : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTEVENTHANDLEROWNER_IID)

  /**
   * Compile the specified event handler, and bind it to aTarget using
   * aContext.
   *
   * @param aContext the context to use when creating event handler
   * @param aTarget the object to which to bind the event handler
   * @param aName the name of the handler
   * @param aBody the handler script body
   * @param aHandler the compiled, bound handler object
   */
  NS_IMETHOD CompileEventHandler(nsIScriptContext* aContext,
                                 void* aTarget,
                                 nsIAtom *aName,
                                 const nsAReadableString& aBody,
                                 void** aHandler) = 0;

  /**
   * Retrieve an already-compiled event handler that can be bound to a
   * target object using a script context.
   *
   * @param aName the name of the event handler to retrieve
   * @param aHandler the compiled event handler
   */
  NS_IMETHOD GetCompiledEventHandler(nsIAtom *aName, void** aHandler) = 0;
};


#define NS_ISCRIPTOBJECTPRINCIPAL_IID \
{ 0x98485f80, 0x9615, 0x11d2,  \
{ 0xbd, 0x92, 0x00, 0x80, 0x5f, 0x8a, 0xe3, 0xf4} }

/**
 * JS Object Principal information.
 */
class nsIScriptObjectPrincipal : public nsISupports {
public:

NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISCRIPTOBJECTPRINCIPAL_IID)

  NS_IMETHOD       GetPrincipal(nsIPrincipal **aPrincipal) = 0;
};

#endif // nsIScriptObjectOwner_h__
