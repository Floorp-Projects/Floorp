/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashReporterClient.h"
#include "CrashReporterMetadataShmem.h"
#include "nsISupportsImpl.h"

#ifdef MOZ_CRASHREPORTER
namespace mozilla {
namespace ipc {

StaticMutex CrashReporterClient::sLock;
StaticRefPtr<CrashReporterClient> CrashReporterClient::sClientSingleton;

CrashReporterClient::CrashReporterClient(const Shmem& aShmem)
 : mMetadata(new CrashReporterMetadataShmem(aShmem))
{
  MOZ_COUNT_CTOR(CrashReporterClient);
}

CrashReporterClient::~CrashReporterClient()
{
  MOZ_COUNT_DTOR(CrashReporterClient);
}

void
CrashReporterClient::AnnotateCrashReport(const nsCString& aKey, const nsCString& aData)
{
  StaticMutexAutoLock lock(sLock);
  mMetadata->AnnotateCrashReport(aKey, aData);
}

void
CrashReporterClient::AppendAppNotes(const nsCString& aData)
{
  StaticMutexAutoLock lock(sLock);
  mMetadata->AppendAppNotes(aData);
}

/* static */ void
CrashReporterClient::InitSingletonWithShmem(const Shmem& aShmem)
{
  StaticMutexAutoLock lock(sLock);

  MOZ_ASSERT(!sClientSingleton);
  sClientSingleton = new CrashReporterClient(aShmem);
}

/* static */ void
CrashReporterClient::DestroySingleton()
{
  StaticMutexAutoLock lock(sLock);
  sClientSingleton = nullptr;
}

/* static */ RefPtr<CrashReporterClient>
CrashReporterClient::GetSingleton()
{
  StaticMutexAutoLock lock(sLock);
  return sClientSingleton;
}

} // namespace ipc
} // namespace mozilla
#endif // MOZ_CRASHREPORTER
