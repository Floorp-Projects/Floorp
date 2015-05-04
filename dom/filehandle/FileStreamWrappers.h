/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileStreamWrappers_h
#define mozilla_dom_FileStreamWrappers_h

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIIPCSerializableInputStream.h"

namespace mozilla {
namespace ipc {
class InputStreamParams;
} // namespace ipc

namespace dom {

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

  enum {
    NOTIFY_PROGRESS = 1 << 0,
    NOTIFY_CLOSE = 1 << 1,
    NOTIFY_DESTROY = 1 << 2
  };

protected:
  virtual ~FileStreamWrapper();

  nsCOMPtr<nsISupports> mFileStream;
  nsRefPtr<FileHelper> mFileHelper;
  uint64_t mOffset;
  uint64_t mLimit;
  uint32_t mFlags;
  bool mFirstTime;
};

class FileInputStreamWrapper : public FileStreamWrapper,
                               public nsIInputStream,
                               public nsIIPCSerializableInputStream
{
  typedef mozilla::ipc::InputStreamParams InputStreamParams;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

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
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileStreamWrappers_h
