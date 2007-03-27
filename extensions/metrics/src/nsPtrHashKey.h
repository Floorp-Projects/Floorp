/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// This class defines a templatized hash key type for holding raw pointers.
// Use it like this:
//   nsDataHashtable< nsPtrHashKey<SomeClass>, SomeValueType > mTable;
//
// This is identical to nsVoidPtrHashKey with void* replaced by T*.

#ifndef nsPtrHashKey_h_
#define nsPtrHashKey_h_

#include "pldhash.h"
#include "nscore.h"

template<class T>
class nsPtrHashKey : public PLDHashEntryHdr
{
 public:
  typedef const T *KeyType;
  typedef const T *KeyTypePointer;

  nsPtrHashKey(const T *key) : mKey(key) {}
  nsPtrHashKey(const nsPtrHashKey<T> &toCopy) : mKey(toCopy.mKey) {}
  ~nsPtrHashKey() {}

  KeyType GetKey() const { return mKey; }

  PRBool KeyEquals(KeyTypePointer key) const { return key == mKey; }

  static KeyTypePointer KeyToPointer(KeyType key) { return key; }
  static PLDHashNumber HashKey(KeyTypePointer key)
  {
    return NS_PTR_TO_INT32(key) >> 2;
  }
  enum { ALLOW_MEMMOVE = PR_TRUE };

 private:
  const T *mKey;
};

#endif  // nsPtrHashKey_h_
