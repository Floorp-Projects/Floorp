/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_domarchivezipevent_h__
#define mozilla_dom_file_domarchivezipevent_h__

#include "ArchiveEvent.h"

#include "FileCommon.h"
#include "zipstruct.h"

BEGIN_FILE_NAMESPACE

class ArchiveZipItem : public ArchiveItem
{
public:
  ArchiveZipItem(const char* aFilename,
                 ZipCentral& aCentralStruct);

  void SetFilename(const nsCString& aFilename);
  nsCString GetFilename();

  // From zipItem to DOMFile:
  virtual nsIDOMFile* File(ArchiveReader* aArchiveReader);

public: // for the event
  static PRUint32 StrToInt32(const PRUint8* aStr);
  static PRUint16 StrToInt16(const PRUint8* aStr);

private: // data
  nsCString mFilename;
  ZipCentral mCentralStruct;
};

class ArchiveReaderZipEvent : public ArchiveReaderEvent
{
public:
  ArchiveReaderZipEvent(ArchiveReader* aArchiveReader);

  nsresult Exec();
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domarchivezipevent_h__

