/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs_gfx.h"
#include "gfxUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Range.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/RectAbsolute.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/layers/WebRenderDrawEventRecorder.h"
#include "WebRenderTypes.h"
#include "webrender_ffi.h"
#include "GeckoProfiler.h"

#include <unordered_map>

#ifdef XP_MACOSX
#  include "mozilla/gfx/UnscaledFontMac.h"
#elif defined(XP_WIN)
#  include "mozilla/gfx/UnscaledFontDWrite.h"
#else
#  include "mozilla/gfx/UnscaledFontFreeType.h"
#endif

namespace std {
template <>
struct hash<mozilla::wr::FontKey> {
  size_t operator()(const mozilla::wr::FontKey& key) const {
    return hash<size_t>()(mozilla::wr::AsUint64(key));
  }
};

template <>
struct hash<mozilla::wr::FontInstanceKey> {
  size_t operator()(const mozilla::wr::FontInstanceKey& key) const {
    return hash<size_t>()(mozilla::wr::AsUint64(key));
  }
};
};  // namespace std

namespace mozilla {

using namespace gfx;

namespace wr {

struct FontTemplate {
  const uint8_t* mData;
  size_t mSize;
  uint32_t mIndex;
  const VecU8* mVec;
  RefPtr<UnscaledFont> mUnscaledFont;

  FontTemplate() : mData(nullptr), mSize(0), mIndex(0), mVec(nullptr) {}

  ~FontTemplate() {
    if (mVec) {
      wr_dec_ref_arc(mVec);
    }
  }
};

struct FontInstanceData {
  WrFontKey mFontKey;
  float mSize;
  Maybe<FontInstanceOptions> mOptions;
  Maybe<FontInstancePlatformOptions> mPlatformOptions;
  UniquePtr<gfx::FontVariation[]> mVariations;
  size_t mNumVariations;
  RefPtr<ScaledFont> mScaledFont;

  FontInstanceData() : mSize(0), mNumVariations(0) {}
};

StaticMutex sFontDataTableLock;
std::unordered_map<WrFontKey, FontTemplate> sFontDataTable;
std::unordered_map<WrFontInstanceKey, FontInstanceData> sBlobFontTable;

// Fixed-size ring buffer logging font deletion events to aid debugging.
static struct FontDeleteLog {
  static const size_t MAX_ENTRIES = 256;

  uint64_t mEntries[MAX_ENTRIES] = {0};
  size_t mNextEntry = 0;

  void AddEntry(uint64_t aEntry) {
    mEntries[mNextEntry] = aEntry;
    mNextEntry = (mNextEntry + 1) % MAX_ENTRIES;
  }

  void Add(WrFontKey aKey) { AddEntry(AsUint64(aKey)); }

  // Store namespace clears as font id 0, since this will never be allocated.
  void Add(WrIdNamespace aNamespace) {
    AddEntry(AsUint64(WrFontKey{aNamespace, 0}));
  }

  void AddAll() { AddEntry(~0); }

  // Find a matching entry in the log, searching backwards starting at the
  // newest entry and finishing with the oldest entry. Returns a brief
  // description of why the font was deleted, if known.
  const char* Find(WrFontKey aKey) {
    uint64_t keyEntry = AsUint64(aKey);
    uint64_t namespaceEntry = AsUint64(WrFontKey{aKey.mNamespace, 0});
    size_t offset = mNextEntry;
    do {
      offset = (offset + MAX_ENTRIES - 1) % MAX_ENTRIES;
      if (mEntries[offset] == keyEntry) {
        return "deleted font";
      } else if (mEntries[offset] == namespaceEntry) {
        return "cleared namespace";
      } else if (mEntries[offset] == (uint64_t)~0) {
        return "cleared all";
      }
    } while (offset != mNextEntry);
    return "unknown font";
  }
} sFontDeleteLog;

void ClearAllBlobImageResources() {
  StaticMutexAutoLock lock(sFontDataTableLock);
  sFontDeleteLog.AddAll();
  sBlobFontTable.clear();
  sFontDataTable.clear();
}

extern "C" {
void ClearBlobImageResources(WrIdNamespace aNamespace) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  sFontDeleteLog.Add(aNamespace);
  for (auto i = sBlobFontTable.begin(); i != sBlobFontTable.end();) {
    if (i->first.mNamespace == aNamespace) {
      i = sBlobFontTable.erase(i);
    } else {
      i++;
    }
  }
  for (auto i = sFontDataTable.begin(); i != sFontDataTable.end();) {
    if (i->first.mNamespace == aNamespace) {
      i = sFontDataTable.erase(i);
    } else {
      i++;
    }
  }
}

bool HasFontData(WrFontKey aKey) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  return sFontDataTable.find(aKey) != sFontDataTable.end();
}

void AddFontData(WrFontKey aKey, const uint8_t* aData, size_t aSize,
                 uint32_t aIndex, const ArcVecU8* aVec) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(aKey);
  if (i == sFontDataTable.end()) {
    FontTemplate& font = sFontDataTable[aKey];
    font.mData = aData;
    font.mSize = aSize;
    font.mIndex = aIndex;
    font.mVec = wr_add_ref_arc(aVec);
  }
}

