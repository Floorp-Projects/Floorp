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
 * Mozilla Foundation
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

#include "nsDOMFile.h"

#include "nsCExternalHandlerService.h"
#include "nsContentCID.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMError.h"
#include "nsICharsetAlias.h"
#include "nsICharsetDetector.h"
#include "nsICharsetConverterManager.h"
#include "nsIConverterInputStream.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIIPCSerializable.h"
#include "nsIMIMEService.h"
#include "nsIPlatformCharset.h"
#include "nsISeekableStream.h"
#include "nsIUnicharInputStream.h"
#include "nsIUnicodeDecoder.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIUUIDGenerator.h"
#include "nsFileDataProtocolHandler.h"
#include "nsStringStream.h"
#include "CheckedInt.h"
#include "nsJSUtils.h"
#include "mozilla/Preferences.h"

#include "plbase64.h"
#include "prmem.h"

using namespace mozilla;

// XXXkhuey the input stream that we pass out of a DOMFile
// can outlive the actual DOMFile object.  Thus, we must
// ensure that the buffer underlying the stream we get
// from NS_NewByteInputStream is held alive as long as the
// stream is.  We do that by passing back this class instead.
class DataOwnerAdapter : public nsIInputStream,
                         public nsISeekableStream
{
  typedef nsDOMMemoryFile::DataOwner DataOwner;
public:
  static nsresult Create(DataOwner* aDataOwner,
                         PRUint32 aStart,
                         PRUint32 aLength,
                         nsIInputStream** _retval);

  NS_DECL_ISUPPORTS

  NS_FORWARD_NSIINPUTSTREAM(mStream->)

  NS_FORWARD_NSISEEKABLESTREAM(mSeekableStream->)

private:
  DataOwnerAdapter(DataOwner* aDataOwner,
                   nsIInputStream* aStream)
    : mDataOwner(aDataOwner), mStream(aStream),
      mSeekableStream(do_QueryInterface(aStream))
  {
    NS_ASSERTION(mSeekableStream, "Somebody gave us the wrong stream!");
  }

  nsRefPtr<DataOwner> mDataOwner;
  nsCOMPtr<nsIInputStream> mStream;
  nsCOMPtr<nsISeekableStream> mSeekableStream;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(DataOwnerAdapter,
                              nsIInputStream,
                              nsISeekableStream)

nsresult DataOwnerAdapter::Create(DataOwner* aDataOwner,
                                  PRUint32 aStart,
                                  PRUint32 aLength,
                                  nsIInputStream** _retval)
{
  nsresult rv;
  NS_ASSERTION(aDataOwner, "Uh ...");

  nsCOMPtr<nsIInputStream> stream;

  rv = NS_NewByteInputStream(getter_AddRefs(stream),
                             static_cast<const char*>(aDataOwner->mData) +
                             aStart,
                             (PRInt32)aLength,
                             NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = new DataOwnerAdapter(aDataOwner, stream));

  return NS_OK;
}

// nsDOMFile implementation

DOMCI_DATA(File, nsDOMFile)
DOMCI_DATA(Blob, nsDOMFile)

NS_INTERFACE_MAP_BEGIN(nsDOMFile)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, mIsFullFile)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsIJSNativeInitializer)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(File, mIsFullFile)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_CONDITIONAL(Blob, !mIsFullFile)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMFile)
NS_IMPL_RELEASE(nsDOMFile)

static nsresult
DOMFileResult(nsresult rv)
{
  if (rv == NS_ERROR_FILE_NOT_FOUND) {
    return NS_ERROR_DOM_FILE_NOT_FOUND_ERR;
  }

  if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_FILES) {
    return NS_ERROR_DOM_FILE_NOT_READABLE_ERR;
  }

  return rv;
}

