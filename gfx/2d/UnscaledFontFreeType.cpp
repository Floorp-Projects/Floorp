/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnscaledFontFreeType.h"

#include FT_TRUETYPE_TABLES_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

namespace mozilla {
namespace gfx {

bool
UnscaledFontFreeType::GetFontFileData(FontFileDataOutput aDataCallback, void* aBaton)
{
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
        buf.st_size <= 0 ||
        off_t(uint32_t(buf.st_size)) != buf.st_size) {
      close(fd);
      return false;
    }
    uint32_t length = buf.st_size;
    uint8_t* fontData =
      reinterpret_cast<uint8_t*>(
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
  if (FT_Load_Sfnt_Table(mFace, 0, 0, nullptr, &length) == FT_Err_Ok) {
    uint8_t* fontData = new uint8_t[length];
    if (FT_Load_Sfnt_Table(mFace, 0, 0, fontData, &length) == FT_Err_Ok) {
      aDataCallback(fontData, length, 0, aBaton);
      success = true;
    }
    delete[] fontData;
  }
  return success;
}

bool
UnscaledFontFreeType::GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton)
{
  if (mFile.empty()) {
    return false;
  }

  const char* path = mFile.c_str();
  size_t pathLength = strlen(path) + 1;
  size_t dataLength = sizeof(FontDescriptor) + pathLength;
  uint8_t* data = new uint8_t[dataLength];
  FontDescriptor* desc = reinterpret_cast<FontDescriptor*>(data);
  desc->mPathLength = pathLength;
  desc->mIndex = mIndex;
  memcpy(data + sizeof(FontDescriptor), path, pathLength);

  aCb(data, dataLength, aBaton);
  delete[] data;
  return true;
}

} // namespace gfx
} // namespace mozilla

