/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_CrashReporterMetadataShmem_h
#define mozilla_ipc_CrashReporterMetadataShmem_h

#include <stdint.h>
#include "mozilla/ipc/Shmem.h"
#include "nsExceptionHandler.h"
#include "nsString.h"

namespace mozilla {
namespace ipc {

class CrashReporterMetadataShmem
{
  typedef mozilla::ipc::Shmem Shmem;
  typedef CrashReporter::AnnotationTable AnnotationTable;

public:
  explicit CrashReporterMetadataShmem(const Shmem& aShmem);
  ~CrashReporterMetadataShmem();

  // Metadata writers. These must only be called in child processes.
  void AnnotateCrashReport(const nsCString& aKey, const nsCString& aData);
  void AppendAppNotes(const nsCString& aData);

  static void ReadAppNotes(const Shmem& aShmem, CrashReporter::AnnotationTable* aNotes);

private:
  void SyncNotesToShmem();

private:
  Shmem mShmem;

  AnnotationTable mNotes;
  nsCString mAppNotes;
};

} // namespace ipc
} // namespace mozilla

#endif // mozilla_ipc_CrashReporterMetadataShmem_h
