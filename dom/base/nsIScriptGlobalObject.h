/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptGlobalObject_h__
#define nsIScriptGlobalObject_h__

#include "nsISupports.h"
#include "nsIGlobalObject.h"
#include "js/TypeDecls.h"
#include "mozilla/EventForwards.h"

class nsIScriptContext;
class nsIScriptGlobalObject;

namespace mozilla {
namespace dom {
struct ErrorEventInit;
} // namespace dom
} // namespace mozilla

// A helper function for nsIScriptGlobalObject implementations to use
// when handling a script error.  Generally called by the global when a context
// notifies it of an error via nsIScriptGlobalObject::HandleScriptError.
// Returns true if HandleDOMEvent was actually called, in which case
// aStatus will be filled in with the status.
bool
NS_HandleScriptError(nsIScriptGlobalObject *aScriptGlobal,
                     const mozilla::dom::ErrorEventInit &aErrorEvent,
                     nsEventStatus *aStatus);


// Must be kept in sync with xpcom/rust/xpcom/src/interfaces/nonidl.rs
#define NS_ISCRIPTGLOBALOBJECT_IID \
{ 0x876f83bd, 0x6314, 0x460a, \
  { 0xa0, 0x45, 0x1c, 0x8f, 0x46, 0x2f, 0xb8, 0xe1 } }

/**
 * The global object which keeps a script context for each supported script
 * language. This often used to store per-window global state.
 * This is a heavyweight interface implemented only by DOM globals, and
 * it might go away some time in the future.
 */

class nsIScriptGlobalObject : public nsIGlobalObject
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTGLOBALOBJECT_IID)

  /**
   * Ensure that the script global object is initialized for working with the
   * specified script language ID.  This will set up the nsIScriptContext
   * and 'script global' for that language, allowing these to be fetched
   * and manipulated.
   * @return NS_OK if successful; error conditions include that the language
   * has not been registered, as well as 'normal' errors, such as
   * out-of-memory
   */
  virtual nsresult EnsureScriptEnvironment() = 0;
  /**
   * Get a script context (WITHOUT added reference) for the specified language.
   */
  virtual nsIScriptContext *GetScriptContext() = 0;

  nsIScriptContext* GetContext() {
    return GetScriptContext();
  }

  /**
   * Handle a script error.  Generally called by a script context.
   */
  bool HandleScriptError(const mozilla::dom::ErrorEventInit &aErrorEventInit,
                         nsEventStatus *aEventStatus) {
    return NS_HandleScriptError(this, aErrorEventInit, aEventStatus);
  }

  virtual bool IsBlackForCC(bool aTracingNeeded = true) { return false; }

protected:
  virtual ~nsIScriptGlobalObject() {}
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptGlobalObject,
                              NS_ISCRIPTGLOBALOBJECT_IID)

#endif
