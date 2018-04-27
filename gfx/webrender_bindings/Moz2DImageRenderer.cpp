/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Range.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "mozilla/layers/WebRenderDrawEventRecorder.h"
#include "WebRenderTypes.h"
#include "webrender_ffi.h"

#include <unordered_map>

#ifdef XP_MACOSX
#include "mozilla/gfx/UnscaledFontMac.h"
#elif defined(XP_WIN)
#include "mozilla/gfx/UnscaledFontDWrite.h"
#else
#include "mozilla/gfx/UnscaledFontFreeType.h"
#endif

namespace std {
  template <>
    struct hash<mozilla::wr::FontKey>{
      public :
        size_t operator()(const mozilla::wr::FontKey &key ) const
        {
          return hash<size_t>()(mozilla::wr::AsUint64(key));
        }
    };
};



namespace mozilla {

using namespace gfx;

namespace wr {

struct FontTemplate {
  const uint8_t* mData;
  size_t mSize;
  uint32_t mIndex;
  const VecU8* mVec;
  RefPtr<UnscaledFont> mUnscaledFont;

  FontTemplate()
    : mData(nullptr)
    , mSize(0)
    , mIndex(0)
    , mVec(nullptr)
  {}

  ~FontTemplate() {
    if (mVec) {
      wr_dec_ref_arc(mVec);
    }
  }
};

StaticMutex sFontDataTableLock;
std::unordered_map<FontKey, FontTemplate> sFontDataTable;

// Fixed-size ring buffer logging font deletion events to aid debugging.
static struct FontDeleteLog {
  static const size_t MAX_ENTRIES = 256;

  uint64_t mEntries[MAX_ENTRIES] = { 0 };
  size_t mNextEntry = 0;

  void AddEntry(uint64_t aEntry) {
    mEntries[mNextEntry] = aEntry;
    mNextEntry = (mNextEntry + 1) % MAX_ENTRIES;
  }

  void Add(WrFontKey aKey) {
    AddEntry(AsUint64(aKey));
  }

  // Store namespace clears as font id 0, since this will never be allocated.
  void Add(WrIdNamespace aNamespace) {
    AddEntry(AsUint64(WrFontKey { aNamespace, 0 }));
  }

  void AddAll() {
    AddEntry(~0);
  }

  // Find a matching entry in the log, searching backwards starting at the newest
  // entry and finishing with the oldest entry. Returns a brief description of why
  // the font was deleted, if known.
  const char* Find(WrFontKey aKey) {
    uint64_t keyEntry = AsUint64(aKey);
    uint64_t namespaceEntry = AsUint64(WrFontKey { aKey.mNamespace, 0 });
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

void
ClearAllBlobImageResources() {
  StaticMutexAutoLock lock(sFontDataTableLock);
  sFontDeleteLog.AddAll();
  sFontDataTable.clear();
}

extern "C" {
void
ClearBlobImageResources(WrIdNamespace aNamespace) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  sFontDeleteLog.Add(aNamespace);
  for (auto i = sFontDataTable.begin(); i != sFontDataTable.end();) {
    if (i->first.mNamespace == aNamespace) {
      i = sFontDataTable.erase(i);
    } else {
      i++;
    }
  }
}

void
AddFontData(WrFontKey aKey, const uint8_t *aData, size_t aSize, uint32_t aIndex, const ArcVecU8 *aVec) {
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

void
AddNativeFontHandle(WrFontKey aKey, void* aHandle, uint32_t aIndex) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(aKey);
  if (i == sFontDataTable.end()) {
    FontTemplate& font = sFontDataTable[aKey];
#ifdef XP_MACOSX
    font.mUnscaledFont = new UnscaledFontMac(reinterpret_cast<CGFontRef>(aHandle), true);
#elif defined(XP_WIN)
    font.mUnscaledFont = new UnscaledFontDWrite(reinterpret_cast<IDWriteFontFace*>(aHandle), nullptr);
#elif defined(ANDROID)
    font.mUnscaledFont = new UnscaledFontFreeType(reinterpret_cast<const char*>(aHandle), aIndex);
#else
    font.mUnscaledFont = new UnscaledFontFontconfig(reinterpret_cast<const char*>(aHandle), aIndex);
#endif
  }
}

void
DeleteFontData(WrFontKey aKey) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  sFontDeleteLog.Add(aKey);
  auto i = sFontDataTable.find(aKey);
  if (i != sFontDataTable.end()) {
    sFontDataTable.erase(i);
  }
}
}

RefPtr<UnscaledFont>
GetUnscaledFont(Translator *aTranslator, wr::FontKey key) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(key);
  if (i == sFontDataTable.end()) {
    gfxDevCrash(LogReason::UnscaledFontNotFound) << "Failed to get UnscaledFont entry for FontKey " << key.mHandle
                                                 << " because " << sFontDeleteLog.Find(key);
    return nullptr;
  }
  FontTemplate &data = i->second;
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
  RefPtr<NativeFontResource> fontResource = Factory::CreateNativeFontResource((uint8_t*)data.mData, data.mSize,
                                                                              aTranslator->GetReferenceDrawTarget()->GetBackendType(),
                                                                              type,
                                                                              aTranslator->GetFontContext());
  RefPtr<UnscaledFont> unscaledFont;
  if (!fontResource) {
    gfxDevCrash(LogReason::NativeFontResourceNotFound) << "Failed to create NativeFontResource for FontKey " << key.mHandle;
  } else {
    // Instance data is only needed for GDI fonts which webrender does not
    // support.
    unscaledFont = fontResource->CreateUnscaledFont(data.mIndex, nullptr, 0);
    if (!unscaledFont) {
      gfxDevCrash(LogReason::UnscaledFontNotFound) << "Failed to create UnscaledFont for FontKey " << key.mHandle;
    }
  }
  data.mUnscaledFont = unscaledFont;
  return unscaledFont;
}

static bool Moz2DRenderCallback(const Range<const uint8_t> aBlob,
                                gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat,
                                const uint16_t *aTileSize,
                                const mozilla::wr::TileOffset *aTileOffset,
                                const mozilla::wr::DeviceUintRect *aDirtyRect,
                                Range<uint8_t> aOutput)
{
  MOZ_ASSERT(aSize.width > 0 && aSize.height > 0);
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }

