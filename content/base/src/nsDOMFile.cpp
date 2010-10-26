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
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
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

#include "plbase64.h"
#include "prmem.h"

using namespace mozilla;

// nsDOMFile implementation

DOMCI_DATA(File, nsDOMFile)
DOMCI_DATA(Blob, nsDOMFile)

NS_INTERFACE_MAP_BEGIN(nsDOMFile)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFile)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBlob)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMFile, mIsFullFile)
  NS_INTERFACE_MAP_ENTRY(nsIXHRSendable)
  NS_INTERFACE_MAP_ENTRY(nsICharsetDetectionObserver)
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

NS_IMETHODIMP
nsDOMFile::GetFileName(nsAString &aFileName)
{
  return GetName(aFileName);
}

NS_IMETHODIMP
nsDOMFile::GetFileSize(PRUint64 *aFileSize)
{
  return GetSize(aFileSize);
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

// Makes sure that aStart and aStart + aLength is less then or equal to aSize
void
ClampToSize(PRUint64 aSize, PRUint64& aStart, PRUint64& aLength)
{
  if (aStart > aSize) {
    aStart = aLength = 0;
  }
  CheckedUint64 endOffset = aStart;
  endOffset += aLength;
  if (!endOffset.valid() || endOffset.value() > aSize) {
    aLength = aSize - aStart;
  }
}

NS_IMETHODIMP
nsDOMFile::Slice(PRUint64 aStart, PRUint64 aLength,
                 const nsAString& aContentType, nsIDOMBlob **aBlob)
{
  *aBlob = nsnull;

  // Truncate aLength and aStart so that we stay within this file.
  PRUint64 thisLength;
  nsresult rv = GetSize(&thisLength);
  NS_ENSURE_SUCCESS(rv, rv);
  ClampToSize(thisLength, aStart, aLength);
  
  // Create the new file
  NS_ADDREF(*aBlob = new nsDOMFile(this, aStart, aLength, aContentType));
  
  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::GetInternalStream(nsIInputStream **aStream)
{
  return mIsFullFile ?
    NS_NewLocalFileInputStream(aStream, mFile, -1, -1,
                               nsIFileInputStream::CLOSE_ON_EOF |
                               nsIFileInputStream::REOPEN_ON_REWIND) :
    NS_NewPartialLocalFileInputStream(aStream, mFile, mStart, mLength,
                                      -1, -1,
                                      nsIFileInputStream::CLOSE_ON_EOF |
                                      nsIFileInputStream::REOPEN_ON_REWIND);
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
nsDOMFile::GetAsText(const nsAString &aCharset, nsAString &aResult)
{
  aResult.Truncate();

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));

  nsCAutoString charsetGuess;
  if (!aCharset.IsEmpty()) {
    CopyUTF16toUTF8(aCharset, charsetGuess);
  } else {
    rv = GuessCharset(stream, charsetGuess);
    NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));

    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(stream);
    if (!seekable) return NS_ERROR_FAILURE;
    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));
  }

  nsCAutoString charset;
  nsCOMPtr<nsICharsetAlias> alias =
    do_GetService(NS_CHARSETALIAS_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = alias->GetPreferred(charsetGuess, charset);
  NS_ENSURE_SUCCESS(rv, rv);

  return DOMFileResult(ConvertStream(stream, charset.get(), aResult));
}

NS_IMETHODIMP
nsDOMFile::GetAsDataURL(nsAString &aResult)
{
  aResult.AssignLiteral("data:");

  nsresult rv;
  if (!mContentType.Length()) {
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString contentType;
    rv = mimeService->GetTypeFromFile(mFile, contentType);
    if (NS_SUCCEEDED(rv)) {
      CopyUTF8toUTF16(contentType, mContentType);
    }
  }

  if (mContentType.Length()) {
    aResult.Append(mContentType);
  } else {
    aResult.AppendLiteral("application/octet-stream");
  }
  aResult.AppendLiteral(";base64,");

  nsCOMPtr<nsIInputStream> stream;
  rv = GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));

  char readBuf[4096];
  PRUint32 leftOver = 0;
  PRUint32 numRead;
  do {
    rv = stream->Read(readBuf + leftOver, sizeof(readBuf) - leftOver, &numRead);
    NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));

    PRUint32 numEncode = numRead + leftOver;
    leftOver = 0;

    if (numEncode == 0) break;

    // unless this is the end of the file, encode in multiples of 3
    if (numRead > 0) {
      leftOver = numEncode % 3;
      numEncode -= leftOver;
    }

    // out buffer should be at least 4/3rds the read buf, plus a terminator
    char *base64 = PL_Base64Encode(readBuf, numEncode, nsnull);
    if (!base64) {
      return DOMFileResult(NS_ERROR_OUT_OF_MEMORY);
    }
    nsDependentCString str(base64);
    PRUint32 strLen = str.Length();
    PRUint32 oldLength = aResult.Length();
    AppendASCIItoUTF16(str, aResult);
    PR_Free(base64);
    if (aResult.Length() - oldLength != strLen) {
      return DOMFileResult(NS_ERROR_OUT_OF_MEMORY);
    }

    if (leftOver) {
      memmove(readBuf, readBuf + numEncode, leftOver);
    }
  } while (numRead > 0);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMFile::GetAsBinary(nsAString &aResult)
{
  aResult.Truncate();

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));

  PRUint32 numRead;
  do {
    char readBuf[4096];
    rv = stream->Read(readBuf, sizeof(readBuf), &numRead);
    NS_ENSURE_SUCCESS(rv, DOMFileResult(rv));
    PRUint32 oldLength = aResult.Length();
    AppendASCIItoUTF16(Substring(readBuf, readBuf + numRead), aResult);
    if (aResult.Length() - oldLength != numRead) {
      return DOMFileResult(NS_ERROR_OUT_OF_MEMORY);
    }
  } while (numRead > 0);

  return NS_OK;
}