void AddNativeFontHandle(WrFontKey aKey, void* aHandle, uint32_t aIndex) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(aKey);
  if (i == sFontDataTable.end()) {
    FontTemplate& font = sFontDataTable[aKey];
#ifdef XP_MACOSX
    font.mUnscaledFont =
        new UnscaledFontMac(reinterpret_cast<CGFontRef>(aHandle), false);
#elif defined(XP_WIN)
    font.mUnscaledFont = new UnscaledFontDWrite(
        reinterpret_cast<IDWriteFontFace*>(aHandle), nullptr);
#elif defined(ANDROID)
    font.mUnscaledFont = new UnscaledFontFreeType(
        reinterpret_cast<const char*>(aHandle), aIndex);
#else
    font.mUnscaledFont = new UnscaledFontFontconfig(
        reinterpret_cast<const char*>(aHandle), aIndex);
#endif
  }
}

void DeleteFontData(WrFontKey aKey) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  sFontDeleteLog.Add(aKey);
  auto i = sFontDataTable.find(aKey);
  if (i != sFontDataTable.end()) {
    sFontDataTable.erase(i);
  }
}

void AddBlobFont(WrFontInstanceKey aInstanceKey, WrFontKey aFontKey,
                 float aSize, const FontInstanceOptions* aOptions,
                 const FontInstancePlatformOptions* aPlatformOptions,
                 const FontVariation* aVariations, size_t aNumVariations) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sBlobFontTable.find(aInstanceKey);
  if (i == sBlobFontTable.end()) {
    FontInstanceData& font = sBlobFontTable[aInstanceKey];
    font.mFontKey = aFontKey;
    font.mSize = aSize;
    if (aOptions) {
      font.mOptions = Some(*aOptions);
    }
    if (aPlatformOptions) {
      font.mPlatformOptions = Some(*aPlatformOptions);
    }
    if (aNumVariations) {
      font.mNumVariations = aNumVariations;
      font.mVariations.reset(new gfx::FontVariation[aNumVariations]);
      PodCopy(font.mVariations.get(),
              reinterpret_cast<const gfx::FontVariation*>(aVariations),
              aNumVariations);
    }
  }
}

void DeleteBlobFont(WrFontInstanceKey aKey) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sBlobFontTable.find(aKey);
  if (i != sBlobFontTable.end()) {
    sBlobFontTable.erase(i);
  }
}

}  // extern

static RefPtr<UnscaledFont> GetUnscaledFont(Translator* aTranslator,
                                            WrFontKey aKey) {
  auto i = sFontDataTable.find(aKey);
  if (i == sFontDataTable.end()) {
    gfxDevCrash(LogReason::UnscaledFontNotFound)
        << "Failed to get UnscaledFont entry for FontKey " << aKey.mHandle
        << " because " << sFontDeleteLog.Find(aKey);
    return nullptr;
  }
  FontTemplate& data = i->second;
  if (data.mUnscaledFont) {
    return data.mUnscaledFont;
  }
  MOZ_ASSERT(data.mData);
  FontType type =
#ifdef XP_MACOSX
      FontType::MAC;
#elif defined(XP_WIN)
      FontType::DWRITE;
#elif defined(ANDROID)
      FontType::FREETYPE;
#else
      FontType::FONTCONFIG;
#endif
  // makes a copy of the data
  RefPtr<NativeFontResource> fontResource = Factory::CreateNativeFontResource(
      (uint8_t*)data.mData, data.mSize, type, aTranslator->GetFontContext());
  RefPtr<UnscaledFont> unscaledFont;
  if (!fontResource) {
    gfxDevCrash(LogReason::NativeFontResourceNotFound)
        << "Failed to create NativeFontResource for FontKey " << aKey.mHandle;
  } else {
    // Instance data is only needed for GDI fonts which webrender does not
    // support.
    unscaledFont = fontResource->CreateUnscaledFont(data.mIndex, nullptr, 0);
    if (!unscaledFont) {
      gfxDevCrash(LogReason::UnscaledFontNotFound)
          << "Failed to create UnscaledFont for FontKey " << aKey.mHandle;
    }
  }
  data.mUnscaledFont = unscaledFont;
  return unscaledFont;
}

