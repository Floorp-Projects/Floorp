// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GFX_EMF_H__
#define CHROME_COMMON_GFX_EMF_H__

#include <windows.h>
#include <vector>

#include "base/basictypes.h"

namespace gfx {

class Rect;

// Simple wrapper class that manage an EMF data stream and its virtual HDC.
class Emf {
 public:
  class Record;
  class Enumerator;
  struct EnumerationContext;

  Emf();
  ~Emf();

  // Generates a virtual HDC that will record every GDI commands and compile it
  // in a EMF data stream.
  // hdc is used to setup the default DPI and color settings. hdc is optional.
  // rect specifies the dimensions (in .01-millimeter units) of the EMF. rect is
  // optional.
  bool CreateDc(HDC sibling, const RECT* rect);

  // Load a EMF data stream. buffer contains EMF data.
  bool CreateFromData(const void* buffer, size_t size);

  // TODO(maruel): CreateFromFile(). If ever used. Maybe users would like to
  // have the ability to save web pages to an EMF file? Afterward, it is easy to
  // convert to PDF or PS.

  // Closes the HDC created by CreateDc() and generates the compiled EMF
  // data.
  bool CloseDc();

  // Closes the EMF data handle when it is not needed anymore.
  void CloseEmf();

  // "Plays" the EMF buffer in a HDC. It is the same effect as calling the
  // original GDI function that were called when recording the EMF. |rect| is in
  // "logical units" and is optional. If |rect| is NULL, the natural EMF bounds
  // are used.
  // Note: Windows has been known to have stack buffer overflow in its GDI
  // functions, whether used directly or indirectly through precompiled EMF
  // data. We have to accept the risk here. Since it is used only for printing,
  // it requires user intervention.
  bool Playback(HDC hdc, const RECT* rect) const;

  // The slow version of Playback(). It enumerates all the records and play them
  // back in the HDC. The trick is that it skip over the records known to have
  // issue with some printers. See Emf::Record::SafePlayback implementation for
  // details.
  bool SafePlayback(HDC hdc) const;

  // Retrieves the bounds of the painted area by this EMF buffer. This value
  // should be passed to Playback to keep the exact same size.
  gfx::Rect GetBounds() const;

  // Retrieves the EMF stream size.
  unsigned GetDataSize() const;

  // Retrieves the EMF stream.
  bool GetData(void* buffer, size_t size) const;

  // Retrieves the EMF stream. It is an helper function.
  bool GetData(std::vector<uint8_t>* buffer) const;

  HENHMETAFILE emf() const {
    return emf_;
  }

  HDC hdc() const {
    return hdc_;
  }

  // Saves the EMF data to a file as-is. It is recommended to use the .emf file
  // extension but it is not enforced. This function synchronously writes to the
  // file. For testing only.
  bool SaveTo(const std::wstring& filename) const;

 private:
  // Playbacks safely one EMF record.
  static int CALLBACK SafePlaybackProc(HDC hdc,
                                       HANDLETABLE* handle_table,
                                       const ENHMETARECORD* record,
                                       int objects_count,
                                       LPARAM param);

  // Compiled EMF data handle.
  HENHMETAFILE emf_;

  // Valid when generating EMF data through a virtual HDC.
  HDC hdc_;

  DISALLOW_EVIL_CONSTRUCTORS(Emf);
};

struct Emf::EnumerationContext {
  HANDLETABLE* handle_table;
  int objects_count;
  HDC hdc;
};

// One EMF record. It keeps pointers to the EMF buffer held by Emf::emf_.
// The entries become invalid once Emf::CloseEmf() is called.
class Emf::Record {
 public:
  Record();

  // Plays the record.
  bool Play() const;

  // Plays the record working around quirks with SetLayout,
  // SetWorldTransform and ModifyWorldTransform. See implementation for details.
  bool SafePlayback(const XFORM* base_matrix) const;

  // Access the underlying EMF record.
  const ENHMETARECORD* record() const { return record_; }

 protected:
  Record(const EnumerationContext* context,
         const ENHMETARECORD* record);

 private:
  friend class Emf;
  friend class Enumerator;
  const ENHMETARECORD* record_;
  const EnumerationContext* context_;
};

// Retrieves individual records out of a Emf buffer. The main use is to skip
// over records that are unsupported on a specific printer or to play back
// only a part of an EMF buffer.
class Emf::Enumerator {
 public:
  // Iterator type used for iterating the records.
  typedef std::vector<Record>::const_iterator const_iterator;

  // Enumerates the records at construction time. |hdc| and |rect| are
  // both optional at the same time or must both be valid.
  // Warning: |emf| must be kept valid for the time this object is alive.
  Enumerator(const Emf& emf, HDC hdc, const RECT* rect);

  // Retrieves the first Record.
  const_iterator begin() const;

  // Retrieves the end of the array.
  const_iterator end() const;

 private:
  // Processes one EMF record and saves it in the items_ array.
  static int CALLBACK EnhMetaFileProc(HDC hdc,
                                      HANDLETABLE* handle_table,
                                      const ENHMETARECORD* record,
                                      int objects_count,
                                      LPARAM param);

  // The collection of every EMF records in the currently loaded EMF buffer.
  // Initialized by Enumerate(). It keeps pointers to the EMF buffer held by
  // Emf::emf_. The entries become invalid once Emf::CloseEmf() is called.
  std::vector<Record> items_;

  EnumerationContext context_;

  DISALLOW_EVIL_CONSTRUCTORS(Enumerator);
};

}  // namespace gfx

#endif  // CHROME_COMMON_GFX_EMF_H__
