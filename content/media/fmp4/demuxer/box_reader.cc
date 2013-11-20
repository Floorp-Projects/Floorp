// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mp4_demuxer/box_reader.h"

#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <memory>
#include <iostream>
#include <assert.h>

#include "mp4_demuxer/box_definitions.h"

#include "mp4_demuxer/Streams.h"

using namespace std;

namespace mp4_demuxer {

Box::~Box() {}

bool StreamReader::Read1(uint8_t* v) {
  RCHECK(HasBytes(1));
  assert(start_ + pos_ <= stream_->Length());
  uint32_t bytesRead = 0;
  if (!stream_->ReadAt(start_ + pos_, v, 1, &bytesRead) || bytesRead != 1) {
    return false;
  }
  pos_ += bytesRead;
  return true;
}

// Internal implementation of multi-byte reads
template<typename T> bool StreamReader::Read(T* v) {
  RCHECK(HasBytes(sizeof(T)));

  T tmp = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    tmp <<= 8;
    uint8_t byte;
    Read1(&byte);
    tmp += byte;
  }
  *v = tmp;
  return true;
}

bool StreamReader::Read2(uint16_t* v) { return Read(v); }
bool StreamReader::Read2s(int16_t* v) { return Read(v); }
bool StreamReader::Read4(uint32_t* v) { return Read(v); }
bool StreamReader::Read4s(int32_t* v) { return Read(v); }
bool StreamReader::Read8(uint64_t* v) { return Read(v); }
bool StreamReader::Read8s(int64_t* v) { return Read(v); }

bool StreamReader::ReadFourCC(FourCC* v) {
  return Read4(reinterpret_cast<uint32_t*>(v));
}

bool StreamReader::ReadVec(std::vector<uint8_t>* vec, int count) {
  RCHECK(HasBytes(count));
  vec->resize(count);
  assert(start_ + pos_ <= stream_->Length());
  uint32_t bytesRead = 0;
  if (!stream_->ReadAt(start_ + pos_, vec->data(), count, &bytesRead)) {
    return false;
  }
  pos_ += bytesRead;
  return true;
}

bool StreamReader::SkipBytes(int bytes) {
  RCHECK(HasBytes(bytes));
  pos_ += bytes;
  return true;
}

bool StreamReader::Read4Into8(uint64_t* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = tmp;
  return true;
}

bool StreamReader::Read4sInto8s(int64_t* v) {
  // Beware of the need for sign extension.
  int32_t tmp;
  RCHECK(Read4s(&tmp));
  *v = tmp;
  return true;
}

int64_t StreamReader::size() const {
  return size_;
}

int64_t StreamReader::pos() const {
  return pos_;
}

BoxReader::BoxReader(Stream* stream, int64_t offset, int64_t size)
    : StreamReader(stream, offset, size),
      type_(FOURCC_NULL),
      version_(0),
      flags_(0),
      scanned_(false) {
}

BoxReader::~BoxReader() {
  if (scanned_ && !children_.empty()) {
    for (ChildMap::iterator itr = children_.begin();
         itr != children_.end(); ++itr) {
      auto reader = itr->second;
      DMX_LOG("Skipping unknown box: '%s' reader type'%s'\n",
          FourCCToString(itr->first).c_str(),
          FourCCToString(reader.type()).c_str());
    }
  }
}

// static
BoxReader* BoxReader::ReadTopLevelBox(Stream* stream,
                                      int64_t offset,
                                      bool* err) {
  nsAutoPtr<BoxReader> reader(new BoxReader(stream, offset, stream->Length()));
  if (!reader->ReadHeader(err))
    return NULL;

  if (!IsValidTopLevelBox(reader->type())) {
    *err = true;
    return NULL;
  }

  if (reader->size() <= stream->Length())
    return reader.forget();

  return NULL;
}

// static
bool BoxReader::StartTopLevelBox(Stream* stream,
                                 int64_t offset,
                                 FourCC* type,
                                 int* box_size) {
  assert(stream->Length() > offset);
  BoxReader reader(stream, offset, stream->Length() - offset);
  bool err = false;
  if (!reader.ReadHeader(&err)) return false;
  if (!IsValidTopLevelBox(reader.type()) || err) {
    return false;
  }
  *type = reader.type();
  *box_size = reader.size();
  return true;
}

// static
bool BoxReader::IsValidTopLevelBox(const FourCC& type) {
  switch (type) {
    case FOURCC_FTYP:
    case FOURCC_PDIN:
    case FOURCC_BLOC:
    case FOURCC_MOOV:
    case FOURCC_MOOF:
    case FOURCC_MFRA:
    case FOURCC_MDAT:
    case FOURCC_FREE:
    case FOURCC_SKIP:
    case FOURCC_META:
    case FOURCC_MECO:
    case FOURCC_STYP:
    case FOURCC_SIDX:
    case FOURCC_SSIX:
    case FOURCC_PRFT:
      return true;
    default:
      // Hex is used to show nonprintable characters and aid in debugging
      DMX_LOG("Unrecognized top-level box type 0x%x\n", type);
      return false;
  }
}

bool BoxReader::ScanChildren() {
  DCHECK(!scanned_);
  scanned_ = true;

  bool err = false;
  while (pos() < size()) {
    BoxReader child(stream_, start_ + pos_, size_ - pos_);
    if (!child.ReadHeader(&err)) break;
    assert(child.size() < size());
    children_.insert(std::pair<FourCC, BoxReader>(child.type(), child));
    pos_ += child.size();
  }

  DCHECK(!err);
  return !err && pos() == size();
}

bool BoxReader::ReadChild(Box* child) {
  DCHECK(scanned_);
  FourCC child_type = child->BoxType();

  ChildMap::iterator itr = children_.find(child_type);
  if (itr == children_.end()) {
    DMX_LOG("No child of type %s\n", FourCCToString(child_type).c_str());
  }
  RCHECK(itr != children_.end());
  DMX_LOG("Found a %s box\n", FourCCToString(child_type).c_str());
  RCHECK(child->Parse(&itr->second));
  children_.erase(itr);
  return true;
}

bool BoxReader::MaybeReadChild(Box* child) {
  if (!children_.count(child->BoxType())) return true;
  return ReadChild(child);
}

bool BoxReader::ReadFullBoxHeader() {
  uint32_t vflags;
  RCHECK(Read4(&vflags));
  version_ = vflags >> 24;
  flags_ = vflags & 0xffffff;
  return true;
}

bool BoxReader::ReadHeader(bool* err) {
  uint64_t size = 0;
  *err = false;

  if (!HasBytes(8)) return false;
  CHECK(Read4Into8(&size) && ReadFourCC(&type_));

  DMX_LOG("BoxReader::ReadHeader() read %s size=%d\n", FourCCToString(type_).c_str(), size);

  if (size == 0) {
    // Media Source specific: we do not support boxes that run to EOS.
    *err = true;
    return false;
  } else if (size == 1) {
    if (!HasBytes(8)) return false;
    CHECK(Read8(&size));
  }

  // Implementation-specific: support for boxes larger than 2^31 has been
  // removed.
  if (size < static_cast<uint64_t>(pos_) ||
      size > static_cast<uint64_t>(kint32max)) {
    *err = true;
    return false;
  }

  // Note that the pos_ head has advanced to the byte immediately after the
  // header, which is where we want it.
  size_ = size;

  return true;
}

}  // namespace mp4_demuxer