nsresult
nsDOMFile::GuessCharset(nsIInputStream *aStream,
                        nsACString &aCharset)
{

  if (!mCharset.IsEmpty()) {
    aCharset = mCharset;
    return NS_OK;
  }

  // First try the universal charset detector
  nsCOMPtr<nsICharsetDetector> detector
    = do_CreateInstance(NS_CHARSET_DETECTOR_CONTRACTID_BASE
                        "universal_charset_detector");
  if (!detector) {
    // No universal charset detector, try the default charset detector
    const nsAdoptingString& detectorName =
      nsContentUtils::GetLocalizedStringPref("intl.charset.detector");
    if (!detectorName.IsEmpty()) {
      nsCAutoString detectorContractID;
      detectorContractID.AssignLiteral(NS_CHARSET_DETECTOR_CONTRACTID_BASE);
      AppendUTF16toUTF8(detectorName, detectorContractID);
      detector = do_CreateInstance(detectorContractID.get());
    }
  }

  nsresult rv;
  if (detector) {
    detector->Init(this);

    PRBool done;
    PRUint32 numRead;
    do {
      char readBuf[4096];
      rv = aStream->Read(readBuf, sizeof(readBuf), &numRead);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = detector->DoIt(readBuf, numRead, &done);
      NS_ENSURE_SUCCESS(rv, rv);
    } while (!done && numRead > 0);

    rv = detector->Done();
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // no charset detector available, check the BOM
    unsigned char sniffBuf[4];
    PRUint32 numRead;
    rv = aStream->Read(reinterpret_cast<char*>(sniffBuf),
                       sizeof(sniffBuf), &numRead);
    NS_ENSURE_SUCCESS(rv, rv);

    if (numRead >= 4 &&
        sniffBuf[0] == 0x00 &&
        sniffBuf[1] == 0x00 &&
        sniffBuf[2] == 0xfe &&
        sniffBuf[3] == 0xff) {
      mCharset = "UTF-32BE";
    } else if (numRead >= 4 &&
               sniffBuf[0] == 0xff &&
               sniffBuf[1] == 0xfe &&
               sniffBuf[2] == 0x00 &&
               sniffBuf[3] == 0x00) {
      mCharset = "UTF-32LE";
    } else if (numRead >= 2 &&
               sniffBuf[0] == 0xfe &&
               sniffBuf[1] == 0xff) {
      mCharset = "UTF-16BE";
    } else if (numRead >= 2 &&
               sniffBuf[0] == 0xff &&
               sniffBuf[1] == 0xfe) {
      mCharset = "UTF-16LE";
    } else if (numRead >= 3 &&
               sniffBuf[0] == 0xef &&
               sniffBuf[1] == 0xbb &&
               sniffBuf[2] == 0xbf) {
      mCharset = "UTF-8";
    }
  }

  if (mCharset.IsEmpty()) {
    // no charset detected, default to the system charset
    nsCOMPtr<nsIPlatformCharset> platformCharset =
      do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = platformCharset->GetCharset(kPlatformCharsetSel_PlainTextInFile,
                                       mCharset);
    }
  }

  if (mCharset.IsEmpty()) {
    // no sniffed or default charset, try UTF-8
    mCharset.AssignLiteral("UTF-8");
  }

  aCharset = mCharset;

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
nsDOMFile::Notify(const char* aCharset, nsDetectionConfident aConf)
{
  mCharset.Assign(aCharset);

  return NS_OK;
}

nsresult
nsDOMFile::ConvertStream(nsIInputStream *aStream,
                         const char *aCharset,
                         nsAString &aResult)
{
  aResult.Truncate();

  nsCOMPtr<nsIConverterInputStream> converterStream =
    do_CreateInstance("@mozilla.org/intl/converter-input-stream;1");
  if (!converterStream) return NS_ERROR_FAILURE;

  nsresult rv = converterStream->Init
                  (aStream, aCharset,
                   8192,
                   nsIConverterInputStream::DEFAULT_REPLACEMENT_CHARACTER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicharInputStream> unicharStream =
    do_QueryInterface(converterStream);
  if (!unicharStream) return NS_ERROR_FAILURE;

  PRUint32 numChars;
  nsString result;
  rv = unicharStream->ReadString(8192, result, &numChars);
  while (NS_SUCCEEDED(rv) && numChars > 0) {
    PRUint32 oldLength = aResult.Length();
    aResult.Append(result);
    if (aResult.Length() - oldLength != result.Length()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    rv = unicharStream->ReadString(8192, result, &numChars);
  }

  return rv;
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
nsDOMMemoryFile::Slice(PRUint64 aStart, PRUint64 aLength,
                       const nsAString& aContentType, nsIDOMBlob **aBlob)
{
  *aBlob = nsnull;

  // Truncate aLength and aStart so that we stay within this file.
  ClampToSize(mLength, aStart, aLength);

  // Create the new file
  NS_ADDREF(*aBlob = new nsDOMMemoryFile(this, aStart, aLength, aContentType));
  
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryFile::GetInternalStream(nsIInputStream **aStream)
{
  if (mLength > PR_INT32_MAX)
    return NS_ERROR_FAILURE;

  return NS_NewByteInputStream(aStream,
                               static_cast<const char*>(mDataOwner->mData) +
                               mStart,
                               (PRInt32)mLength);
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
