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

#include "nsIScriptContext.h"
#include "nsIDOMScriptObjectFactory.h"

// A thin class used to help with script object memory management.  No virtual
// functions and a fully inline implementation should keep the cost down.
// [Note that a fully inline implementation is necessary for use by other
// languages, which do not link against the layout component module]
class nsScriptObjectHolder {
public:
  // A constructor that will cause a reference to |ctx| to be stored in
  // the object.  Only use for short-lived object holders.
  nsScriptObjectHolder(nsIScriptContext *ctx, void *aObject = nsnull) :
      mObject(aObject), mContext(ctx) {
    NS_ASSERTION(ctx, "Must provide a valid context");
  }

  // copy constructor
  nsScriptObjectHolder(const nsScriptObjectHolder& other) :
      mObject(other.mObject),
      mContext(other.mContext)
  {
    // New hold the script object and new reference on the script context.
    if (mObject)
      mContext->HoldScriptObject(mObject);
  }

  ~nsScriptObjectHolder() {
    if (mObject)
      mContext->DropScriptObject(mObject);
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

  // Drop the script object - but *not* the nsIScriptContext.
  nsresult drop() {
    nsresult rv = NS_OK;
    if (mObject) {
      rv = mContext->DropScriptObject(mObject);
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
      rv = mContext->HoldScriptObject(object);
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
    return mContext->GetScriptTypeID();
  }
protected:
  void *mObject;
  nsCOMPtr<nsIScriptContext> mContext;
};

#endif // nsDOMScriptObjectHolder_h__
