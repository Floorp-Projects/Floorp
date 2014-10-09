/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_archivereader_domarchivezipevent_h__
#define mozilla_dom_archivereader_domarchivezipevent_h__

#include "mozilla/Attributes.h"
#include "ArchiveEvent.h"

#include "ArchiveReaderCommon.h"
#include "zipstruct.h"

BEGIN_ARCHIVEREADER_NAMESPACE

/**
 * ArchiveZipItem - ArchiveItem for ArchiveReaderZipEvent
 */
class ArchiveZipItem : public ArchiveItem
{
public:
  ArchiveZipItem(const char* aFilename,
                 const ZipCentral& aCentralStruct,
                 const nsACString& aEncoding);
protected:
  virtual ~ArchiveZipItem();

public:
  nsresult GetFilename(nsString& aFilename) MOZ_OVERRIDE;

  // From zipItem to File:
  virtual nsIDOMFile* File(ArchiveReader* aArchiveReader) MOZ_OVERRIDE;

public: // for the event
  static uint32_t StrToInt32(const uint8_t* aStr);
  static uint16_t StrToInt16(const uint8_t* aStr);

private:
  nsresult ConvertFilename();

private: // data
  nsCString mFilename;

  nsString mFilenameU;
  ZipCentral mCentralStruct;

  nsCString mEncoding;
};

/**
 * ArchiveReaderEvent implements the ArchiveReaderEvent for the ZIP format
 */
class ArchiveReaderZipEvent : public ArchiveReaderEvent
{
public:
  ArchiveReaderZipEvent(ArchiveReader* aArchiveReader,
                        const nsACString& aEncoding);

  nsresult Exec() MOZ_OVERRIDE;

private:
  nsCString mEncoding;
};

END_ARCHIVEREADER_NAMESPACE

#endif // mozilla_dom_archivereader_domarchivezipevent_h__