static RefPtr<ScaledFont> GetScaledFont(Translator* aTranslator,
                                        WrFontInstanceKey aKey) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sBlobFontTable.find(aKey);
  if (i == sBlobFontTable.end()) {
    gfxDevCrash(LogReason::ScaledFontNotFound)
        << "Failed to get ScaledFont entry for FontInstanceKey "
        << aKey.mHandle;
    return nullptr;
  }
  FontInstanceData& data = i->second;
  if (data.mScaledFont) {
    return data.mScaledFont;
  }
  RefPtr<UnscaledFont> unscaled = GetUnscaledFont(aTranslator, data.mFontKey);
  if (!unscaled) {
    return nullptr;
  }
  RefPtr<ScaledFont> scaled = unscaled->CreateScaledFontFromWRFont(
      data.mSize, data.mOptions.ptrOr(nullptr),
      data.mPlatformOptions.ptrOr(nullptr), data.mVariations.get(),
      data.mNumVariations);
  if (!scaled) {
    gfxDevCrash(LogReason::ScaledFontNotFound)
        << "Failed to create ScaledFont for FontKey " << aKey.mHandle;
  }
  data.mScaledFont = scaled;
  return data.mScaledFont;
}

template <typename T>
T ConvertFromBytes(const uint8_t* bytes) {
  T t;
  memcpy(&t, bytes, sizeof(T));
  return t;
}

struct Reader {
  const uint8_t* buf;
  size_t len;
  size_t pos;

  Reader(const uint8_t* buf, size_t len) : buf(buf), len(len), pos(0) {}

  template <typename T>
  T Read() {
    MOZ_RELEASE_ASSERT(pos + sizeof(T) <= len);
    T ret = ConvertFromBytes<T>(buf + pos);
    pos += sizeof(T);
    return ret;
  }

  size_t ReadSize() { return Read<size_t>(); }
  int ReadInt() { return Read<int>(); }

  IntRectAbsolute ReadBounds() { return Read<IntRectAbsolute>(); }

  layers::BlobFont ReadBlobFont() { return Read<layers::BlobFont>(); }
};

