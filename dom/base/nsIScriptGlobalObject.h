/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptGlobalObject_h__
#define nsIScriptGlobalObject_h__

#include "nsISupports.h"
#include "nsEvent.h"
#include "nsIProgrammingLanguage.h"

class nsIScriptContext;
class nsIDOMEvent;
class nsIScriptGlobalObjectOwner;
class nsIArray;
class nsScriptErrorEvent;
class nsIScriptGlobalObject;
struct JSObject; // until we finally remove GetGlobalJSObject...

// Some helpers for working with integer "script type IDs", and specifically
// for working with arrays of such objects. For example, it is common for
// implementations supporting multiple script languages to keep each
// language's nsIScriptContext in an array indexed by the language ID.

// Implementation note: We always ignore nsIProgrammingLanguage::UNKNOWN and
// nsIProgrammingLanguage::CPLUSPLUS - this gives javascript slot 0.  An
// attempted micro-optimization tried to avoid us going all the way to 
// nsIProgrammingLanguage::MAX; however:
// * Someone is reportedly working on a PHP impl - that has value 9
// * nsGenericElement therefore allows 4 bits for the value.
// So there is no good reason for us to be more restrictive again...

#define NS_STID_FIRST nsIProgrammingLanguage::JAVASCRIPT
// like nsGenericElement, only 4 bits worth is valid...
#define NS_STID_LAST (nsIProgrammingLanguage::MAX > 0x000FU ? \
                      0x000FU : nsIProgrammingLanguage::MAX)

// Use to declare the array size
#define NS_STID_ARRAY_UBOUND (NS_STID_LAST-NS_STID_FIRST+1)

// Is a language ID valid?
#define NS_STID_VALID(langID) (langID >= NS_STID_FIRST && langID <= NS_STID_LAST)

// Return an index for a given ID.
#define NS_STID_INDEX(langID) (langID-NS_STID_FIRST)

// Create a 'for' loop iterating over all possible language IDs (*not* indexes)
#define NS_STID_FOR_ID(varName) \
          for (varName=NS_STID_FIRST;varName<=NS_STID_LAST;varName++)

// Create a 'for' loop iterating over all indexes (when you don't need to know
// what language it is)
#define NS_STID_FOR_INDEX(varName) \
          for (varName=0;varName<=NS_STID_INDEX(NS_STID_LAST);varName++)

// A helper function for nsIScriptGlobalObject implementations to use
// when handling a script error.  Generally called by the global when a context
// notifies it of an error via nsIScriptGlobalObject::HandleScriptError.
// Returns true if HandleDOMEvent was actually called, in which case
// aStatus will be filled in with the status.
bool
NS_HandleScriptError(nsIScriptGlobalObject *aScriptGlobal,
                     nsScriptErrorEvent *aErrorEvent,
                     nsEventStatus *aStatus);


#define NS_ISCRIPTGLOBALOBJECT_IID \
{ 0x92569431, 0x6e6e, 0x408a, \
  { 0xa8, 0x8c, 0x45, 0x28, 0x5c, 0x1c, 0x85, 0x73 } }

/**
 * The global object which keeps a script context for each supported script
 * language. This often used to store per-window global state.
 */

class nsIScriptGlobalObject : public nsISupports
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
  
  virtual JSObject* GetGlobalJSObject() = 0;

  nsIScriptContext* GetContext() {
    return GetScriptContext();
  }

  /**
   * Called when the global script for a language is finalized, typically as
   * part of its GC process.  By the time this call is made, the
   * nsIScriptContext for the language has probably already been removed.
   * After this call, the passed object is dead - which should generally be the
   * same object the global is using for a global for that language.
   */
  virtual void OnFinalize(JSObject* aObject) = 0;

  /**
   * Called to enable/disable scripts.
   */
  virtual void SetScriptsEnabled(bool aEnabled, bool aFireTimeouts) = 0;

  /**
   * Handle a script error.  Generally called by a script context.
   */
  virtual nsresult HandleScriptError(nsScriptErrorEvent *aErrorEvent,
                                     nsEventStatus *aEventStatus) {
    NS_ENSURE_STATE(NS_HandleScriptError(this, aErrorEvent, aEventStatus));
    return NS_OK;
  }

  virtual bool IsBlackForCC() { return false; }
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptGlobalObject,
                              NS_ISCRIPTGLOBALOBJECT_IID)

#endif
