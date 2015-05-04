/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeviceStorageFileDescriptor_h
#define DeviceStorageFileDescriptor_h

#include "mozilla/ipc/FileDescriptor.h"

struct DeviceStorageFileDescriptor final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DeviceStorageFileDescriptor)
  nsRefPtr<DeviceStorageFile> mDSFile;
  mozilla::ipc::FileDescriptor mFileDescriptor;
private:
  ~DeviceStorageFileDescriptor() {}
};

#endif
