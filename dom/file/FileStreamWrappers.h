/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_file_filestreamwrappers_h__
#define mozilla_dom_file_filestreamwrappers_h__

#include "FileCommon.h"

#include "nsIInputStream.h"
#include "nsIOutputStream.h"

BEGIN_FILE_NAMESPACE

class FileHelper;

class FileStreamWrapper : public nsISupports
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  FileStreamWrapper(nsISupports* aFileStream,
                    FileHelper* aFileHelper,
                    uint64_t aOffset,
                    uint64_t aLimit,
                    uint32_t aFlags);

  virtual ~FileStreamWrapper();

  enum {
    NOTIFY_PROGRESS = 1 << 0,
    NOTIFY_CLOSE = 1 << 1,
    NOTIFY_DESTROY = 1 << 2
  };

protected:
  nsCOMPtr<nsISupports> mFileStream;
  nsRefPtr<FileHelper> mFileHelper;
  uint64_t mOffset;
  uint64_t mLimit;
  uint32_t mFlags;
  bool mFirstTime;
};

class FileInputStreamWrapper : public FileStreamWrapper,
                               public nsIInputStream
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINPUTSTREAM

  FileInputStreamWrapper(nsISupports* aFileStream,
                         FileHelper* aFileHelper,
                         uint64_t aOffset,
                         uint64_t aLimit,
                         uint32_t aFlags);

protected:
  virtual ~FileInputStreamWrapper()
  { }

private:
  nsCOMPtr<nsIInputStream> mInputStream;
};

class FileOutputStreamWrapper : public FileStreamWrapper,
                                public nsIOutputStream
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIOUTPUTSTREAM

  FileOutputStreamWrapper(nsISupports* aFileStream,
                         FileHelper* aFileHelper,
                         uint64_t aOffset,
                         uint64_t aLimit,
                         uint32_t aFlags);

protected:
  virtual ~FileOutputStreamWrapper()
  { }

private:
  nsCOMPtr<nsIOutputStream> mOutputStream;
#ifdef DEBUG
  void* mWriteThread;
#endif
};

END_FILE_NAMESPACE

#endif // mozilla_dom_file_filestreamwrappers_h__
