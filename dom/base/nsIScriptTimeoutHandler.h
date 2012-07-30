/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIScriptTimeoutHandler_h___
#define nsIScriptTimeoutHandler_h___

class nsIArray;

#define NS_ISCRIPTTIMEOUTHANDLER_IID \
{ 0xcaf520a5, 0x8078, 0x4cba, \
  { 0x8a, 0xb9, 0xb6, 0x8a, 0x12, 0x43, 0x4f, 0x05 } }

/**
 * Abstraction of the script objects etc required to do timeouts in a
 * language agnostic way.
 */

class nsIScriptTimeoutHandler : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTTIMEOUTHANDLER_IID)

  // Get a script object for the language suitable for passing back to
  // the language's context as an event handler.  If this returns nullptr,
  // GetHandlerText() will be called to get the string.
  virtual JSObject *GetScriptObject() = 0;

  // Get the handler text of not a compiled object.
  virtual const PRUnichar *GetHandlerText() = 0;

  // Get the location of the script.
  // Note: The memory pointed to by aFileName is owned by the
  // nsIScriptTimeoutHandler and should not be freed by the caller.
  virtual void GetLocation(const char **aFileName, PRUint32 *aLineNo) = 0;

  // If a script object, get the argv suitable for passing back to the
  // script context.
  virtual nsIArray *GetArgv() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptTimeoutHandler,
                              NS_ISCRIPTTIMEOUTHANDLER_IID)

#endif // nsIScriptTimeoutHandler_h___
