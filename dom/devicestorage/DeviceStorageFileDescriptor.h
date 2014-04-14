/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeviceStorageFileDescriptor_h
#define DeviceStorageFileDescriptor_h

#include "mozilla/ipc/FileDescriptor.h"

class DeviceStorageFileDescriptor MOZ_FINAL
  : public mozilla::RefCounted<DeviceStorageFileDescriptor>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(DeviceStorageFileDescriptor)
  nsRefPtr<DeviceStorageFile> mDSFile;
  mozilla::ipc::FileDescriptor mFileDescriptor;
};

#endif
