/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80: */
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
 * The Original Code is mozilla.com code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@mozilla.com> (original author)
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

#ifndef __nsPIDOMStorage_h_
#define __nsPIDOMStorage_h_

#include "nsISupports.h"
#include "nsTArray.h"

class nsIDOMStorageObsolete;
class nsIURI;
class nsIPrincipal;

// {BAFFCEB1-FD40-4ea9-8378-3509DD79204A}
#define NS_PIDOMSTORAGE_IID                                 \
  { 0xbaffceb1, 0xfd40, 0x4ea9,  \
    { 0x83, 0x78, 0x35, 0x9, 0xdd, 0x79, 0x20, 0x4a } }

class nsPIDOMStorage : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMSTORAGE_IID)

  typedef enum {
    Unknown = 0,
    GlobalStorage = 1,
    LocalStorage = 2,
    SessionStorage = 3
  } nsDOMStorageType;

  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI) = 0;
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI) = 0;
  virtual nsresult InitAsGlobalStorage(const nsACString &aDomainDemanded) = 0;

  virtual already_AddRefed<nsIDOMStorage> Clone() = 0;
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI) = 0;
  virtual bool IsForkOf(nsIDOMStorage* aThat) = 0;

  virtual nsTArray<nsString> *GetKeys() = 0;

  virtual nsIPrincipal* Principal() = 0;
  virtual bool CanAccess(nsIPrincipal *aPrincipal) = 0;

  virtual nsDOMStorageType StorageType() = 0;

  virtual void BroadcastChangeNotification(const nsSubstring &aKey,
                                           const nsSubstring &aOldValue,
                                           const nsSubstring &aNewValue) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIDOMStorage, NS_PIDOMSTORAGE_IID)

#endif // __nsPIDOMStorage_h_
