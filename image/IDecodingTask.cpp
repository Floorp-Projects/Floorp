/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDecodingTask.h"

#include "gfxPrefs.h"
#include "nsThreadUtils.h"

#include "Decoder.h"
#include "DecodePool.h"
#include "RasterImage.h"

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Helpers for sending notifications to the image associated with a decoder.
///////////////////////////////////////////////////////////////////////////////

static void
NotifyProgress(NotNull<Decoder*> aDecoder)
{
  MOZ_ASSERT(aDecoder->HasProgress() && !aDecoder->IsMetadataDecode());

  // Synchronously notify if we can.
  if (NS_IsMainThread() &&
      !(aDecoder->GetDecoderFlags() & DecoderFlags::ASYNC_NOTIFY)) {
    aDecoder->GetImage()->NotifyProgress(aDecoder->TakeProgress(),
                                         aDecoder->TakeInvalidRect(),
                                         aDecoder->GetSurfaceFlags());
    return;
  }

  // We're forced to notify asynchronously.
  NotNull<RefPtr<Decoder>> decoder = aDecoder;
  NS_DispatchToMainThread(NS_NewRunnableFunction([=]() -> void {
    decoder->GetImage()->NotifyProgress(decoder->TakeProgress(),
                                        decoder->TakeInvalidRect(),
                                        decoder->GetSurfaceFlags());
  }));
}

static void
NotifyDecodeComplete(NotNull<Decoder*> aDecoder)
{
  // Synchronously notify if we can.
  if (NS_IsMainThread() &&
      !(aDecoder->GetDecoderFlags() & DecoderFlags::ASYNC_NOTIFY)) {
    aDecoder->GetImage()->FinalizeDecoder(aDecoder);
    return;
  }

  // We're forced to notify asynchronously.
  NotNull<RefPtr<Decoder>> decoder = aDecoder;
  NS_DispatchToMainThread(NS_NewRunnableFunction([=]() -> void {
    decoder->GetImage()->FinalizeDecoder(decoder.get());
  }));
}


///////////////////////////////////////////////////////////////////////////////
// IDecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

void
IDecodingTask::Resume()
{
  DecodePool::Singleton()->AsyncRun(this);
}


///////////////////////////////////////////////////////////////////////////////
// DecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

DecodingTask::DecodingTask(NotNull<Decoder*> aDecoder)
  : mDecoder(aDecoder)
{
  MOZ_ASSERT(!mDecoder->IsMetadataDecode(),
             "Use MetadataDecodingTask for metadata decodes");
}

void
DecodingTask::Run()
{
  while (true) {
    LexerResult result = mDecoder->Decode(WrapNotNull(this));

    if (result.is<TerminalState>()) {
      NotifyDecodeComplete(mDecoder);
      return;  // We're done.
    }

    MOZ_ASSERT(result.is<Yield>());

    // Notify for the progress we've made so far.
    if (mDecoder->HasProgress()) {
      NotifyProgress(mDecoder);
    }

    if (result == LexerResult(Yield::NEED_MORE_DATA)) {
      // We can't make any more progress right now. The decoder itself will
      // ensure that we get reenqueued when more data is available; just return
      // for now.
      return;
    }

    // Right now we don't do anything special for other kinds of yields, so just
    // keep working.
  }
}

bool
DecodingTask::ShouldPreferSyncRun() const
{
  return mDecoder->ShouldSyncDecode(gfxPrefs::ImageMemDecodeBytesAtATime());
}


///////////////////////////////////////////////////////////////////////////////
// MetadataDecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

MetadataDecodingTask::MetadataDecodingTask(NotNull<Decoder*> aDecoder)
  : mDecoder(aDecoder)
{
  MOZ_ASSERT(mDecoder->IsMetadataDecode(),
             "Use DecodingTask for non-metadata decodes");
}

void
MetadataDecodingTask::Run()
{
  LexerResult result = mDecoder->Decode(WrapNotNull(this));

  if (result.is<TerminalState>()) {
    NotifyDecodeComplete(mDecoder);
    return;  // We're done.
  }

  if (result == LexerResult(Yield::NEED_MORE_DATA)) {
    // We can't make any more progress right now. We also don't want to report
    // any progress, because it's important that metadata decode results are
    // delivered atomically. The decoder itself will ensure that we get
    // reenqueued when more data is available; just return for now.
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Metadata decode yielded for an unexpected reason");
}


///////////////////////////////////////////////////////////////////////////////
// AnonymousDecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

AnonymousDecodingTask::AnonymousDecodingTask(NotNull<Decoder*> aDecoder)
  : mDecoder(aDecoder)
{ }

void
AnonymousDecodingTask::Run()
{
  while (true) {
    LexerResult result = mDecoder->Decode(WrapNotNull(this));

    if (result.is<TerminalState>()) {
      return;  // We're done.
    }

    if (result == LexerResult(Yield::NEED_MORE_DATA)) {
      // We can't make any more progress right now. Let the caller decide how to
      // handle it.
      return;
    }

    // Right now we don't do anything special for other kinds of yields, so just
    // keep working.
    MOZ_ASSERT(result.is<Yield>());
  }
}

} // namespace image
} // namespace mozilla
