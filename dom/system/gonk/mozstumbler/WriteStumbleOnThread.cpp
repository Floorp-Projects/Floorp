/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WriteStumbleOnThread.h"
#include "StumblerLogging.h"
#include "UploadStumbleRunnable.h"
#include "nsDumpUtils.h"
#include "nsGZFileWriter.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsPrintfCString.h"

#define MAXFILESIZE_KB (15 * 1024)
#define ONEDAY_IN_MSEC (24 * 60 * 60 * 1000)
#define MAX_UPLOAD_ATTEMPTS 20

mozilla::Atomic<bool> WriteStumbleOnThread::sIsAlreadyRunning(false);
WriteStumbleOnThread::UploadFreqGuard WriteStumbleOnThread::sUploadFreqGuard = {0};

#define FILENAME_INPROGRESS NS_LITERAL_CSTRING("stumbles.json.gz")
#define FILENAME_COMPLETED NS_LITERAL_CSTRING("stumbles.done.json.gz")
#define OUTPUT_DIR NS_LITERAL_CSTRING("mozstumbler")

class DeleteRunnable : public nsRunnable
{
  public:
    DeleteRunnable() {}

    NS_IMETHODIMP
    Run() override
    {
      nsCOMPtr<nsIFile> tmpFile;
      nsresult rv = nsDumpUtils::OpenTempFile(FILENAME_COMPLETED,
                                              getter_AddRefs(tmpFile),
                                              OUTPUT_DIR,
                                              nsDumpUtils::CREATE);
      if (NS_SUCCEEDED(rv)) {
        tmpFile->Remove(true);
      }
      // critically, this sets this flag to false so writing can happen again
      WriteStumbleOnThread::sIsAlreadyRunning = false;
      return NS_OK;
    }

  private:
    ~DeleteRunnable() {}
};

void
WriteStumbleOnThread::UploadEnded(bool deleteUploadFile)
{
  if (!deleteUploadFile) {
    sIsAlreadyRunning = false;
    return;
  }

  nsCOMPtr<nsIEventTarget> target = do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  MOZ_ASSERT(target);
  nsCOMPtr<nsIRunnable> event = new DeleteRunnable();
  target->Dispatch(event, NS_DISPATCH_NORMAL);
}

void
WriteStumbleOnThread::WriteJSON(Partition aPart)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv;
  rv = nsDumpUtils::OpenTempFile(FILENAME_INPROGRESS, getter_AddRefs(tmpFile),
                                 OUTPUT_DIR, nsDumpUtils::CREATE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("Open a file for stumble failed");
    return;
  }

  nsRefPtr<nsGZFileWriter> gzWriter = new nsGZFileWriter(nsGZFileWriter::Append);
  rv = gzWriter->Init(tmpFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("gzWriter init failed");
    return;
  }

  /*
   The json format is like below.
   {items:[
   {item},
   {item},
   {item}
   ]}
   */

  // Need to add "]}" after the last item
  if (aPart == Partition::End) {
    gzWriter->Write("]}");
    rv = gzWriter->Finish();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      STUMBLER_ERR("ostream finish failed");
    }

    nsCOMPtr<nsIFile> targetFile;
    nsresult rv = nsDumpUtils::OpenTempFile(FILENAME_COMPLETED, getter_AddRefs(targetFile),
                                            OUTPUT_DIR, nsDumpUtils::CREATE);
    nsAutoString targetFilename;
    rv = targetFile->GetLeafName(targetFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      STUMBLER_ERR("Get Filename failed");
      return;
    }
    rv = targetFile->Remove(true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      STUMBLER_ERR("Remove File failed");
      return;
    }
    // Rename tmpfile
    rv = tmpFile->MoveTo(/* directory */ nullptr, targetFilename);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      STUMBLER_ERR("Rename File failed");
      return;
    }
    return;
  }

  // Need to add "{items:[" before the first item
  if (aPart == Partition::Begining) {
    gzWriter->Write("{\"items\":[{");
  } else if (aPart == Partition::Middle) {
    gzWriter->Write(",{");
  }
  gzWriter->Write(mDesc.get());
  //  one item is ended with '}' (e.g. {item})
  gzWriter->Write("}");
  rv = gzWriter->Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("ostream finish failed");
  }

  // check if it is the end of this file
  int64_t fileSize = 0;
  rv = tmpFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("GetFileSize failed");
    return;
  }
  if (fileSize >= MAXFILESIZE_KB) {
    WriteJSON(Partition::End);
    return;
  }
}

