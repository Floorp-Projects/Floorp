/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMimeTypeArray_h___
#define nsMimeTypeArray_h___

#include "nsIDOMMimeTypeArray.h"
#include "nsIDOMMimeType.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"

class nsIDOMNavigator;

// NB: Due to weak references, nsNavigator has intimate knowledge of our
// members.
class nsMimeTypeArray : public nsIDOMMimeTypeArray
{
public:
  nsMimeTypeArray(nsIDOMNavigator* navigator);
  virtual ~nsMimeTypeArray();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMIMETYPEARRAY

  void Refresh();

  nsIDOMMimeType* GetItemAt(PRUint32 aIndex, nsresult* aResult);
  nsIDOMMimeType* GetNamedItem(const nsAString& aName, nsresult* aResult);

  static nsMimeTypeArray* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMMimeTypeArray> array_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMMimeTypeArray pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(array_qi == static_cast<nsIDOMMimeTypeArray*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsMimeTypeArray*>(aSupports);
  }

  void Invalidate()
  {
    // NB: This will cause GetMimeTypes to fail from now on.
    mNavigator = nsnull;
    Clear();
  }

private:
  nsresult GetMimeTypes();
  void     Clear();

protected:
  nsIDOMNavigator* mNavigator;
  // Number of mimetypes handled by plugins.
  PRUint32 mPluginMimeTypeCount;
  // mMimeTypeArray contains all mimetypes handled by plugins
  // (mPluginMimeTypeCount) and any mimetypes that we handle internally and
  // have been looked up before. The number of items in mMimeTypeArray should
  // thus always be equal to or higher than mPluginMimeTypeCount.
  nsCOMArray<nsIDOMMimeType> mMimeTypeArray;
  bool mInited;
};

class nsMimeType : public nsIDOMMimeType
{
public:
  nsMimeType(nsIDOMPlugin* aPlugin, nsIDOMMimeType* aMimeType);
  virtual ~nsMimeType();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMIMETYPE

  void DetachPlugin() { mPlugin = nsnull; }

protected:
  nsIDOMPlugin* mPlugin;
  nsCOMPtr<nsIDOMMimeType> mMimeType;
};

class nsHelperMimeType : public nsIDOMMimeType
{
public:
  nsHelperMimeType(const nsAString& aType)
    : mType(aType)
  {
  }

  virtual ~nsHelperMimeType()
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMIMETYPE 
  
private:
  nsString mType;
};

#endif /* nsMimeTypeArray_h___ */
