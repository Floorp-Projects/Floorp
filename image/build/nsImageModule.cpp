/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsImageModule.h"

#include "mozilla/ModuleUtils.h"
#include "nsMimeTypes.h"

#include "DecodePool.h"
#include "ImageFactory.h"
#include "ShutdownTracker.h"
#include "SurfaceCache.h"
#include "SurfacePipe.h"

#include "gfxPrefs.h"
#include "imgLoader.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"
#include "imgTools.h"

#include "nsICOEncoder.h"
#include "nsPNGEncoder.h"
#include "nsJPEGEncoder.h"
#include "nsBMPEncoder.h"

// objects that just require generic constructors
using namespace mozilla::image;

// XXX We would like to get rid of the imgLoader factory constructor.  See the
// comment documenting the imgLoader constructor.
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(imgLoader, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgRequestProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(imgTools)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsICOEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJPEGEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPNGEncoder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBMPEncoder)
NS_DEFINE_NAMED_CID(NS_IMGLOADER_CID);
NS_DEFINE_NAMED_CID(NS_IMGREQUESTPROXY_CID);
NS_DEFINE_NAMED_CID(NS_IMGTOOLS_CID);
NS_DEFINE_NAMED_CID(NS_ICOENCODER_CID);
NS_DEFINE_NAMED_CID(NS_JPEGENCODER_CID);
NS_DEFINE_NAMED_CID(NS_PNGENCODER_CID);
NS_DEFINE_NAMED_CID(NS_BMPENCODER_CID);

static const mozilla::Module::CIDEntry kImageCIDs[] = {
  { &kNS_IMGLOADER_CID, false, nullptr, imgLoaderConstructor, },
  { &kNS_IMGREQUESTPROXY_CID, false, nullptr, imgRequestProxyConstructor, },
  { &kNS_IMGTOOLS_CID, false, nullptr, imgToolsConstructor, },
  { &kNS_ICOENCODER_CID, false, nullptr, nsICOEncoderConstructor, },
  { &kNS_JPEGENCODER_CID, false, nullptr, nsJPEGEncoderConstructor, },
  { &kNS_PNGENCODER_CID, false, nullptr, nsPNGEncoderConstructor, },
  { &kNS_BMPENCODER_CID, false, nullptr, nsBMPEncoderConstructor, },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kImageContracts[] = {
  { "@mozilla.org/image/cache;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/loader;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/request;1", &kNS_IMGREQUESTPROXY_CID },
  { "@mozilla.org/image/tools;1", &kNS_IMGTOOLS_CID },
  { "@mozilla.org/image/encoder;2?type=" IMAGE_ICO_MS, &kNS_ICOENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=" IMAGE_JPEG, &kNS_JPEGENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=" IMAGE_PNG, &kNS_PNGENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=" IMAGE_BMP, &kNS_BMPENCODER_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kImageCategories[] = {
  { "Gecko-Content-Viewers", IMAGE_GIF, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_JPEG, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_PJPEG, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_JPG, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_ICO, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_ICO_MS, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_BMP, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_BMP_MS, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_ICON_MS, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_PNG, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_APNG, "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", IMAGE_X_PNG, "@mozilla.org/content/document-loader-factory;1" },
  { "content-sniffing-services", "@mozilla.org/image/loader;1", "@mozilla.org/image/loader;1" },
  { nullptr }
};

static bool sInitialized = false;
nsresult
mozilla::image::EnsureModuleInitialized()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInitialized) {
    return NS_OK;
  }

  // Make sure the preferences are initialized
  gfxPrefs::GetSingleton();

  mozilla::image::ShutdownTracker::Initialize();
  mozilla::image::ImageFactory::Initialize();
  mozilla::image::DecodePool::Initialize();
  mozilla::image::SurfaceCache::Initialize();
  mozilla::image::SurfacePipe::Initialize();
  imgLoader::GlobalInit();
  sInitialized = true;
  return NS_OK;
}

void
mozilla::image::ShutdownModule()
{
  if (!sInitialized) {
    return;
  }
  imgLoader::Shutdown();
  mozilla::image::SurfaceCache::Shutdown();
  sInitialized = false;
}

static const mozilla::Module kImageModule = {
  mozilla::Module::kVersion,
  kImageCIDs,
  kImageContracts,
  kImageCategories,
  nullptr,
  mozilla::image::EnsureModuleInitialized,
  // We need to be careful about shutdown ordering to avoid intermittent crashes
  // when hashtable enumeration decides to destroy modules in an unfortunate
  // order. So our shutdown is invoked explicitly during layout module shutdown.
  nullptr
};

NSMODULE_DEFN(nsImageLib2Module) = &kImageModule;
