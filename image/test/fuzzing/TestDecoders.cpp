/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "imgIContainer.h"
#include "imgITools.h"
#include "ImageOps.h"
#include "mozilla/gfx/2D.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIStringStream.h"
#include "nsIThread.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#include "FuzzingInterfaceStream.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

static std::string mimeType = "";

// Prevents x being optimized away if it has no side-effects.
// If optimized away, tools like ASan wouldn't be able to detect
// faulty memory accesses.
#define DUMMY_IF(x) if (x) { volatile int v; v=0; }

class DecodeToSurfaceRunnableFuzzing : public Runnable
{
public:
  DecodeToSurfaceRunnableFuzzing(RefPtr<SourceSurface>& aSurface,
                          nsIInputStream* aInputStream)
    : mozilla::Runnable("DecodeToSurfaceRunnableFuzzing")
    , mSurface(aSurface)
    , mInputStream(aInputStream)
  { }

  NS_IMETHOD Run() override
  {
    Go();
    return NS_OK;
  }

  void Go()
  {
    mSurface =
      ImageOps::DecodeToSurface(mInputStream.forget(),
                                nsDependentCString(mimeType.c_str()),
                                imgIContainer::DECODE_FLAGS_DEFAULT);
    if (!mSurface)
      return;

    if (mSurface->GetType() == SurfaceType::DATA) {
      if (mSurface->GetFormat() == SurfaceFormat::B8G8R8X8 ||
          mSurface->GetFormat() == SurfaceFormat::B8G8R8A8) {
        DUMMY_IF(IntSize(1,1) == mSurface->GetSize());
        DUMMY_IF(IsSolidColor(mSurface, BGRAColor::Green(), 1));
      }
    }
  }

private:
  RefPtr<SourceSurface>& mSurface;
  nsCOMPtr<nsIInputStream> mInputStream;
};

static int
RunDecodeToSurfaceFuzzing(nsCOMPtr<nsIInputStream> inputStream)
{
  uint64_t len;
  inputStream->Available(&len);
  if (len <= 0) {
      return 0;
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewThread(getter_AddRefs(thread), nullptr);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  // We run the DecodeToSurface tests off-main-thread to ensure that
  // DecodeToSurface doesn't require any main-thread-only code.
  RefPtr<SourceSurface> surface;
  nsCOMPtr<nsIRunnable> runnable =
    new DecodeToSurfaceRunnableFuzzing(surface, inputStream);
  thread->Dispatch(runnable, nsIThread::DISPATCH_SYNC);

  thread->Shutdown();

  // Explicitly release the SourceSurface on the main thread.
  surface = nullptr;

  return 0;
}

int FuzzingInitImage(int *argc, char ***argv) {
    nsCOMPtr<imgITools> imgTools =
      do_CreateInstance("@mozilla.org/image/tools;1");
    if (imgTools == nullptr) {
      std::cerr << "Initializing image tools failed" << std::endl;
      return 1;
    }

    char* mimeTypePtr = getenv("MOZ_FUZZ_IMG_MIMETYPE");
    if (!mimeTypePtr) {
      std::cerr << "Must specify mime-type in MOZ_FUZZ_IMG_MIMETYPE environment variable." << std::endl;
      return 1;
    }

    mimeType = std::string(mimeTypePtr);
    int ret = strncmp(mimeType.c_str(), "image/", strlen("image/"));

    if (ret) {
      std::cerr << "MOZ_FUZZ_IMG_MIMETYPE should start with 'image/', e.g. 'image/gif'. Return: " << ret << std::endl;
      return 1;
    }
    return 0;
}

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzing, Image);