WriteStumbleOnThread::Partition
WriteStumbleOnThread::GetWritePosition()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = nsDumpUtils::OpenTempFile(FILENAME_INPROGRESS, getter_AddRefs(tmpFile),
                                          OUTPUT_DIR, nsDumpUtils::CREATE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("Open a file for stumble failed");
    return Partition::Unknown;
  }

  int64_t fileSize = 0;
  rv = tmpFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("GetFileSize failed");
    return Partition::Unknown;
  }

  if (fileSize == 0) {
    return Partition::Begining;
  } else if (fileSize >= MAXFILESIZE_KB) {
    return Partition::End;
  } else {
    return Partition::Middle;
  }
}

NS_IMETHODIMP
WriteStumbleOnThread::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  bool b = sIsAlreadyRunning.exchange(true);
  if (b) {
    return NS_OK;
  }

  UploadFileStatus status = GetUploadFileStatus();

  if (UploadFileStatus::NoFile != status) {
    if (UploadFileStatus::ExistsAndReadyToUpload == status) {
      Upload();
      return NS_OK;
    }
  } else {
    Partition partition = GetWritePosition();
    if (partition == Partition::Unknown) {
      STUMBLER_ERR("GetWritePosition failed, skip once");
    } else {
      WriteJSON(partition);
    }
  }

  sIsAlreadyRunning = false;
  return NS_OK;
}


/*
 If the upload file exists, then check if it is one day old.
 • if it is a day old -> ExistsAndReadyToUpload
 • if it is less than the current day old -> Exists
 • otherwise -> NoFile

 The Exists case means that the upload and the stumbling is rate limited
 per-day to the size of the one file.
 */
WriteStumbleOnThread::UploadFileStatus
WriteStumbleOnThread::GetUploadFileStatus()
{
  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = nsDumpUtils::OpenTempFile(FILENAME_COMPLETED, getter_AddRefs(tmpFile),
                                          OUTPUT_DIR, nsDumpUtils::CREATE);
  int64_t fileSize;
  rv = tmpFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("GetFileSize failed");
    return UploadFileStatus::NoFile;
  }
  if (fileSize <= 0) {
    tmpFile->Remove(true);
    return UploadFileStatus::NoFile;
  }

  PRTime lastModifiedTime;
  tmpFile->GetLastModifiedTime(&lastModifiedTime);
  if ((PR_Now() / PR_USEC_PER_MSEC) - lastModifiedTime >= ONEDAY_IN_MSEC) {
    return UploadFileStatus::ExistsAndReadyToUpload;
  }
  return UploadFileStatus::Exists;
}

void
WriteStumbleOnThread::Upload()
{
  MOZ_ASSERT(!NS_IsMainThread());

  time_t seconds = time(0);
  int day = seconds / (60 * 60 * 24);

  if (sUploadFreqGuard.daySinceEpoch < day) {
    sUploadFreqGuard.daySinceEpoch = day;
    sUploadFreqGuard.attempts = 0;
  }

  sUploadFreqGuard.attempts++;
  if (sUploadFreqGuard.attempts > MAX_UPLOAD_ATTEMPTS) {
    STUMBLER_ERR("Too many upload attempts today");
    sIsAlreadyRunning = false;
    return;
  }

  nsCOMPtr<nsIFile> tmpFile;
  nsresult rv = nsDumpUtils::OpenTempFile(FILENAME_COMPLETED, getter_AddRefs(tmpFile),
                                          OUTPUT_DIR, nsDumpUtils::CREATE);
  int64_t fileSize;
  rv = tmpFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    STUMBLER_ERR("GetFileSize failed");
    sIsAlreadyRunning = false;
    return;
  }

  if (fileSize <= 0) {
    sIsAlreadyRunning = false;
    return;
  }

  // prepare json into nsIInputStream
  nsCOMPtr<nsIInputStream> inStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inStream), tmpFile);
  if (NS_FAILED(rv)) {
    sIsAlreadyRunning = false;
    return;
  }

  nsRefPtr<nsIRunnable> uploader = new UploadStumbleRunnable(inStream);
  NS_DispatchToMainThread(uploader);
}
