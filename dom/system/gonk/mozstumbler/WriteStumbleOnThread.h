/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WriteStumbleOnThread_H
#define WriteStumbleOnThread_H

#include "mozilla/Atomics.h"

class DeleteRunnable;

/*
 This class is the entry point to stumbling, in that it
 receives the location+cell+wifi string and writes it
 to disk, or instead, it calls UploadStumbleRunnable
 to upload the data.

 Writes will happen until the file is a max size, then stop.
 Uploads will happen only when the file is one day old.
 The purpose of these decisions is to have very simple rate-limiting
 on the writes, as well as the uploads.

 There is only one file active; it is either being used for writing,
 or for uploading. If the file is ready for uploading, no further
 writes will take place until this file has been uploaded.
 This can mean writing might not take place for days until the uploaded
 file is processed. This is correct by-design.

 A notable limitation is that the upload is triggered by a location event,
 this is used as an arbitrary and simple trigger. In future, there are
 better events that can be used, such as detecting network activity.

 This thread is guarded so that only one instance is active (see the
 mozilla::Atomics used for this).
 */
class WriteStumbleOnThread : public mozilla::Runnable
{
public:
  explicit WriteStumbleOnThread(const nsCString& aDesc)
  : mDesc(aDesc)
  {}

  NS_IMETHOD Run() override;

  static void UploadEnded(bool deleteUploadFile);

  // Used externally to determine if cell+wifi scans should happen
  // (returns false for that case).
  static bool IsFileWaitingForUpload();

private:
  friend class DeleteRunnable;

  enum class Partition {
    Begining,
    Middle,
    End,
    Unknown
  };

  enum class UploadFileStatus {
    NoFile, Exists, ExistsAndReadyToUpload
  };

  ~WriteStumbleOnThread() {}

  Partition GetWritePosition();
  UploadFileStatus GetUploadFileStatus();
  void WriteJSON(Partition aPart);
  void Upload();

  nsCString mDesc;

  // Only run one instance of this
  static mozilla::Atomic<bool> sIsAlreadyRunning;

  static mozilla::Atomic<bool> sIsFileWaitingForUpload;

  // Limit the upload attempts per day. If the device is rebooted
  // this resets the allowed attempts, which is acceptable.
  struct UploadFreqGuard {
    int attempts;
    int daySinceEpoch;
  };
  static UploadFreqGuard sUploadFreqGuard;

};

#endif
