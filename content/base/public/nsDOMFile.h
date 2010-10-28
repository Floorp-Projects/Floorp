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
 * The Original Code is mozila.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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

#ifndef nsDOMFile_h__
#define nsDOMFile_h__

#include "nsICharsetDetectionObserver.h"
#include "nsIDOMFile.h"
#include "nsIDOMFileList.h"
#include "nsIDOMFileError.h"
#include "nsIInputStream.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "mozilla/AutoRestore.h"
#include "nsString.h"
#include "nsIXMLHttpRequest.h"
#include "prmem.h"
#include "nsAutoPtr.h"

class nsIFile;
class nsIInputStream;
class nsIClassInfo;

class nsDOMFile : public nsIDOMFile,
                  public nsIXHRSendable,
                  public nsICharsetDetectionObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBLOB
  NS_DECL_NSIDOMFILE
  NS_DECL_NSIXHRSENDABLE

  nsDOMFile(nsIFile *aFile, const nsAString& aContentType)
    : mFile(aFile),
      mContentType(aContentType),
      mIsFullFile(true)
  {}

  nsDOMFile(nsIFile *aFile)
    : mFile(aFile),
      mIsFullFile(true)
  {}

  nsDOMFile(const nsDOMFile* aOther, PRUint64 aStart, PRUint64 aLength,
            const nsAString& aContentType)
    : mFile(aOther->mFile),
      mStart(aOther->mIsFullFile ? aStart :
                                   (aOther->mStart + aStart)),
      mLength(aLength),
      mContentType(aContentType),
      mIsFullFile(false)
  {
    NS_ASSERTION(mFile, "must have file");
    // Ensure non-null mContentType
    mContentType.SetIsVoid(PR_FALSE);
  }

  virtual ~nsDOMFile() {}

  // from nsICharsetDetectionObserver
  NS_IMETHOD Notify(const char *aCharset, nsDetectionConfident aConf);

protected:
  nsCOMPtr<nsIFile> mFile;

  // start and length in 
  PRUint64 mStart;
  PRUint64 mLength;

  nsString mContentType;
  
  bool mIsFullFile;

  // Used during charset detection
  nsCString mCharset;
  nsresult GuessCharset(nsIInputStream *aStream,
                        nsACString &aCharset);
  nsresult ConvertStream(nsIInputStream *aStream, const char *aCharset,
                         nsAString &aResult);
};

class nsDOMMemoryFile : public nsDOMFile
{
public:
  nsDOMMemoryFile(void *aMemoryBuffer,
                  PRUint64 aLength,
                  const nsAString& aName,
                  const nsAString& aContentType)
    : nsDOMFile(nsnull, aContentType),
      mDataOwner(new DataOwner(aMemoryBuffer)),
      mName(aName)
  {
    mStart = 0;
    mLength = aLength;
  }

  nsDOMMemoryFile(const nsDOMMemoryFile* aOther, PRUint64 aStart,
                  PRUint64 aLength, const nsAString& aContentType)
    : nsDOMFile(nsnull, aContentType),
      mDataOwner(aOther->mDataOwner)
  {
    NS_ASSERTION(mDataOwner && mDataOwner->mData, "must have data");

    mIsFullFile = false;
    mStart = aOther->mStart + aStart;
    mLength = aLength;

    // Ensure non-null mContentType
    mContentType.SetIsVoid(PR_FALSE);
  }

  NS_IMETHOD GetName(nsAString&);
  NS_IMETHOD GetSize(PRUint64*);
  NS_IMETHOD GetInternalStream(nsIInputStream**);
  NS_IMETHOD GetMozFullPathInternal(nsAString&);
  NS_IMETHOD Slice(PRUint64 aStart, PRUint64 aLength,
                   const nsAString& aContentType, nsIDOMBlob **aBlob);

protected:
  class DataOwner {
  public:
    NS_INLINE_DECL_REFCOUNTING(DataOwner)
    DataOwner(void* aMemoryBuffer)
      : mData(aMemoryBuffer)
    {
    }
    ~DataOwner() {
      PR_Free(mData);
    }
    void* mData;
  };

  // Used when backed by a memory store
  nsRefPtr<DataOwner> mDataOwner;

  nsString mName;
};

class nsDOMFileList : public nsIDOMFileList
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFILELIST

  PRBool Append(nsIDOMFile *aFile) { return mFiles.AppendObject(aFile); }

  PRBool Remove(PRUint32 aIndex) { return mFiles.RemoveObjectAt(aIndex); }
  void Clear() { return mFiles.Clear(); }

  nsIDOMFile* GetItemAt(PRUint32 aIndex)
  {
    return mFiles.SafeObjectAt(aIndex);
  }

  static nsDOMFileList* FromSupports(nsISupports* aSupports)
  {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMFileList> list_qi = do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMFileList pointer as the nsISupports
      // pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(list_qi == static_cast<nsIDOMFileList*>(aSupports),
                   "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMFileList*>(aSupports);
  }

private:
  nsCOMArray<nsIDOMFile> mFiles;
};

class nsDOMFileError : public nsIDOMFileError
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMFILEERROR

  nsDOMFileError(PRUint16 aErrorCode) : mCode(aErrorCode) {}

private:
  PRUint16 mCode;
};

class NS_STACK_CLASS nsDOMFileInternalUrlHolder {
public:
  nsDOMFileInternalUrlHolder(nsIDOMBlob* aFile, nsIPrincipal* aPrincipal
                             MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM);
  ~nsDOMFileInternalUrlHolder();
  nsAutoString mUrl;
private:
  MOZILLA_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#endif