  auto stride = aSize.width * gfx::BytesPerPixel(aFormat);

  if (aOutput.length() < static_cast<size_t>(aSize.height * stride)) {
    return false;
  }

  // In bindings.rs we allocate a buffer filled with opaque white.
  bool uninitialized = false;

  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
    gfx::BackendType::SKIA,
    aOutput.begin().get(),
    aSize,
    stride,
    aFormat,
    uninitialized
  );

  if (!dt) {
    return false;
  }

  if (aTileOffset) {
    // It's overkill to use a TiledDrawTarget for a single tile
    // but it was the easiest way to get the offset handling working
    gfx::TileSet tileset;
    gfx::Tile tile;
    tile.mDrawTarget = dt;
    tile.mTileOrigin = gfx::IntPoint(aTileOffset->x * *aTileSize, aTileOffset->y * *aTileSize);
    tileset.mTiles = &tile;
    tileset.mTileCount = 1;
    dt = gfx::Factory::CreateTiledDrawTarget(tileset);
  }

  if (aDirtyRect) {
    Rect dirty(aDirtyRect->origin.x, aDirtyRect->origin.y, aDirtyRect->size.width, aDirtyRect->size.height);
    dt->PushClipRect(dirty);
  }

  struct Reader {
    const uint8_t *buf;
    size_t len;
    size_t pos;

    Reader(const uint8_t *buf, size_t len) : buf(buf), len(len), pos(0) {}

    size_t ReadSize() {
      size_t ret;
      MOZ_RELEASE_ASSERT(pos + sizeof(ret) <= len);
      memcpy(&ret, buf + pos, sizeof(ret));
      pos += sizeof(ret);
      return ret;
    }
    int ReadInt() {
      int ret;
      MOZ_RELEASE_ASSERT(pos + sizeof(ret) <= len);
      memcpy(&ret, buf + pos, sizeof(ret));
      pos += sizeof(ret);
      return ret;
    }

    void SkipBounds() {
      MOZ_RELEASE_ASSERT(pos + sizeof(int) * 4 <= len);
      pos += sizeof(int) * 4;
    }

  };
  //XXX: Make safe
  size_t indexOffset = *(size_t*)(aBlob.end().get()-sizeof(size_t));
  Reader reader(aBlob.begin().get()+indexOffset, aBlob.length()-sizeof(size_t)-indexOffset);

  bool ret;
  size_t offset = 0;
  while (reader.pos < reader.len) {
    size_t end = reader.ReadSize();
    size_t extra_end = reader.ReadSize();
    reader.SkipBounds();

    layers::WebRenderTranslator translator(dt);

    size_t count = *(size_t*)(aBlob.begin().get() + end);
    for (size_t i = 0; i < count; i++) {
      wr::FontKey key = *(wr::FontKey*)(aBlob.begin() + end + sizeof(count) + sizeof(wr::FontKey)*i).get();
      RefPtr<UnscaledFont> font = GetUnscaledFont(&translator, key);
      translator.AddUnscaledFont(0, font);
    }
    Range<const uint8_t> blob(aBlob.begin() + offset, aBlob.begin() + end);
    ret = translator.TranslateRecording((char*)blob.begin().get(), blob.length());
    MOZ_RELEASE_ASSERT(ret);
    offset = extra_end;
  }

#if 0
  dt->SetTransform(gfx::Matrix());
  float r = float(rand()) / RAND_MAX;
  float g = float(rand()) / RAND_MAX;
  float b = float(rand()) / RAND_MAX;
  dt->FillRect(gfx::Rect(0, 0, aSize.width, aSize.height), gfx::ColorPattern(gfx::Color(r, g, b, 0.5)));
#endif

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

} // namespace
} // namespace

extern "C" {

bool wr_moz2d_render_cb(const mozilla::wr::ByteSlice blob,
                        uint32_t width, uint32_t height,
                        mozilla::wr::ImageFormat aFormat,
                        const uint16_t *aTileSize,
                        const mozilla::wr::TileOffset *aTileOffset,
                        const mozilla::wr::DeviceUintRect *aDirtyRect,
                        mozilla::wr::MutByteSlice output)
{
  return mozilla::wr::Moz2DRenderCallback(mozilla::wr::ByteSliceToRange(blob),
                                          mozilla::gfx::IntSize(width, height),
                                          mozilla::wr::ImageFormatToSurfaceFormat(aFormat),
                                          aTileSize,
                                          aTileOffset,
                                          aDirtyRect,
                                          mozilla::wr::MutByteSliceToRange(output));
}

} // extern


