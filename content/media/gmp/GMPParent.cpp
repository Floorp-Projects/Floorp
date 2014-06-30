/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPParent.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIInputStream.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "mozIGeckoMediaPluginService.h"

namespace mozilla {
namespace gmp {

GMPParent::GMPParent()
  : mState(GMPStateNotLoaded)
  , mProcess(nullptr)
{
}

GMPParent::~GMPParent()
{
}

nsresult
GMPParent::Init(nsIFile* aPluginDir)
{
  MOZ_ASSERT(aPluginDir);
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  mDirectory = aPluginDir;

  nsAutoString leafname;
  nsresult rv = aPluginDir->GetLeafName(leafname);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MOZ_ASSERT(leafname.Length() > 4);
  mName = Substring(leafname, 4);

  return ReadGMPMetaData();
}

nsresult
GMPParent::LoadProcess()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (mState == GMPStateLoaded) {
    return NS_OK;
  }

  nsAutoCString path;
  if (NS_FAILED(mDirectory->GetNativePath(path))) {
    return NS_ERROR_FAILURE;
  }

  mProcess = new GMPProcessParent(path.get());
  if (!mProcess->Launch(30 * 1000)) {
    mProcess->Delete();
    mProcess = nullptr;
    return NS_ERROR_FAILURE;
  }

  bool opened = Open(mProcess->GetChannel(), mProcess->GetChildProcessHandle());
  if (!opened) {
    mProcess->Delete();
    mProcess = nullptr;
    return NS_ERROR_FAILURE;
  }

  mState = GMPStateLoaded;

  return NS_OK;
}

void
GMPParent::MaybeUnloadProcess()
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (mVideoDecoders.Length() == 0 &&
      mVideoEncoders.Length() == 0) {
    UnloadProcess();
  }
}

void
GMPParent::UnloadProcess()
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (mState == GMPStateNotLoaded) {
    MOZ_ASSERT(mVideoDecoders.IsEmpty() && mVideoEncoders.IsEmpty());
    return;
  }

  mState = GMPStateNotLoaded;

  // Invalidate and remove any remaining API objects.
  for (uint32_t i = mVideoDecoders.Length(); i > 0; i--) {
    mVideoDecoders[i - 1]->DecodingComplete();
  }

  // Invalidate and remove any remaining API objects.
  for (uint32_t i = mVideoEncoders.Length(); i > 0; i--) {
    mVideoEncoders[i - 1]->EncodingComplete();
  }

  Close();
  if (mProcess) {
    mProcess->Delete();
    mProcess = nullptr;
  }
}

void
GMPParent::VideoDecoderDestroyed(GMPVideoDecoderParent* aDecoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ALWAYS_TRUE(mVideoDecoders.RemoveElement(aDecoder));

  // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
  // until after this has completed.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::MaybeUnloadProcess);
  NS_DispatchToCurrentThread(event);
}

void
GMPParent::VideoEncoderDestroyed(GMPVideoEncoderParent* aEncoder)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  MOZ_ALWAYS_TRUE(mVideoEncoders.RemoveElement(aEncoder));

  // Recv__delete__ is on the stack, don't potentially destroy the top-level actor
  // until after this has completed.
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &GMPParent::MaybeUnloadProcess);
  NS_DispatchToCurrentThread(event);
}

GMPState
GMPParent::State() const
{
  return mState;
}

#ifdef DEBUG
nsIThread*
GMPParent::GMPThread()
{
  if (!mGMPThread) {
    nsCOMPtr<mozIGeckoMediaPluginService> mps = do_GetService("@mozilla.org/gecko-media-plugin-service;1");
    MOZ_ASSERT(mps);
    if (!mps) {
      return nullptr;
    }
    mps->GetThread(getter_AddRefs(mGMPThread));
    MOZ_ASSERT(mGMPThread);
  }

  return mGMPThread;
}
#endif

bool
GMPParent::SupportsAPI(const nsCString& aAPI, const nsCString& aTag)
{
  for (uint32_t i = 0; i < mCapabilities.Length(); i++) {
    if (!mCapabilities[i]->mAPIName.Equals(aAPI)) {
      continue;
    }
    nsTArray<nsCString>& tags = mCapabilities[i]->mAPITags;
    for (uint32_t j = 0; j < tags.Length(); j++) {
      if (tags[j].Equals(aTag)) {
        return true;
      }
    }
  }
  return false;
}

bool
GMPParent::EnsureProcessLoaded()
{
  if (mState == GMPStateLoaded) {
    return true;
  }

  nsresult rv = LoadProcess();

  return NS_SUCCEEDED(rv);
}

nsresult
GMPParent::GetGMPVideoDecoder(GMPVideoDecoderParent** aGMPVD)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  PGMPVideoDecoderParent* pvdp = SendPGMPVideoDecoderConstructor();
  if (!pvdp) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<GMPVideoDecoderParent> vdp = static_cast<GMPVideoDecoderParent*>(pvdp);
  mVideoDecoders.AppendElement(vdp);
  vdp.forget(aGMPVD);

  return NS_OK;
}

