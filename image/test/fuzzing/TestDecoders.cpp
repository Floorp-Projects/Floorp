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
#include "mozilla/Preferences.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#include "FuzzingInterfaceStream.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

// Prevents x being optimized away if it has no side-effects.
// If optimized away, tools like ASan wouldn't be able to detect
// faulty memory accesses.
#define DUMMY_IF(x) \
  if (x) {          \
    volatile int v; \
    v = 0;          \
  }

class DecodeToSurfaceRunnableFuzzing : public Runnable {
 public:
  DecodeToSurfaceRunnableFuzzing(RefPtr<SourceSurface>& aSurface,
                                 nsIInputStream* aInputStream,
                                 const char* mimeType)
      : mozilla::Runnable("DecodeToSurfaceRunnableFuzzing"),
        mSurface(aSurface),
        mInputStream(aInputStream),
        mMimeType(mimeType) {}

  NS_IMETHOD Run() override {
    Go();
    return NS_OK;
  }

  void Go() {
    mSurface = ImageOps::DecodeToSurface(mInputStream.forget(), mMimeType,
                                         imgIContainer::DECODE_FLAGS_DEFAULT);
    if (!mSurface) return;

    if (mSurface->GetType() == SurfaceType::DATA) {
      if (mSurface->GetFormat() == SurfaceFormat::OS_RGBX ||
          mSurface->GetFormat() == SurfaceFormat::OS_RGBA) {
        DUMMY_IF(IntSize(1, 1) == mSurface->GetSize());
        DUMMY_IF(IsSolidColor(mSurface, BGRAColor::Green(), 1));
      }
    }
  }

 private:
  RefPtr<SourceSurface>& mSurface;
  nsCOMPtr<nsIInputStream> mInputStream;
  nsAutoCString mMimeType;
};

static int RunDecodeToSurfaceFuzzing(nsCOMPtr<nsIInputStream> inputStream,
                                     const char* mimeType) {
  uint64_t len;
  inputStream->Available(&len);
  if (len <= 0) {
    return 0;
  }

  // Ensure CMS state is initialized on the main thread.
  gfxPlatform::GetCMSMode();

  nsCOMPtr<nsIThread> thread;
  nsresult rv =
      NS_NewNamedThread("Decoder Test", getter_AddRefs(thread), nullptr);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  // We run the DecodeToSurface tests off-main-thread to ensure that
  // DecodeToSurface doesn't require any other main-thread-only code.
  RefPtr<SourceSurface> surface;
  nsCOMPtr<nsIRunnable> runnable =
      new DecodeToSurfaceRunnableFuzzing(surface, inputStream, mimeType);
  thread->Dispatch(runnable, nsIThread::DISPATCH_SYNC);

  thread->Shutdown();

  // Explicitly release the SourceSurface on the main thread.
  surface = nullptr;

  return 0;
}

static int RunDecodeToSurfaceFuzzingJPEG(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/jpeg");
}

static int RunDecodeToSurfaceFuzzingGIF(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/gif");
}

static int RunDecodeToSurfaceFuzzingICO(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/ico");
}

static int RunDecodeToSurfaceFuzzingBMP(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/bmp");
}

static int RunDecodeToSurfaceFuzzingPNG(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/png");
}

static int RunDecodeToSurfaceFuzzingWebP(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/webp");
}

static int RunDecodeToSurfaceFuzzingAVIF(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/avif");
}

static int RunDecodeToSurfaceFuzzingJXL(nsCOMPtr<nsIInputStream> inputStream) {
  return RunDecodeToSurfaceFuzzing(inputStream, "image/jxl");
}

int FuzzingInitImage(int* argc, char*** argv) {
  Preferences::SetBool("image.avif.enabled", true);
  Preferences::SetBool("image.jxl.enabled", true);

  nsCOMPtr<imgITools> imgTools =
      do_CreateInstance("@mozilla.org/image/tools;1");
  if (imgTools == nullptr) {
    std::cerr << "Initializing image tools failed" << std::endl;
    return 1;
  }

  return 0;
}

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingJPEG,
                             ImageJPEG);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingGIF,
                             ImageGIF);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingICO,
                             ImageICO);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingBMP,
                             ImageBMP);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingPNG,
                             ImagePNG);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingWebP,
                             ImageWebP);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingAVIF,
                             ImageAVIF);

MOZ_FUZZING_INTERFACE_STREAM(FuzzingInitImage, RunDecodeToSurfaceFuzzingJXL,
                             ImageJXL);
