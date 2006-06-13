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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Mark Hammond <mhammond@skippinet.com.au>
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

#ifndef nsDOMScriptObjectHolder_h__
#define nsDOMScriptObjectHolder_h__

#include "nsIScriptRuntime.h"
#include "nsIDOMScriptObjectFactory.h"

class nsIScriptContext;

// Drop a reference to a script object when all you have is the script
// language ID.
inline nsresult NS_DropScriptObject(PRUint32 aLangID, void *aThing)
{
  nsresult rv;
  nsCOMPtr<nsIScriptRuntime> scriptRT;
  NS_DEFINE_CID(cid, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

  nsCOMPtr<nsIDOMScriptObjectFactory> factory = do_GetService(cid, &rv);
  if (NS_SUCCEEDED(rv))
    rv = factory->GetScriptRuntimeByID(aLangID, getter_AddRefs(scriptRT));
  if (NS_SUCCEEDED(rv))
    rv = scriptRT->DropScriptObject(aThing);
  if (NS_FAILED(rv))
    NS_ERROR("Failed to drop the script object");
  return rv;
}

// Hold a reference to a script object when all you have is the script
// language ID, and you need to take a copy of the void script object.
// Must be matched by an NS_DropScriptObject
inline nsresult NS_HoldScriptObject(PRUint32 aLangID, void *aThing)
{
  nsresult rv;
  nsCOMPtr<nsIScriptRuntime> scriptRT;
  NS_DEFINE_CID(cid, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

  nsCOMPtr<nsIDOMScriptObjectFactory> factory = do_GetService(cid, &rv);
  if (NS_SUCCEEDED(rv))
    rv = factory->GetScriptRuntimeByID(aLangID, getter_AddRefs(scriptRT));
  if (NS_SUCCEEDED(rv))
      rv = scriptRT->HoldScriptObject(aThing);
  if (NS_FAILED(rv))
      NS_ERROR("Failed to hold the script object");
  return rv;
}

// A thin class used to help with script object memory management.  No virtual
// functions and a fully inline implementation should keep the cost down.
// [Note that a fully inline implementation is necessary for use by other
// languages, which do not link against the layout component module]
class nsScriptObjectHolder {
public:
  // A constructor that will cause a reference to |ctx| to be stored in
  // the object.  Only use for short-lived object holders.
  nsScriptObjectHolder(nsIScriptContext *ctx, void *aObject = nsnull) :
      mObject(aObject), mContextOrLangID(NS_REINTERPRET_CAST(PtrBits, ctx)) {
    NS_ASSERTION(ctx, "Must provide a valid context");
    NS_ADDREF(ctx);
  }

  // A constructor that stores only the integer language ID - freeing the
  // script object is slower in this case, but is safe for long-lived
  // object holders.
  nsScriptObjectHolder(PRUint32 aLangID, void *aObject = nsnull) :
      mObject(aObject),
      mContextOrLangID((aLangID << 1) | SOH_HAS_LANGID_BIT) {
    NS_ASSERTION(aLangID != nsIProgrammingLanguage::UNKNOWN,
                 "Please supply a valid language ID");
  }

  // copy constructor
  nsScriptObjectHolder(const nsScriptObjectHolder& other) :
      mObject(other.mObject),
      mContextOrLangID(other.mContextOrLangID)
  {
    if (mContextOrLangID & SOH_HAS_LANGID_BIT) {
      if (mObject) {
        NS_HoldScriptObject(getScriptTypeIDFromBits(), mObject);
      }
    } else {
      // New hold the script object and new reference on the script context.
      nsIScriptContext *ctx = NS_REINTERPRET_CAST(nsIScriptContext *,
                                                  mContextOrLangID);
      NS_ASSERTION(ctx, "Lost context pointer?");
      NS_ADDREF(ctx);
      if (mObject)
        ctx->HoldScriptObject(mObject);
    }
  }

  ~nsScriptObjectHolder() {
    // If we have a language ID, then we only need to NS_DropScriptObject if
    // we are holding a script object.
    // If we have a script context, we must always release our reference to it,
    // even if we are not holding a script object.
    if (mContextOrLangID & SOH_HAS_LANGID_BIT) {
      if (mObject) {
        NS_DropScriptObject(getScriptTypeIDFromBits(), mObject);
      }
    } else {
      nsIScriptContext *ctx = NS_REINTERPRET_CAST(nsIScriptContext *,
                                                  mContextOrLangID);
      NS_ASSERTION(ctx, "Lost context pointer?");
      if (mObject) {
        ctx->DropScriptObject(mObject);
      }
      NS_IF_RELEASE(ctx);
    }
  }

  // misc operators
  nsScriptObjectHolder &operator=(const nsScriptObjectHolder &other) {
    set(other);
    return *this;
  }
  PRBool operator!() const {
    return !mObject;
  }
  operator void *() const {
    return mObject;
  }

  // Drop the script object - but *not* the nsIScriptContext nor the language
  // ID.
  nsresult drop() {
    nsresult rv = NS_OK;
    if (mObject) {
      if (mContextOrLangID & SOH_HAS_LANGID_BIT) {
        rv = NS_DropScriptObject(getScriptTypeIDFromBits(), mObject);
      } else {
        nsIScriptContext *ctx = NS_REINTERPRET_CAST(nsIScriptContext *,
                                                    mContextOrLangID);
        NS_ASSERTION(ctx, "Lost ctx pointer!");
        rv = ctx->DropScriptObject(mObject);
      }
      mObject = nsnull;
    }
    return rv;
  }
  nsresult set(void *object) {
    NS_ASSERTION(getScriptTypeID() != nsIProgrammingLanguage::UNKNOWN,
                 "Must know the language!");
    nsresult rv = drop();
    if (NS_FAILED(rv))
      return rv;
    if (object) {
      if (mContextOrLangID & SOH_HAS_LANGID_BIT) {
        rv = NS_HoldScriptObject(getScriptTypeIDFromBits(), object);
      } else {
        nsIScriptContext *ctx = NS_REINTERPRET_CAST(nsIScriptContext *,
                                                    mContextOrLangID);
        rv = ctx->HoldScriptObject(object);
      }
      // don't store the pointer if we failed to lock it.
      if (NS_SUCCEEDED(rv)) {
        mObject = object;
      }
    }
    return rv;
  }
  nsresult set(const nsScriptObjectHolder &other) {
    NS_ASSERTION(getScriptTypeID() == other.getScriptTypeID(),
                 "Must have identical languages!");
    nsresult rv = drop();
    if (NS_FAILED(rv))
      return rv;
    return set(other.mObject);
  }
  // Get the language ID.
  PRUint32 getScriptTypeID() const {
    if (mContextOrLangID & SOH_HAS_LANGID_BIT) {
      return getScriptTypeIDFromBits();
    }
    nsIScriptContext *ctx = NS_REINTERPRET_CAST(nsIScriptContext *,
                                                mContextOrLangID);
    NS_ASSERTION(ctx, "How did I lose my pointer?");
    return ctx->GetScriptTypeID();
  }
protected:
  PRUint32 getScriptTypeIDFromBits() const {
    NS_ASSERTION(mContextOrLangID & SOH_HAS_LANGID_BIT, "Not in the bits!");
    return (mContextOrLangID & ~SOH_HAS_LANGID_BIT) >> 1;
  }
  void *mObject;
  // We store either an nsIScriptContext* if this bit is clear,
  // else the language ID (specifically, ((lang_id << 1) | SOH_HAS_LANGID_BIT)
  // when set.
  typedef PRWord PtrBits;
  enum { SOH_HAS_LANGID_BIT = 0x1 };

  PtrBits mContextOrLangID;
};

#endif // nsDOMScriptObjectHolder_h__