nsresult
GMPParent::GetGMPVideoEncoder(GMPVideoEncoderParent** aGMPVE)
{
  MOZ_ASSERT(GMPThread() == NS_GetCurrentThread());

  if (!EnsureProcessLoaded()) {
    return NS_ERROR_FAILURE;
  }

  PGMPVideoEncoderParent* pvep = SendPGMPVideoEncoderConstructor();
  if (!pvep) {
    return NS_ERROR_FAILURE;
  }
  nsRefPtr<GMPVideoEncoderParent> vep = static_cast<GMPVideoEncoderParent*>(pvep);
  mVideoEncoders.AppendElement(vep);
  vep.forget(aGMPVE);

  return NS_OK;
}

void
GMPParent::ActorDestroy(ActorDestroyReason aWhy)
{
  UnloadProcess();
}

PGMPVideoDecoderParent*
GMPParent::AllocPGMPVideoDecoderParent()
{
  GMPVideoDecoderParent* vdp = new GMPVideoDecoderParent(this);
  NS_ADDREF(vdp);
  return vdp;
}

bool
GMPParent::DeallocPGMPVideoDecoderParent(PGMPVideoDecoderParent* aActor)
{
  GMPVideoDecoderParent* vdp = static_cast<GMPVideoDecoderParent*>(aActor);
  NS_RELEASE(vdp);
  return true;
}

PGMPVideoEncoderParent*
GMPParent::AllocPGMPVideoEncoderParent()
{
  GMPVideoEncoderParent* vep = new GMPVideoEncoderParent(this);
  NS_ADDREF(vep);
  return vep;
}

bool
GMPParent::DeallocPGMPVideoEncoderParent(PGMPVideoEncoderParent* aActor)
{
  GMPVideoEncoderParent* vep = static_cast<GMPVideoEncoderParent*>(aActor);
  NS_RELEASE(vep);
  return true;
}

nsresult
ParseNextRecord(nsILineInputStream* aLineInputStream,
                const nsCString& aPrefix,
                nsCString& aResult,
                bool& aMoreLines)
{
  nsAutoCString record;
  nsresult rv = aLineInputStream->ReadLine(record, &aMoreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (record.Length() <= aPrefix.Length() ||
      !Substring(record, 0, aPrefix.Length()).Equals(aPrefix)) {
    return NS_ERROR_FAILURE;
  }

  aResult = Substring(record, aPrefix.Length());
  aResult.Trim("\b\t\r\n ");

  return NS_OK;
}

nsresult
GMPParent::ReadGMPMetaData()
{
  MOZ_ASSERT(mDirectory, "Plugin directory cannot be NULL!");
  MOZ_ASSERT(!mName.IsEmpty(), "Plugin mName cannot be empty!");

  nsCOMPtr<nsIFile> infoFile;
  nsresult rv = mDirectory->Clone(getter_AddRefs(infoFile));
  if (NS_FAILED(rv)) {
    return rv;
  }
  infoFile->AppendRelativePath(mName + NS_LITERAL_STRING(".info"));

  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), infoFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(inputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCString value;
  bool moreLines = false;

  // 'Name:' record
  nsCString prefix = NS_LITERAL_CSTRING("Name:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (value.IsEmpty()) {
    // Not OK for name to be empty. Must have one non-whitespace character.
    return NS_ERROR_FAILURE;
  }
  mDisplayName = value;

  // 'Description:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("Description:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mDescription = value;

  // 'Version:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("Version:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  mVersion = value;

  // 'Capability:' record
  if (!moreLines) {
    return NS_ERROR_FAILURE;
  }
  prefix = NS_LITERAL_CSTRING("APIs:");
  rv = ParseNextRecord(lineInputStream, prefix, value, moreLines);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCCharSeparatedTokenizer apiTokens(value, ',');
  while (apiTokens.hasMoreTokens()) {
    nsAutoCString api(apiTokens.nextToken());
    api.StripWhitespace();
    if (api.IsEmpty()) {
      continue;
    }

    int32_t tagsStart = api.FindChar('[');
    if (tagsStart == 0) {
      // Not allowed to be the first character.
      // API name must be at least one character.
      continue;
    }

    auto cap = new GMPCapability();
    if (tagsStart == -1) {
      // No tags.
      cap->mAPIName.Assign(api);
    } else {
      auto tagsEnd = api.FindChar(']');
      if (tagsEnd == -1 || tagsEnd < tagsStart) {
        // Invalid syntax, skip whole capability.
        delete cap;
        continue;
      }

      cap->mAPIName.Assign(Substring(api, 0, tagsStart));

      if ((tagsEnd - tagsStart) > 1) {
        const nsDependentCSubstring ts(Substring(api, tagsStart + 1, tagsEnd - tagsStart - 1));
        nsCCharSeparatedTokenizer tagTokens(ts, ':');
        while (tagTokens.hasMoreTokens()) {
          const nsDependentCSubstring tag(tagTokens.nextToken());
          cap->mAPITags.AppendElement(tag);
        }
      }
    }

    mCapabilities.AppendElement(cap);
  }

  if (mCapabilities.IsEmpty()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

bool
GMPParent::CanBeSharedCrossOrigin() const
{
  return mOrigin.IsEmpty();
}

bool
GMPParent::CanBeUsedFrom(const nsAString& aOrigin) const
{
  return (mOrigin.IsEmpty() && State() == GMPStateNotLoaded) ||
         mOrigin.Equals(aOrigin);
}

void
GMPParent::SetOrigin(const nsAString& aOrigin)
{
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(CanBeUsedFrom(aOrigin));
  mOrigin = aOrigin;
}

} // namespace gmp
} // namespace mozilla