static bool Moz2DRenderCallback(const Range<const uint8_t> aBlob,
                                gfx::SurfaceFormat aFormat,
                                const mozilla::wr::DeviceIntRect* aVisibleRect,
                                const mozilla::wr::LayoutIntRect* aRenderRect,
                                const uint16_t aTileSize,
                                const mozilla::wr::TileOffset* aTileOffset,
                                const mozilla::wr::LayoutIntRect* aDirtyRect,
                                Range<uint8_t> aOutput) {
  IntSize size(aRenderRect->size.width, aRenderRect->size.height);
  AUTO_PROFILER_TRACING_MARKER("WebRender", "RasterizeSingleBlob", GRAPHICS);
  MOZ_RELEASE_ASSERT(size.width > 0 && size.height > 0);
  if (size.width <= 0 || size.height <= 0) {
    return false;
  }

  auto stride = size.width * gfx::BytesPerPixel(aFormat);

  if (aOutput.length() < static_cast<size_t>(size.height * stride)) {
    return false;
  }

  // In bindings.rs we allocate a buffer filled with opaque white.
  bool uninitialized = false;

  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
      gfx::BackendType::SKIA, aOutput.begin().get(), size, stride, aFormat,
      uninitialized);

  if (!dt) {
    return false;
  }

  // We try hard to not have empty blobs but we can end up with
  // them because of CompositorHitTestInfo and merging.
  size_t footerSize = sizeof(size_t);
  MOZ_RELEASE_ASSERT(aBlob.length() >= footerSize);
  size_t indexOffset = ConvertFromBytes<size_t>(aBlob.end().get() - footerSize);

  // aRenderRect is the part of the blob that we are currently rendering
  // (for example a tile) in the same coordinate space as aVisibleRect.
  IntPoint origin = gfx::IntPoint(aRenderRect->origin.x, aRenderRect->origin.y);

  MOZ_RELEASE_ASSERT(indexOffset <= aBlob.length() - footerSize);
  Reader reader(aBlob.begin().get() + indexOffset,
                aBlob.length() - footerSize - indexOffset);

  dt = gfx::Factory::CreateOffsetDrawTarget(dt, origin);

  auto bounds = gfx::IntRect(origin, size);

  if (aDirtyRect) {
    gfx::Rect dirty(aDirtyRect->origin.x, aDirtyRect->origin.y,
                    aDirtyRect->size.width, aDirtyRect->size.height);
    dt->PushClipRect(dirty);
    bounds = bounds.Intersect(
        IntRect(aDirtyRect->origin.x, aDirtyRect->origin.y,
                aDirtyRect->size.width, aDirtyRect->size.height));
  }

  bool ret = true;
  size_t offset = 0;
  auto absBounds = IntRectAbsolute::FromRect(bounds);
  while (reader.pos < reader.len) {
    size_t end = reader.ReadSize();
    size_t extra_end = reader.ReadSize();
    MOZ_RELEASE_ASSERT(extra_end >= end);
    MOZ_RELEASE_ASSERT(extra_end < aBlob.length());

    auto combinedBounds = absBounds.Intersect(reader.ReadBounds());
    if (combinedBounds.IsEmpty()) {
      offset = extra_end;
      continue;
    }

    layers::WebRenderTranslator translator(dt);
    Reader fontReader(aBlob.begin().get() + end, extra_end - end);
    size_t count = fontReader.ReadSize();
    for (size_t i = 0; i < count; i++) {
      layers::BlobFont blobFont = fontReader.ReadBlobFont();
      RefPtr<ScaledFont> scaledFont =
          GetScaledFont(&translator, blobFont.mFontInstanceKey);
      translator.AddScaledFont(blobFont.mScaledFontPtr, scaledFont);
    }

    Range<const uint8_t> blob(aBlob.begin() + offset, aBlob.begin() + end);
    ret =
        translator.TranslateRecording((char*)blob.begin().get(), blob.length());
    if (!ret) {
      gfxCriticalNote << "Replay failure: " << translator.GetError();
      MOZ_RELEASE_ASSERT(false);
    }
    offset = extra_end;
  }

  if (StaticPrefs::gfx_webrender_blob_paint_flashing()) {
    dt->SetTransform(gfx::Matrix());
    float r = float(rand()) / float(RAND_MAX);
    float g = float(rand()) / float(RAND_MAX);
    float b = float(rand()) / float(RAND_MAX);
    dt->FillRect(gfx::Rect(origin.x, origin.y, size.width, size.height),
                 gfx::ColorPattern(gfx::DeviceColor(r, g, b, 0.5)));
  }

  if (aDirtyRect) {
    dt->PopClip();
  }

#if 0
  static int i = 0;
  char filename[40];
  sprintf(filename, "out%d.png", i++);
  gfxUtils::WriteAsPNG(dt, filename);
#endif

  return ret;
}

}  // namespace wr
}  // namespace mozilla

extern "C" {

bool wr_moz2d_render_cb(const mozilla::wr::ByteSlice blob,
                        mozilla::wr::ImageFormat aFormat,
                        const mozilla::wr::LayoutIntRect* aRenderRect,
                        const mozilla::wr::DeviceIntRect* aVisibleRect,
                        const uint16_t aTileSize,
                        const mozilla::wr::TileOffset* aTileOffset,
                        const mozilla::wr::LayoutIntRect* aDirtyRect,
                        mozilla::wr::MutByteSlice output) {
  return mozilla::wr::Moz2DRenderCallback(
      mozilla::wr::ByteSliceToRange(blob),
      mozilla::wr::ImageFormatToSurfaceFormat(aFormat), aVisibleRect,
      aRenderRect, aTileSize, aTileOffset, aDirtyRect,
      mozilla::wr::MutByteSliceToRange(output));
}

}  // extern
