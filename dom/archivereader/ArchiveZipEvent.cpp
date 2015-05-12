/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveZipEvent.h"
#include "ArchiveZipFile.h"

#include "nsContentUtils.h"
#include "nsCExternalHandlerService.h"

using namespace mozilla::dom;

USING_ARCHIVEREADER_NAMESPACE

#ifndef PATH_MAX
#  define PATH_MAX 65536 // The filename length is stored in 2 bytes
#endif

ArchiveZipItem::ArchiveZipItem(const char* aFilename,
                               const ZipCentral& aCentralStruct,
                               const nsACString& aEncoding)
: mFilename(aFilename),
  mCentralStruct(aCentralStruct),
  mEncoding(aEncoding)
{
  MOZ_COUNT_CTOR(ArchiveZipItem);
}

ArchiveZipItem::~ArchiveZipItem()
{
  MOZ_COUNT_DTOR(ArchiveZipItem);
}

nsresult
ArchiveZipItem::ConvertFilename()
{
  if (mEncoding.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  nsString filenameU;
  nsresult rv = nsContentUtils::ConvertStringFromEncoding(
                  mEncoding,
                  mFilename, filenameU);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filenameU.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  mFilenameU = filenameU;
  return NS_OK;
}

nsresult
ArchiveZipItem::GetFilename(nsString& aFilename)
{
  if (mFilenameU.IsEmpty()) {
    // Maybe this string is UTF-8:
    if (IsUTF8(mFilename, false)) {
      mFilenameU = NS_ConvertUTF8toUTF16(mFilename);
    }

    // Let's use the enconding value for the dictionary
    else {
      nsresult rv = ConvertFilename();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  aFilename = mFilenameU;
  return NS_OK;
}

// From zipItem to File:
already_AddRefed<File>
ArchiveZipItem::GetFile(ArchiveReader* aArchiveReader)
{
  nsString filename;

  if (NS_FAILED(GetFilename(filename))) {
    return nullptr;
  }

  nsRefPtr<dom::File> file = dom::File::Create(aArchiveReader,
    new ArchiveZipBlobImpl(filename,
                           NS_ConvertUTF8toUTF16(GetType()),
                           StrToInt32(mCentralStruct.orglen),
                           mCentralStruct, aArchiveReader->GetBlobImpl()));
  MOZ_ASSERT(file);
  return file.forget();
}

uint32_t
ArchiveZipItem::StrToInt32(const uint8_t* aStr)
{
  return (uint32_t)( (aStr [0] <<  0) |
                     (aStr [1] <<  8) |
                     (aStr [2] << 16) |
                     (aStr [3] << 24) );
}

uint16_t
ArchiveZipItem::StrToInt16(const uint8_t* aStr)
{
  return (uint16_t) ((aStr [0]) | (aStr [1] << 8));
}

// ArchiveReaderZipEvent

ArchiveReaderZipEvent::ArchiveReaderZipEvent(ArchiveReader* aArchiveReader,
                                             const nsACString& aEncoding)
: ArchiveReaderEvent(aArchiveReader),
  mEncoding(aEncoding)
{
}

// NOTE: this runs in a different thread!!
nsresult
ArchiveReaderZipEvent::Exec()
{
  uint32_t centralOffset(0);
  nsresult rv;

  nsCOMPtr<nsIInputStream> inputStream;
  rv = mArchiveReader->GetInputStream(getter_AddRefs(inputStream));
  if (NS_FAILED(rv) || !inputStream) {
    return RunShare(NS_ERROR_UNEXPECTED);
  }

  // From the input stream to a seekable stream
  nsCOMPtr<nsISeekableStream> seekableStream;
  seekableStream = do_QueryInterface(inputStream);
  if (!seekableStream) {
    return RunShare(NS_ERROR_UNEXPECTED);
  }

  uint64_t size;
  rv = mArchiveReader->GetSize(&size);
  if (NS_FAILED(rv)) {
    return RunShare(NS_ERROR_UNEXPECTED);
  }

  // Reading backward.. looking for the ZipEnd signature
  for (uint64_t curr = size - ZIPEND_SIZE; curr > 4; --curr) {
    seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, curr);

    uint8_t buffer[ZIPEND_SIZE];
    uint32_t ret;

    rv = inputStream->Read((char*)buffer, sizeof(buffer), &ret);
    if (NS_FAILED(rv) || ret != sizeof(buffer)) {
      return RunShare(NS_ERROR_UNEXPECTED);
    }

    // Here we are:
    if (ArchiveZipItem::StrToInt32(buffer) == ENDSIG) {
      centralOffset = ArchiveZipItem::StrToInt32(((ZipEnd*)buffer)->offset_central_dir);
      break;
    }
  }

  // No central Offset
  if (!centralOffset || centralOffset >= size - ZIPEND_SIZE) {
    return RunShare(NS_ERROR_FAILURE);
  }

  // Seek to the first central directory:
  seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, centralOffset);

  // For each central directory:
  while (centralOffset <= size - ZIPCENTRAL_SIZE) {
    ZipCentral centralStruct;
    uint32_t ret;
    
    rv = inputStream->Read((char*)&centralStruct, ZIPCENTRAL_SIZE, &ret);
    if (NS_FAILED(rv) || ret != ZIPCENTRAL_SIZE) {
      return RunShare(NS_ERROR_UNEXPECTED);
    }

    uint16_t filenameLen = ArchiveZipItem::StrToInt16(centralStruct.filename_len);
    uint16_t extraLen = ArchiveZipItem::StrToInt16(centralStruct.extrafield_len);
    uint16_t commentLen = ArchiveZipItem::StrToInt16(centralStruct.commentfield_len);

    // Point to the next item at the top of loop
    centralOffset += ZIPCENTRAL_SIZE + filenameLen + extraLen + commentLen;
    if (filenameLen == 0 || filenameLen >= PATH_MAX || centralOffset >= size) {
      return RunShare(NS_ERROR_FILE_CORRUPTED);
    }

    // Read the name:
    nsAutoArrayPtr<char> filename(new char[filenameLen + 1]);
    rv = inputStream->Read(filename, filenameLen, &ret);
    if (NS_FAILED(rv) || ret != filenameLen) {
      return RunShare(NS_ERROR_UNEXPECTED);
    }

    filename[filenameLen] = 0;

    // We ignore the directories:
    if (filename[filenameLen - 1] != '/') {
      mFileList.AppendElement(new ArchiveZipItem(filename, centralStruct, mEncoding));
    }

    // Ignore the rest
    seekableStream->Seek(nsISeekableStream::NS_SEEK_CUR, extraLen + commentLen);
  }

  return RunShare(NS_OK);
}
