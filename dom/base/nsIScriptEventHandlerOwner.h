/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptEventHandlerOwner_h__
#define nsIScriptEventHandlerOwner_h__

#include "nsISupports.h"
#include "nsIScriptContext.h"
#include "nsAString.h"

template<class> class nsScriptObjectHolder;

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

#endif // nsIScriptEventHandlerOwner_h__
