/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileReaderSyncPrivate.h"

#include "nsCExternalHandlerService.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMError.h"
#include "nsIDOMFile.h"
#include "nsCharsetAlias.h"
#include "nsICharsetDetector.h"
#include "nsIConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsIPlatformCharset.h"
#include "nsISeekableStream.h"
#include "nsISupportsImpl.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "RuntimeService.h"

#include "mozilla/Base64.h"

USING_WORKERS_NAMESPACE

NS_IMPL_ISUPPORTS1(FileReaderSyncPrivate, nsICharsetDetectionObserver)

FileReaderSyncPrivate::FileReaderSyncPrivate()
{
  MOZ_COUNT_CTOR(mozilla::dom::workers::FileReaderSyncPrivate);
}

FileReaderSyncPrivate::~FileReaderSyncPrivate()
{
  MOZ_COUNT_DTOR(mozilla::dom::workers::FileReaderSyncPrivate);
}

nsresult
FileReaderSyncPrivate::ReadAsArrayBuffer(nsIDOMBlob* aBlob, PRUint32 aLength,
                                         uint8* aBuffer)
{
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = aBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 numRead;
  rv = stream->Read((char*)aBuffer, aLength, &numRead);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(numRead == aLength, "failed to read data");

  return NS_OK;
}

nsresult
FileReaderSyncPrivate::ReadAsBinaryString(nsIDOMBlob* aBlob, nsAString& aResult)
{
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = aBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 numRead;
  do {
    char readBuf[4096];
    rv = stream->Read(readBuf, sizeof(readBuf), &numRead);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 oldLength = aResult.Length();
    AppendASCIItoUTF16(Substring(readBuf, readBuf + numRead), aResult);
    if (aResult.Length() - oldLength != numRead) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } while (numRead > 0);

  return NS_OK;
}

nsresult
FileReaderSyncPrivate::ReadAsText(nsIDOMBlob* aBlob,
                                  const nsAString& aEncoding, nsAString& aResult)
{
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = aBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString charsetGuess;
  if (aEncoding.IsEmpty()) {
    rv = GuessCharset(stream, charsetGuess);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(stream);
    NS_ENSURE_TRUE(seekable, NS_ERROR_FAILURE);

    // Seek to 0 because guessing the charset advances the stream.
    rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    CopyUTF16toUTF8(aEncoding, charsetGuess);
  }

  nsCString charset;
  rv = nsCharsetAlias::GetPreferred(charsetGuess, charset);
  NS_ENSURE_SUCCESS(rv, rv);

  return ConvertStream(stream, charset.get(), aResult);
}

nsresult
FileReaderSyncPrivate::ReadAsDataURL(nsIDOMBlob* aBlob, nsAString& aResult)
{
  nsAutoString scratchResult;
  scratchResult.AssignLiteral("data:");

  nsString contentType;
  aBlob->GetType(contentType);

  if (contentType.IsEmpty()) {
    scratchResult.AppendLiteral("application/octet-stream");
  } else {
    scratchResult.Append(contentType);
  }
  scratchResult.AppendLiteral(";base64,");

  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = aBlob->GetInternalStream(getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint64 size;
  rv = aBlob->GetSize(&size);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream, size);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString encodedData;
  rv = Base64EncodeInputStream(bufferedStream, encodedData, size);
  NS_ENSURE_SUCCESS(rv, rv);

  scratchResult.Append(encodedData);

  aResult = scratchResult;
  return NS_OK;
}

nsresult
FileReaderSyncPrivate::ConvertStream(nsIInputStream *aStream,
                                     const char *aCharset,
                                     nsAString &aResult)
{
  nsCOMPtr<nsIConverterInputStream> converterStream =
    do_CreateInstance("@mozilla.org/intl/converter-input-stream;1");
  NS_ENSURE_TRUE(converterStream, NS_ERROR_FAILURE);

  nsresult rv = converterStream->Init(aStream, aCharset, 8192,
                  nsIConverterInputStream::DEFAULT_REPLACEMENT_CHARACTER);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIUnicharInputStream> unicharStream =
    do_QueryInterface(converterStream);
  NS_ENSURE_TRUE(unicharStream, NS_ERROR_FAILURE);

  PRUint32 numChars;
  nsString result;
  while (NS_SUCCEEDED(unicharStream->ReadString(8192, result, &numChars)) &&
         numChars > 0) {
    PRUint32 oldLength = aResult.Length();
    aResult.Append(result);
    if (aResult.Length() - oldLength != result.Length()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return rv;
}

nsresult
FileReaderSyncPrivate::GuessCharset(nsIInputStream *aStream,
                                    nsACString &aCharset)
{
  // First try the universal charset detector
  nsCOMPtr<nsICharsetDetector> detector
    = do_CreateInstance(NS_CHARSET_DETECTOR_CONTRACTID_BASE
                        "universal_charset_detector");
  if (!detector) {
    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    // No universal charset detector, try the default charset detector
    const nsACString& detectorName = runtime->GetDetectorName();

    if (!detectorName.IsEmpty()) {
      nsCAutoString detectorContractID;
      detectorContractID.AssignLiteral(NS_CHARSET_DETECTOR_CONTRACTID_BASE);
      detectorContractID += detectorName;
      detector = do_CreateInstance(detectorContractID.get());
    }
  }

  nsresult rv;
  if (detector) {
    detector->Init(this);

    bool done;
    PRUint32 numRead;
    do {
      char readBuf[4096];
      rv = aStream->Read(readBuf, sizeof(readBuf), &numRead);
      NS_ENSURE_SUCCESS(rv, rv);
      if (numRead <= 0) {
        break;
      }
      rv = detector->DoIt(readBuf, numRead, &done);
      NS_ENSURE_SUCCESS(rv, rv);
    } while (!done);

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
    RuntimeService* runtime = RuntimeService::GetService();
    mCharset = runtime->GetSystemCharset();
  }

  if (mCharset.IsEmpty()) {
    // no sniffed or default charset, try UTF-8
    mCharset.AssignLiteral("UTF-8");
  }

  aCharset = mCharset;
  mCharset.Truncate();

  return NS_OK;
}

NS_IMETHODIMP
FileReaderSyncPrivate::Notify(const char* aCharset, nsDetectionConfident aConf)
{
  mCharset.Assign(aCharset);

  return NS_OK;
}
