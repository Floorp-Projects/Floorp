/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIScriptObjectOwner_h__
#define nsIScriptObjectOwner_h__

#include "nsISupports.h"
#include "nsIScriptContext.h"
#include "nsAString.h"

class nsScriptObjectHolder;

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
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTOBJECTOWNER_IID)

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

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptObjectOwner,
                              NS_ISCRIPTOBJECTOWNER_IID)

class nsIAtom;

#define NS_ISCRIPTEVENTHANDLEROWNER_IID \
{ 0x1e2be5d2, 0x381a, 0x46dc, \
 { 0xae, 0x97, 0xa5, 0x5f, 0x45, 0xfd, 0x36, 0x63 } }

/**
 * Associate a compiled event handler with its target object, which owns it
 * This is an adjunct to nsIScriptObjectOwner that nsEventListenerManager's
 * implementation queries for, in order to avoid recompiling a recurrent or
 * prototype-inherited event handler.
 */
class nsIScriptEventHandlerOwner : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTEVENTHANDLEROWNER_IID)

  /**
   * Compile the specified event handler.  This does NOT bind it to
   * anything.  That's the caller's responsibility.
   *
   * @param aContext the context to use when creating event handler
   * @param aName the name of the handler
   * @param aBody the handler script body
   * @param aURL the URL or filename for error messages
   * @param aLineNo the starting line number of the script for error messages
   * @param aHandler the holder for the compiled handler object
   */
  virtual nsresult CompileEventHandler(nsIScriptContext* aContext,
                                       nsIAtom *aName,
                                       const nsAString& aBody,
                                       const char* aURL,
                                       PRUint32 aLineNo,
                                       nsScriptObjectHolder &aHandler) = 0;

  /**
   * Retrieve an already-compiled event handler that can be bound to a
   * target object using a script context.
   *
   * @param aName the name of the event handler to retrieve
   * @param aHandler the holder for the compiled event handler.
   */
  virtual nsresult GetCompiledEventHandler(nsIAtom *aName,
                                           nsScriptObjectHolder &aHandler) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptEventHandlerOwner,
                              NS_ISCRIPTEVENTHANDLEROWNER_IID)

#endif // nsIScriptObjectOwner_h__
