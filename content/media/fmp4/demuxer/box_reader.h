// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_BOX_READER_H_
#define MEDIA_MP4_BOX_READER_H_

#include <map>
#include <vector>

#include "mp4_demuxer/basictypes.h"
#include "mp4_demuxer/fourccs.h"

#include "nsAutoPtr.h"

namespace mp4_demuxer {

class BoxReader;
class Stream;

struct Box {
  virtual ~Box();
  virtual bool Parse(BoxReader* reader) = 0;
  virtual FourCC BoxType() const = 0;
};

class StreamReader {
 public:
  StreamReader(Stream* stream, int64_t offset, int64_t size)
    : stream_(stream), start_(offset), pos_(0), size_(size) {}

  bool HasBytes(int count) { return (pos() + count <= size()); }

  // Read a value from the stream, perfoming endian correction, and advance the
  // stream pointer.
  bool Read1(uint8_t* v)  WARN_UNUSED_RESULT;
  bool Read2(uint16_t* v) WARN_UNUSED_RESULT;
  bool Read2s(int16_t* v) WARN_UNUSED_RESULT;
  bool Read4(uint32_t* v) WARN_UNUSED_RESULT;
  bool Read4s(int32_t* v) WARN_UNUSED_RESULT;
  bool Read8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read8s(int64_t* v) WARN_UNUSED_RESULT;

  bool ReadFourCC(FourCC* v) WARN_UNUSED_RESULT;

  bool ReadVec(std::vector<uint8_t>* t, int count) WARN_UNUSED_RESULT;

  // These variants read a 4-byte integer of the corresponding signedness and
  // store it in the 8-byte return type.
  bool Read4Into8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read4sInto8s(int64_t* v) WARN_UNUSED_RESULT;

  // Advance the stream by this many bytes.
  bool SkipBytes(int nbytes) WARN_UNUSED_RESULT;

  //const uint8_t* data() const { return buf_; }
  int64_t size() const;
  int64_t pos() const;

protected:

  // The start offset of the box in the stream.
  const int64_t start_;

  // The size of the box in the stream.
  int64_t size_;

  // The offset from start_ at which the read cursor is.
  // 0 initially.
  int64_t pos_;

  // The stream that we read from.
  Stream* stream_;

  template<typename T> bool Read(T* t) WARN_UNUSED_RESULT;
};

class BoxReader : public StreamReader {
 public:
  ~BoxReader();

  // Create a BoxReader from a buffer. Note that this function may return NULL
  // if an intact, complete box was not available in the buffer. If |*err| is
  // set, there was a stream-level error when creating the box; otherwise, NULL
  // values are only expected when insufficient data is available.
  //
  // |buf| is retained but not owned, and must outlive the BoxReader instance.
  static BoxReader* ReadTopLevelBox(Stream* stream,
                                    int64_t offset,
                                    bool* err);

  // Read the box header from the current buffer. This function returns true if
  // the header is sane; that is, it does not check to ensure the entire box is
  // in the buffer before returning true.
  static bool StartTopLevelBox(Stream* stream,
                               int64_t offset,
                               FourCC* type,
                               int* box_size);

  // Returns true if |type| is recognized to be a top-level box, false
  // otherwise. This returns true for some boxes which we do not parse.
  // Helpful in debugging misaligned appends.
  static bool IsValidTopLevelBox(const FourCC& type);

  // Scan through all boxes within the current box, starting at the current
  // buffer position. Must be called before any of the *Child functions work.
  bool ScanChildren() WARN_UNUSED_RESULT;

  // Read exactly one child box from the set of children. The type of the child
  // will be determined by the BoxType() method of |child|.
  bool ReadChild(Box* child) WARN_UNUSED_RESULT;

  // Read one child if available. Returns false on error, true on successful
  // read or on child absent.
  bool MaybeReadChild(Box* child) WARN_UNUSED_RESULT;

  // Read at least one child. False means error or no such child present.
  template<typename T> bool ReadChildren(
      std::vector<T>* children) WARN_UNUSED_RESULT;

  // Read any number of children. False means error.
  template<typename T> bool MaybeReadChildren(
      std::vector<T>* children) WARN_UNUSED_RESULT;

  // Read all children, regardless of FourCC. This is used from exactly one box,
  // corresponding to a rather significant inconsistency in the BMFF spec.
  // Note that this method is mutually exclusive with ScanChildren().
  template<typename T> bool ReadAllChildren(
      std::vector<T>* children) WARN_UNUSED_RESULT;

  // Populate the values of 'version()' and 'flags()' from a full box header.
  // Many boxes, but not all, use these values. This call should happen after
  // the box has been initialized, and does not re-read the main box header.
  bool ReadFullBoxHeader() WARN_UNUSED_RESULT;

  FourCC type() const   { return type_; }
  uint8_t version() const { return version_; }
  uint32_t flags() const  { return flags_; }

private:

  BoxReader(Stream* stream, int64_t offset, int64_t size);

  // Must be called immediately after init. If the return is false, this
  // indicates that the box header and its contents were not available in the
  // stream or were nonsensical, and that the box must not be used further. In
  // this case, if |*err| is false, the problem was simply a lack of data, and
  // should only be an error condition if some higher-level component knows that
  // no more data is coming (i.e. EOS or end of containing box). If |*err| is
  // true, the error is unrecoverable and the stream should be aborted.
  bool ReadHeader(bool* err);

  FourCC type_;
  uint8_t version_;
  uint32_t flags_;

  typedef std::multimap<FourCC, BoxReader> ChildMap;

  // The set of child box FourCCs and their corresponding buffer readers. Only
  // valid if scanned_ is true.
  ChildMap children_;
  bool scanned_;
};

// Template definitions
template<typename T> bool BoxReader::ReadChildren(std::vector<T>* children) {
  RCHECK(MaybeReadChildren(children) && !children->empty());
  return true;
}

template<typename T>
bool BoxReader::MaybeReadChildren(std::vector<T>* children) {
  DCHECK(scanned_);
  DCHECK(children->empty());

  children->resize(1);
  FourCC child_type = (*children)[0].BoxType();

  ChildMap::iterator start_itr = children_.lower_bound(child_type);
  ChildMap::iterator end_itr = children_.upper_bound(child_type);
  children->resize(std::distance(start_itr, end_itr));
  typename std::vector<T>::iterator child_itr = children->begin();
  for (ChildMap::iterator itr = start_itr; itr != end_itr; ++itr) {
    RCHECK(child_itr->Parse(&itr->second));
    ++child_itr;
  }
  children_.erase(start_itr, end_itr);

  DMX_LOG("Found %d %s boxes\n",
      children->size(),
      FourCCToString(child_type).c_str());

  return true;
}

template<typename T>
bool BoxReader::ReadAllChildren(std::vector<T>* children) {
  DCHECK(!scanned_);
  scanned_ = true;

  bool err = false;
  while (pos() < size()) {
    BoxReader child_reader(stream_, start_ + pos_, size_ - pos_);
    if (!child_reader.ReadHeader(&err)) break;
    T child;
    RCHECK(child.Parse(&child_reader));
    children->push_back(child);
    pos_ += child_reader.size();
  }

  return !err;
}

}  // namespace mp4_demuxer

#endif  // MEDIA_MP4_BOX_READER_H_
