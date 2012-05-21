/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptObjectOwner_h__
#define nsIScriptObjectOwner_h__

#include "nsISupports.h"
#include "nsIScriptContext.h"
#include "nsAString.h"

template<class> class nsScriptObjectHolder;

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
{ 0xc8f35f71, 0x07d1, 0x4ff3, \
  { 0xa3, 0x2f, 0x65, 0xcb, 0x35, 0x64, 0xac, 0xe0 } }

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
                                       nsScriptObjectHolder<JSObject>& aHandler) = 0;

  /**
   * Retrieve an already-compiled event handler that can be bound to a
   * target object using a script context.
   *
   * @param aName the name of the event handler to retrieve
   * @param aHandler the holder for the compiled event handler.
   */
  virtual nsresult GetCompiledEventHandler(nsIAtom *aName,
                                           nsScriptObjectHolder<JSObject>& aHandler) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptEventHandlerOwner,
                              NS_ISCRIPTEVENTHANDLEROWNER_IID)

#endif // nsIScriptObjectOwner_h__
