/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#include "RasterImage.h"

/* We end up pulling in windows.h because we eventually hit gfxWindowsSurface;
 * windows.h defines LoadImage, so we have to #undef it or imgLoader::LoadImage
 * gets changed.
 * This #undef needs to be in multiple places because we don't always pull
 * headers in in the same order.
 */
#undef LoadImage

#include "imgLoader.h"
#include "imgRequest.h"
#include "imgRequestProxy.h"
#include "imgTools.h"
#include "DiscardTracker.h"

#include "nsICOEncoder.h"
#include "nsPNGEncoder.h"
#include "nsJPEGEncoder.h"
#include "nsBMPEncoder.h"

// objects that just require generic constructors
namespace mozilla {
namespace image {
NS_GENERIC_FACTORY_CONSTRUCTOR(RasterImage)
}
}
using namespace mozilla::image;

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
NS_DEFINE_NAMED_CID(NS_RASTERIMAGE_CID);
NS_DEFINE_NAMED_CID(NS_ICOENCODER_CID);
NS_DEFINE_NAMED_CID(NS_JPEGENCODER_CID);
NS_DEFINE_NAMED_CID(NS_PNGENCODER_CID);
NS_DEFINE_NAMED_CID(NS_BMPENCODER_CID);

static const mozilla::Module::CIDEntry kImageCIDs[] = {
  { &kNS_IMGLOADER_CID, false, NULL, imgLoaderConstructor, },
  { &kNS_IMGREQUESTPROXY_CID, false, NULL, imgRequestProxyConstructor, },
  { &kNS_IMGTOOLS_CID, false, NULL, imgToolsConstructor, },
  { &kNS_RASTERIMAGE_CID, false, NULL, RasterImageConstructor, },
  { &kNS_ICOENCODER_CID, false, NULL, nsICOEncoderConstructor, },
  { &kNS_JPEGENCODER_CID, false, NULL, nsJPEGEncoderConstructor, },
  { &kNS_PNGENCODER_CID, false, NULL, nsPNGEncoderConstructor, },
  { &kNS_BMPENCODER_CID, false, NULL, nsBMPEncoderConstructor, },
  { NULL }
};

static const mozilla::Module::ContractIDEntry kImageContracts[] = {
  { "@mozilla.org/image/cache;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/loader;1", &kNS_IMGLOADER_CID },
  { "@mozilla.org/image/request;1", &kNS_IMGREQUESTPROXY_CID },
  { "@mozilla.org/image/tools;1", &kNS_IMGTOOLS_CID },
  { "@mozilla.org/image/rasterimage;1", &kNS_RASTERIMAGE_CID },
  { "@mozilla.org/image/encoder;2?type=image/vnd.microsoft.icon", &kNS_ICOENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=image/jpeg", &kNS_JPEGENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=image/png", &kNS_PNGENCODER_CID },
  { "@mozilla.org/image/encoder;2?type=image/bmp", &kNS_BMPENCODER_CID },
  { NULL }
};

static const mozilla::Module::CategoryEntry kImageCategories[] = {
  { "Gecko-Content-Viewers", "image/gif", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/jpeg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/pjpeg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/jpg", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/vnd.microsoft.icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/bmp", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-ms-bmp", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/icon", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/png", "@mozilla.org/content/document-loader-factory;1" },
  { "Gecko-Content-Viewers", "image/x-png", "@mozilla.org/content/document-loader-factory;1" },
  { "content-sniffing-services", "@mozilla.org/image/loader;1", "@mozilla.org/image/loader;1" },
  { NULL }
};

static nsresult
imglib_Initialize()
{
  mozilla::image::DiscardTracker::Initialize();
  imgLoader::InitCache();
  return NS_OK;
}

static void
imglib_Shutdown()
{
  imgLoader::Shutdown();
  mozilla::image::DiscardTracker::Shutdown();
}

static const mozilla::Module kImageModule = {
  mozilla::Module::kVersion,
  kImageCIDs,
  kImageContracts,
  kImageCategories,
  NULL,
  imglib_Initialize,
  imglib_Shutdown
};

NSMODULE_DEFN(nsImageLib2Module) = &kImageModule;
