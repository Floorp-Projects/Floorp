// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/data_pack.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_piece.h"

// For details of the file layout, see
// http://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings

namespace {
static const uint32_t kFileFormatVersion = 1;
// Length of file header: version and entry count.
static const size_t kHeaderLength = 2 * sizeof(uint32_t);

struct DataPackEntry {
  uint32_t resource_id;
  uint32_t file_offset;
  uint32_t length;

  static int CompareById(const void* void_key, const void* void_entry) {
    uint32_t key = *reinterpret_cast<const uint32_t*>(void_key);
    const DataPackEntry* entry =
        reinterpret_cast<const DataPackEntry*>(void_entry);
    if (key < entry->resource_id) {
      return -1;
    } else if (key > entry->resource_id) {
      return 1;
    } else {
      return 0;
    }
  }
}  __attribute((packed));

}  // anonymous namespace

namespace base {

// In .cc for MemoryMappedFile dtor.
DataPack::DataPack() : resource_count_(0) {
}
DataPack::~DataPack() {
}

bool DataPack::Load(const FilePath& path) {
  mmap_.reset(new file_util::MemoryMappedFile);
  if (!mmap_->Initialize(path)) {
    mmap_.reset();
    return false;
  }

  // Parse the header of the file.
  // First uint32_t: version; second: resource count.
  const uint32_t* ptr = reinterpret_cast<const uint32_t*>(mmap_->data());
  uint32_t version = ptr[0];
  if (version != kFileFormatVersion) {
    LOG(ERROR) << "Bad data pack version: got " << version << ", expected "
               << kFileFormatVersion;
    mmap_.reset();
    return false;
  }
  resource_count_ = ptr[1];

  // Sanity check the file.
  // 1) Check we have enough entries.
  if (kHeaderLength + resource_count_ * sizeof(DataPackEntry) >
      mmap_->length()) {
    LOG(ERROR) << "Data pack file corruption: too short for number of "
                  "entries specified.";
    mmap_.reset();
    return false;
  }
  // 2) Verify the entries are within the appropriate bounds.
  for (size_t i = 0; i < resource_count_; ++i) {
    const DataPackEntry* entry = reinterpret_cast<const DataPackEntry*>(
        mmap_->data() + kHeaderLength + (i * sizeof(DataPackEntry)));
    if (entry->file_offset + entry->length > mmap_->length()) {
      LOG(ERROR) << "Entry #" << i << " in data pack points off end of file. "
                 << "Was the file corrupted?";
      mmap_.reset();
      return false;
    }
  }

  return true;
}

bool DataPack::Get(uint32_t resource_id, StringPiece* data) {
  // It won't be hard to make this endian-agnostic, but it's not worth
  // bothering to do right now.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN,
                 datapack_assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error DataPack assumes little endian
#endif

  DataPackEntry* target = reinterpret_cast<DataPackEntry*>(
      bsearch(&resource_id, mmap_->data() + kHeaderLength, resource_count_,
              sizeof(DataPackEntry), DataPackEntry::CompareById));
  if (!target) {
    LOG(ERROR) << "No resource found with id: " << resource_id;
    return false;
  }

  data->set(mmap_->data() + target->file_offset, target->length);
  return true;
}

}  // namespace base
