/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArchiveZipEvent.h"
#include "ArchiveZipFile.h"

#include "nsContentUtils.h"
#include "nsCExternalHandlerService.h"

USING_FILE_NAMESPACE

#ifndef PATH_MAX
#  define PATH_MAX 65536 // The filename length is stored in 2 bytes
#endif

// ArchiveZipItem
ArchiveZipItem::ArchiveZipItem(const char* aFilename,
                               ZipCentral& aCentralStruct)
: mFilename(aFilename),
  mCentralStruct(aCentralStruct)
{
}

// Getter/Setter for the filename
nsCString
ArchiveZipItem::GetFilename()
{
  return mFilename;
}

void
ArchiveZipItem::SetFilename(const nsCString& aFilename)
{
  mFilename = aFilename;
}


// From zipItem to DOMFile:
nsIDOMFile*
ArchiveZipItem::File(ArchiveReader* aArchiveReader)
{
  return new ArchiveZipFile(NS_ConvertUTF8toUTF16(mFilename),
                            NS_ConvertUTF8toUTF16(GetType()),
                            StrToInt32(mCentralStruct.orglen),
                            mCentralStruct,
                            aArchiveReader);
}

PRUint32
ArchiveZipItem::StrToInt32(const PRUint8* aStr)
{
  return (PRUint32)( (aStr [0] <<  0) |
                     (aStr [1] <<  8) |
                     (aStr [2] << 16) |
                     (aStr [3] << 24) );
}

PRUint16
ArchiveZipItem::StrToInt16(const PRUint8* aStr)
{
  return (PRUint16) ((aStr [0]) | (aStr [1] << 8));
}

// ArchiveReaderZipEvent

ArchiveReaderZipEvent::ArchiveReaderZipEvent(ArchiveReader* aArchiveReader)
: ArchiveReaderEvent(aArchiveReader)
{
}

// NOTE: this runs in a different thread!!
nsresult
ArchiveReaderZipEvent::Exec()
{
  PRUint32 centralOffset(0);
  nsresult rv;

  nsCOMPtr<nsIInputStream> inputStream;
  rv = mArchiveReader->GetInputStream(getter_AddRefs(inputStream));
  if (rv != NS_OK || !inputStream) {
    return RunShare(NS_ERROR_UNEXPECTED);
  }

  // From the input stream to a seekable stream
  nsCOMPtr<nsISeekableStream> seekableStream;
  seekableStream = do_QueryInterface(inputStream);
  if (!seekableStream) {
    return RunShare(NS_ERROR_UNEXPECTED);
  }

  PRUint64 size;
  rv = mArchiveReader->GetSize(&size);
  if (rv != NS_OK) {
    return RunShare(NS_ERROR_UNEXPECTED);
  }

  // Reading backward.. looking for the ZipEnd signature
  for (PRUint64 curr = size - ZIPEND_SIZE; curr > 4; --curr)
  {
    seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, curr);

    PRUint8 buffer[ZIPEND_SIZE];
    PRUint32 ret;

    rv = inputStream->Read((char*)buffer, sizeof(buffer), &ret);
    if (rv != NS_OK || ret != sizeof(buffer)) {
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
    PRUint32 ret;
    
    rv = inputStream->Read((char*)&centralStruct, ZIPCENTRAL_SIZE, &ret);
    if (rv != NS_OK || ret != ZIPCENTRAL_SIZE) {
      return RunShare(NS_ERROR_UNEXPECTED);
    }

    PRUint16 filenameLen = ArchiveZipItem::StrToInt16(centralStruct.filename_len);
    PRUint16 extraLen = ArchiveZipItem::StrToInt16(centralStruct.extrafield_len);
    PRUint16 commentLen = ArchiveZipItem::StrToInt16(centralStruct.commentfield_len);

    // Point to the next item at the top of loop
    centralOffset += ZIPCENTRAL_SIZE + filenameLen + extraLen + commentLen;
    if (filenameLen == 0 || filenameLen >= PATH_MAX || centralOffset >= size) {
      return RunShare(NS_ERROR_FILE_CORRUPTED);
    }

    // Read the name:
    char* filename = (char*)PR_Malloc(filenameLen + 1);
    rv = inputStream->Read(filename, filenameLen, &ret);
    if (rv != NS_OK || ret != filenameLen) {
      return RunShare(NS_ERROR_UNEXPECTED);
    }

    filename[filenameLen] = 0;

    // We ignore the directories:
    if (filename[filenameLen - 1] != '/') {
      mFileList.AppendElement(new ArchiveZipItem(filename, centralStruct));
    }

    PR_Free(filename);

    // Ignore the rest
    seekableStream->Seek(nsISeekableStream::NS_SEEK_CUR, extraLen + commentLen);
  }

  return RunShare(NS_OK);
}
