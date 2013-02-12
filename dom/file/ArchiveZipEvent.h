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

#include "DictionaryHelpers.h"

BEGIN_FILE_NAMESPACE

/**
 * ArchiveZipItem - ArchiveItem for ArchiveReaderZipEvent
 */
class ArchiveZipItem : public ArchiveItem
{
public:
  ArchiveZipItem(const char* aFilename,
                 const ZipCentral& aCentralStruct,
                 const mozilla::idl::ArchiveReaderOptions& aOptions);
  virtual ~ArchiveZipItem();

  nsresult GetFilename(nsString& aFilename);

  // From zipItem to DOMFile:
  virtual nsIDOMFile* File(ArchiveReader* aArchiveReader);

public: // for the event
  static uint32_t StrToInt32(const uint8_t* aStr);
  static uint16_t StrToInt16(const uint8_t* aStr);

private:
  nsresult ConvertFilename();

private: // data
  nsCString mFilename;

  nsString mFilenameU;
  ZipCentral mCentralStruct;

  mozilla::idl::ArchiveReaderOptions mOptions;
};

/**
 * ArchiveReaderEvent implements the ArchiveReaderEvent for the ZIP format
 */
class ArchiveReaderZipEvent : public ArchiveReaderEvent
{
public:
  ArchiveReaderZipEvent(ArchiveReader* aArchiveReader,
                        const mozilla::idl::ArchiveReaderOptions& aOptions);

  nsresult Exec();

private:
  mozilla::idl::ArchiveReaderOptions mOptions;
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_domarchivezipevent_h__