/* static */ nsresult
nsDOMFile::NewFile(nsISupports* *aNewObject)
{
  nsCOMPtr<nsISupports> file = do_QueryObject(new nsDOMFile(nsnull));
  file.forget(aNewObject);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::GetName(nsAString &aFileName)
{
  NS_ASSERTION(mIsFullFile, "Should only be called on files");
  return mFile->GetLeafName(aFileName);
}

NS_IMETHODIMP
nsDOMFile::GetMozFullPath(nsAString &aFileName)
{
  NS_ASSERTION(mIsFullFile, "Should only be called on files");
  if (nsContentUtils::IsCallerTrustedForCapability("UniversalFileRead")) {
    return GetMozFullPathInternal(aFileName);
  }
  aFileName.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::GetMozFullPathInternal(nsAString &aFilename)
{
  NS_ASSERTION(mIsFullFile, "Should only be called on files");
  return mFile->GetPath(aFilename);
}

NS_IMETHODIMP
nsDOMFile::GetSize(PRUint64 *aFileSize)
{
  if (mIsFullFile) {
    PRInt64 fileSize;
    nsresult rv = mFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);
  
    if (fileSize < 0) {
      return NS_ERROR_FAILURE;
    }
  
    *aFileSize = fileSize;
  }
  else {
    *aFileSize = mLength;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::GetType(nsAString &aType)
{
  if (mContentType.IsEmpty() && mFile && mIsFullFile) {
    nsresult rv;
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString mimeType;
    rv = mimeService->GetTypeFromFile(mFile, mimeType);
    if (NS_FAILED(rv)) {
      aType.Truncate();
      return NS_OK;
    }

    AppendUTF8toUTF16(mimeType, mContentType);
  }

  aType = mContentType;

  return NS_OK;
}

// Makes sure that aStart and aEnd is less then or equal to aSize and greater
// than 0
void
ParseSize(PRInt64 aSize, PRInt64& aStart, PRInt64& aEnd)
{
  CheckedInt64 newStartOffset = aStart;
  if (aStart < -aSize) {
    newStartOffset = 0;
  }
  else if (aStart < 0) {
    newStartOffset += aSize;
  }
  else if (aStart > aSize) {
    newStartOffset = aSize;
  }

  CheckedInt64 newEndOffset = aEnd;
  if (aEnd < -aSize) {
    newEndOffset = 0;
  }
  else if (aEnd < 0) {
    newEndOffset += aSize;
  }
  else if (aEnd > aSize) {
    newEndOffset = aSize;
  }

  if (!newStartOffset.valid() || !newEndOffset.valid() ||
      newStartOffset.value() >= newEndOffset.value()) {
    aStart = aEnd = 0;
  }
  else {
    aStart = newStartOffset.value();
    aEnd = newEndOffset.value();
  }
}

NS_IMETHODIMP
nsDOMFile::MozSlice(PRInt64 aStart, PRInt64 aEnd,
                    const nsAString& aContentType, PRUint8 optional_argc,
                    nsIDOMBlob **aBlob)
{
  *aBlob = nsnull;

  // Truncate aStart and aEnd so that we stay within this file.
  PRUint64 thisLength;
  nsresult rv = GetSize(&thisLength);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!optional_argc) {
    aEnd = (PRInt64)thisLength;
  }

  ParseSize((PRInt64)thisLength, aStart, aEnd);
  
  // Create the new file
  NS_ADDREF(*aBlob = new nsDOMFile(this, aStart, aEnd - aStart, aContentType));
  
  return NS_OK;
}

const PRUint32 sFileStreamFlags =
  nsIFileInputStream::CLOSE_ON_EOF |
  nsIFileInputStream::REOPEN_ON_REWIND |
  nsIFileInputStream::DEFER_OPEN;

NS_IMETHODIMP
nsDOMFile::GetInternalStream(nsIInputStream **aStream)
{
  return mIsFullFile ?
    NS_NewLocalFileInputStream(aStream, mFile, -1, -1, sFileStreamFlags) :
    NS_NewPartialLocalFileInputStream(aStream, mFile, mStart, mLength,
                                      -1, -1, sFileStreamFlags);
}

NS_IMETHODIMP
nsDOMFile::GetInternalUrl(nsIPrincipal* aPrincipal, nsAString& aURL)
{
  NS_ENSURE_STATE(aPrincipal);

  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);
  
  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);
    
  nsCString url = NS_LITERAL_CSTRING(FILEDATA_SCHEME ":") +
    Substring(chars + 1, chars + NSID_LENGTH - 2);

  nsFileDataProtocolHandler::AddFileDataEntry(url, this,
                                              aPrincipal);

  CopyASCIItoUTF16(url, aURL);
  
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::GetSendInfo(nsIInputStream** aBody,
                       nsACString& aContentType,
                       nsACString& aCharset)
{
  nsresult rv;

  nsCOMPtr<nsIInputStream> stream;
  rv = this->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString contentType;
  rv = this->GetType(contentType);
  NS_ENSURE_SUCCESS(rv, rv);

  CopyUTF16toUTF8(contentType, aContentType);

  aCharset.Truncate();

  stream.forget(aBody);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::Initialize(nsISupports* aOwner,
                      JSContext* aCx,
                      JSObject* aObj,
                      PRUint32 aArgc,
                      jsval* aArgv)
{
  nsresult rv;

  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR; // Real short trip
  }

  NS_ENSURE_TRUE(aArgc > 0, NS_ERROR_UNEXPECTED);

  // We expect to get a path to represent as a File object,
  // or an nsIFile
  nsCOMPtr<nsIFile> file;
  if (!JSVAL_IS_STRING(aArgv[0])) {
    // Lets see if it's an nsIFile
    if (!JSVAL_IS_OBJECT(aArgv[0])) {
      return NS_ERROR_UNEXPECTED; // We're not interested
    }

    JSObject* obj = JSVAL_TO_OBJECT(aArgv[0]);
    NS_ASSERTION(obj, "This is a bit odd");

    // Is it an nsIFile
    file = do_QueryInterface(
      nsContentUtils::XPConnect()->
        GetNativeOfWrapper(aCx, obj));
    if (!file)
      return NS_ERROR_UNEXPECTED;
  } else {
    // It's a string
    JSString* str = JS_ValueToString(aCx, aArgv[0]);
    NS_ENSURE_TRUE(str, NS_ERROR_XPC_BAD_CONVERT_JS);

    nsDependentJSString xpcomStr;
    if (!xpcomStr.init(aCx, str)) {
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewLocalFile(xpcomStr, PR_FALSE, getter_AddRefs(localFile));
    NS_ENSURE_SUCCESS(rv, rv);

    file = do_QueryInterface(localFile);
    NS_ASSERTION(file, "This should never happen");
  }

  PRBool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(exists, NS_ERROR_FILE_NOT_FOUND);

  mFile = file;
  return NS_OK;
}

// nsDOMMemoryFile Implementation
NS_IMETHODIMP
nsDOMMemoryFile::GetName(nsAString &aFileName)
{
  NS_ASSERTION(mIsFullFile, "Should only be called on files");
  aFileName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryFile::GetSize(PRUint64 *aFileSize)
{
  *aFileSize = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryFile::MozSlice(PRInt64 aStart, PRInt64 aEnd,
                          const nsAString& aContentType, PRUint8 optional_argc,
                          nsIDOMBlob **aBlob)
{
  *aBlob = nsnull;

  if (!optional_argc) {
    aEnd = (PRInt64)mLength;
  }

  // Truncate aLength and aStart so that we stay within this file.
  ParseSize((PRInt64)mLength, aStart, aEnd);

  // Create the new file
  NS_ADDREF(*aBlob = new nsDOMMemoryFile(this, aStart, aEnd - aStart,
                                         aContentType));
  
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryFile::GetInternalStream(nsIInputStream **aStream)
{
  if (mLength > PR_INT32_MAX)
    return NS_ERROR_FAILURE;

  return DataOwnerAdapter::Create(mDataOwner, mStart, mLength, aStream);
}

NS_IMETHODIMP
nsDOMMemoryFile::GetMozFullPathInternal(nsAString &aFilename)
{
  NS_ASSERTION(mIsFullFile, "Should only be called on files");
  aFilename.Truncate();
  return NS_OK;
}

// nsDOMFileList implementation

DOMCI_DATA(FileList, nsDOMFileList)

NS_INTERFACE_MAP_BEGIN(nsDOMFileList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileList)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(FileList)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMFileList)
NS_IMPL_RELEASE(nsDOMFileList)

NS_IMETHODIMP
nsDOMFileList::GetLength(PRUint32* aLength)
{
  *aLength = mFiles.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFileList::Item(PRUint32 aIndex, nsIDOMFile **aFile)
{
  NS_IF_ADDREF(*aFile = GetItemAt(aIndex));

  return NS_OK;
}

// nsDOMFileError implementation

DOMCI_DATA(FileError, nsDOMFileError)

NS_INTERFACE_MAP_BEGIN(nsDOMFileError)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFileError)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFileError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(FileError)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMFileError)
NS_IMPL_RELEASE(nsDOMFileError)

NS_IMETHODIMP
nsDOMFileError::GetCode(PRUint16* aCode)
{
  *aCode = mCode;
  return NS_OK;
}

nsDOMFileInternalUrlHolder::nsDOMFileInternalUrlHolder(nsIDOMBlob* aFile,
                                                       nsIPrincipal* aPrincipal
                                                       MOZILLA_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL) {
  MOZILLA_GUARD_OBJECT_NOTIFIER_INIT;
  aFile->GetInternalUrl(aPrincipal, mUrl);
}
 
nsDOMFileInternalUrlHolder::~nsDOMFileInternalUrlHolder() {
  if (!mUrl.IsEmpty()) {
    nsCAutoString narrowUrl;
    CopyUTF16toUTF8(mUrl, narrowUrl);
    nsFileDataProtocolHandler::RemoveFileDataEntry(narrowUrl);
  }
}
