/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnscaledFontFreeType.h"
#include "NativeFontResourceFreeType.h"
#include "ScaledFontFreeType.h"
#include "Logging.h"
#include "StackArray.h"

#include FT_MULTIPLE_MASTERS_H
#include FT_TRUETYPE_TABLES_H

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace mozilla::gfx {

bool UnscaledFontFreeType::GetFontFileData(FontFileDataOutput aDataCallback,
                                           void* aBaton) {
  if (!mFile.empty()) {
    int fd = open(mFile.c_str(), O_RDONLY);
    if (fd < 0) {
      return false;
    }
    struct stat buf;
    if (fstat(fd, &buf) < 0 ||
        // Don't erroneously read directories as files.
        !S_ISREG(buf.st_mode) ||
        // Verify the file size fits in a uint32_t.
        buf.st_size <= 0 || off_t(uint32_t(buf.st_size)) != buf.st_size) {
      close(fd);
      return false;
    }
    uint32_t length = buf.st_size;
    uint8_t* fontData = reinterpret_cast<uint8_t*>(
        mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (fontData == MAP_FAILED) {
      return false;
    }
    aDataCallback(fontData, length, mIndex, aBaton);
    munmap(fontData, length);
    return true;
  }

  bool success = false;
  FT_ULong length = 0;
  // Request the SFNT file. This may not always succeed for all font types.
  if (FT_Load_Sfnt_Table(mFace->GetFace(), 0, 0, nullptr, &length) ==
      FT_Err_Ok) {
    uint8_t* fontData = new uint8_t[length];
    if (FT_Load_Sfnt_Table(mFace->GetFace(), 0, 0, fontData, &length) ==
        FT_Err_Ok) {
      aDataCallback(fontData, length, 0, aBaton);
      success = true;
    }
    delete[] fontData;
  }
  return success;
}

bool UnscaledFontFreeType::GetFontDescriptor(FontDescriptorOutput aCb,
                                             void* aBaton) {
  if (mFile.empty()) {
    return false;
  }

  aCb(reinterpret_cast<const uint8_t*>(mFile.data()), mFile.size(), mIndex,
      aBaton);
  return true;
}

RefPtr<SharedFTFace> UnscaledFontFreeType::InitFace() {
  if (mFace) {
    return mFace;
  }
  if (mFile.empty()) {
    return nullptr;
  }
  mFace = Factory::NewSharedFTFace(nullptr, mFile.c_str(), mIndex);
  if (!mFace) {
    gfxWarning() << "Failed initializing FreeType face from filename";
    return nullptr;
  }
  return mFace;
}

void UnscaledFontFreeType::GetVariationSettingsFromFace(
    std::vector<FontVariation>* aVariations, FT_Face aFace) {
  if (!aFace || !(aFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) {
    return;
  }

  typedef FT_Error (*GetVarFunc)(FT_Face, FT_MM_Var**);
  typedef FT_Error (*DoneVarFunc)(FT_Library, FT_MM_Var*);
  typedef FT_Error (*GetVarDesignCoordsFunc)(FT_Face, FT_UInt, FT_Fixed*);
#if MOZ_TREE_FREETYPE
  GetVarFunc getVar = &FT_Get_MM_Var;
  DoneVarFunc doneVar = &FT_Done_MM_Var;
  GetVarDesignCoordsFunc getCoords = &FT_Get_Var_Design_Coordinates;
#else
  static GetVarFunc getVar;
  static DoneVarFunc doneVar;
  static GetVarDesignCoordsFunc getCoords;
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    getVar = (GetVarFunc)dlsym(RTLD_DEFAULT, "FT_Get_MM_Var");
    doneVar = (DoneVarFunc)dlsym(RTLD_DEFAULT, "FT_Done_MM_Var");
    getCoords = (GetVarDesignCoordsFunc)dlsym(RTLD_DEFAULT,
                                              "FT_Get_Var_Design_Coordinates");
  }
  if (!getVar || !getCoords) {
    return;
  }
#endif

  FT_MM_Var* mmVar = nullptr;
  if ((*getVar)(aFace, &mmVar) == FT_Err_Ok) {
    aVariations->reserve(mmVar->num_axis);
    StackArray<FT_Fixed, 32> coords(mmVar->num_axis);
    if ((*getCoords)(aFace, mmVar->num_axis, coords.data()) == FT_Err_Ok) {
      bool changed = false;
      for (uint32_t i = 0; i < mmVar->num_axis; i++) {
        if (coords[i] != mmVar->axis[i].def) {
          changed = true;
        }
        aVariations->push_back(FontVariation{uint32_t(mmVar->axis[i].tag),
                                             float(coords[i] / 65536.0)});
      }
      if (!changed) {
        aVariations->clear();
      }
    }
    if (doneVar) {
      (*doneVar)(aFace->glyph->library, mmVar);
    } else {
      free(mmVar);
    }
  }
}

void UnscaledFontFreeType::ApplyVariationsToFace(
    const FontVariation* aVariations, uint32_t aNumVariations, FT_Face aFace) {
  if (!aFace || !(aFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS)) {
    return;
  }

  typedef FT_Error (*SetVarDesignCoordsFunc)(FT_Face, FT_UInt, FT_Fixed*);
#ifdef MOZ_TREE_FREETYPE
  SetVarDesignCoordsFunc setCoords = &FT_Set_Var_Design_Coordinates;
#else
  typedef FT_Error (*SetVarDesignCoordsFunc)(FT_Face, FT_UInt, FT_Fixed*);
  static SetVarDesignCoordsFunc setCoords;
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    setCoords = (SetVarDesignCoordsFunc)dlsym(RTLD_DEFAULT,
                                              "FT_Set_Var_Design_Coordinates");
  }
  if (!setCoords) {
    return;
  }
#endif

  StackArray<FT_Fixed, 32> coords(aNumVariations);
  for (uint32_t i = 0; i < aNumVariations; i++) {
    coords[i] = std::round(aVariations[i].mValue * 65536.0f);
  }
  if ((*setCoords)(aFace, aNumVariations, coords.data()) != FT_Err_Ok) {
    // ignore the problem?
  }
}

}  // namespace mozilla::gfx
