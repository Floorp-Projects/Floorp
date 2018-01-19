/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/Range.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/InlineTranslator.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "WebRenderTypes.h"
#include "webrender_ffi.h"

#include <iostream>
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
  const uint8_t *mData;
  size_t mSize;
  uint32_t mIndex;
  const VecU8 *mVec;
  RefPtr<UnscaledFont> mUnscaledFont;
};

StaticMutex sFontDataTableLock;
std::unordered_map<FontKey, FontTemplate> sFontDataTable;

void
ClearBlobImageResources(WrIdNamespace aNamespace) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  for (auto i = sFontDataTable.begin(); i != sFontDataTable.end();) {
    if (i->first.mNamespace == aNamespace) {
      if (i->second.mVec) {
        wr_dec_ref_arc(i->second.mVec);
      }
      i = sFontDataTable.erase(i);
    } else {
      i++;
    }
  }
}

extern "C" {
void
AddFontData(WrFontKey aKey, const uint8_t *aData, size_t aSize, uint32_t aIndex, const ArcVecU8 *aVec) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(aKey);
  if (i == sFontDataTable.end()) {
    FontTemplate font;
    font.mData = aData;
    font.mSize = aSize;
    font.mIndex = aIndex;
    font.mVec = wr_add_ref_arc(aVec);
    sFontDataTable[aKey] = font;
  }
}

void
AddNativeFontHandle(WrFontKey aKey, void* aHandle, uint32_t aIndex) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(aKey);
  if (i == sFontDataTable.end()) {
    FontTemplate font;
    font.mData = nullptr;
    font.mSize = 0;
    font.mIndex = 0;
    font.mVec = nullptr;
#ifdef XP_MACOSX
    font.mUnscaledFont = new UnscaledFontMac(reinterpret_cast<CGFontRef>(aHandle), true);
#elif defined(XP_WIN)
    font.mUnscaledFont = new UnscaledFontDWrite(reinterpret_cast<IDWriteFontFace*>(aHandle), nullptr);
#elif defined(ANDROID)
    font.mUnscaledFont = new UnscaledFontFreeType(reinterpret_cast<const char*>(aHandle), aIndex);
#else
    font.mUnscaledFont = new UnscaledFontFontconfig(reinterpret_cast<const char*>(aHandle), aIndex);
#endif
    sFontDataTable[aKey] = font;
  }
}

void
DeleteFontData(WrFontKey aKey) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(aKey);
  if (i != sFontDataTable.end()) {
    if (i->second.mVec) {
      wr_dec_ref_arc(i->second.mVec);
    }
    sFontDataTable.erase(i);
  }
}
}

RefPtr<UnscaledFont>
GetUnscaledFont(Translator *aTranslator, wr::FontKey key) {
  StaticMutexAutoLock lock(sFontDataTableLock);
  auto i = sFontDataTable.find(key);
  if (i == sFontDataTable.end()) {
    gfxDevCrash(LogReason::UnscaledFontNotFound) << "Failed to get UnscaledFont entry for FontKey " << key.mHandle;
    return nullptr;
  }
  auto &data = i->second;
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
  };
  //XXX: Make safe
  size_t indexOffset = *(size_t*)(aBlob.end().get()-sizeof(size_t));
  Reader reader(aBlob.begin().get()+indexOffset, aBlob.length()-sizeof(size_t)-indexOffset);

  bool ret;
  size_t offset = 0;
  while (reader.pos < reader.len) {
    size_t end = reader.ReadSize();
    size_t extra_end = reader.ReadSize();

    gfx::InlineTranslator translator(dt);

    size_t count = *(size_t*)(aBlob.begin().get() + end);
    for (size_t i = 0; i < count; i++) {
      wr::FontKey key = *(wr::FontKey*)(aBlob.begin() + end + sizeof(count) + sizeof(wr::FontKey)*i).get();
      RefPtr<UnscaledFont> font = GetUnscaledFont(&translator, key);
      translator.AddUnscaledFont(0, font);
    }
    Range<const uint8_t> blob(aBlob.begin() + offset, aBlob.begin() + end);
    ret = translator.TranslateRecording((char*)blob.begin().get(), blob.length());
    offset = extra_end;
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
                        mozilla::wr::MutByteSlice output)
{
  return mozilla::wr::Moz2DRenderCallback(mozilla::wr::ByteSliceToRange(blob),
                                          mozilla::gfx::IntSize(width, height),
                                          mozilla::wr::ImageFormatToSurfaceFormat(aFormat),
                                          aTileSize,
                                          aTileOffset,
                                          mozilla::wr::MutByteSliceToRange(output));
}

} // extern


