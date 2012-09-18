// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gfx/emf.h"

#include "base/gfx/rect.h"
#include "base/logging.h"

namespace gfx {

Emf::Emf() : emf_(NULL), hdc_(NULL) {
}

Emf::~Emf() {
  CloseEmf();
  DCHECK(!emf_ && !hdc_);
}

bool Emf::CreateDc(HDC sibling, const RECT* rect) {
  DCHECK(!emf_ && !hdc_);
  hdc_ = CreateEnhMetaFile(sibling, NULL, rect, NULL);
  DCHECK(hdc_);
  return hdc_ != NULL;
}

bool Emf::CreateFromData(const void* buffer, size_t size) {
  DCHECK(!emf_ && !hdc_);
  emf_ = SetEnhMetaFileBits(static_cast<unsigned>(size),
                            reinterpret_cast<const BYTE*>(buffer));
  DCHECK(emf_);
  return emf_ != NULL;
}

bool Emf::CloseDc() {
  DCHECK(!emf_ && hdc_);
  emf_ = CloseEnhMetaFile(hdc_);
  DCHECK(emf_);
  hdc_ = NULL;
  return emf_ != NULL;
}

void Emf::CloseEmf() {
  DCHECK(!hdc_);
  if (emf_) {
    DeleteEnhMetaFile(emf_);
    emf_ = NULL;
  }
}

bool Emf::Playback(HDC hdc, const RECT* rect) const {
  DCHECK(emf_ && !hdc_);
  RECT bounds;
  if (!rect) {
    // Get the natural bounds of the EMF buffer.
    bounds = GetBounds().ToRECT();
    rect = &bounds;
  }
  return PlayEnhMetaFile(hdc, emf_, rect) != 0;
}

bool Emf::SafePlayback(HDC context) const {
  DCHECK(emf_ && !hdc_);
  XFORM base_matrix;
  if (!GetWorldTransform(context, &base_matrix)) {
    NOTREACHED();
    return false;
  }

  return EnumEnhMetaFile(context,
                         emf_,
                         &Emf::SafePlaybackProc,
                         reinterpret_cast<void*>(&base_matrix),
                         &GetBounds().ToRECT()) != 0;
}

gfx::Rect Emf::GetBounds() const {
  DCHECK(emf_ && !hdc_);
  ENHMETAHEADER header;
  if (GetEnhMetaFileHeader(emf_, sizeof(header), &header) != sizeof(header)) {
    NOTREACHED();
    return gfx::Rect();
  }
  if (header.rclBounds.left == 0 &&
      header.rclBounds.top == 0 &&
      header.rclBounds.right == -1 &&
      header.rclBounds.bottom == -1) {
    // A freshly created EMF buffer that has no drawing operation has invalid
    // bounds. Instead of having an (0,0) size, it has a (-1,-1) size. Detect
    // this special case and returns an empty Rect instead of an invalid one.
    return gfx::Rect();
  }
  return gfx::Rect(header.rclBounds.left,
                   header.rclBounds.top,
                   header.rclBounds.right - header.rclBounds.left,
                   header.rclBounds.bottom - header.rclBounds.top);
}

unsigned Emf::GetDataSize() const {
  DCHECK(emf_ && !hdc_);
  return GetEnhMetaFileBits(emf_, 0, NULL);
}

bool Emf::GetData(void* buffer, size_t size) const {
  DCHECK(emf_ && !hdc_);
  DCHECK(buffer && size);
  unsigned size2 = GetEnhMetaFileBits(emf_, static_cast<unsigned>(size),
                                      reinterpret_cast<BYTE*>(buffer));
  DCHECK(size2 == size);
  return size2 == size && size2 != 0;
}

bool Emf::GetData(std::vector<uint8_t>* buffer) const {
  unsigned size = GetDataSize();
  if (!size)
    return false;

  buffer->resize(size);
  if (!GetData(&buffer->front(), size))
    return false;
  return true;
}

bool Emf::SaveTo(const std::wstring& filename) const {
  HANDLE file = CreateFile(filename.c_str(), GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           CREATE_ALWAYS, 0, NULL);
  if (file == INVALID_HANDLE_VALUE)
    return false;

  bool success = false;
  std::vector<uint8_t> buffer;
  if (GetData(&buffer)) {
    DWORD written = 0;
    if (WriteFile(file, &*buffer.begin(), static_cast<DWORD>(buffer.size()),
                  &written, NULL) &&
        written == buffer.size()) {
      success = true;
    }
  }
  CloseHandle(file);
  return success;
}

int CALLBACK Emf::SafePlaybackProc(HDC hdc,
                                   HANDLETABLE* handle_table,
                                   const ENHMETARECORD* record,
                                   int objects_count,
                                   LPARAM param) {
  const XFORM* base_matrix = reinterpret_cast<const XFORM*>(param);
  EnumerationContext context;
  context.handle_table = handle_table;
  context.objects_count = objects_count;
  context.hdc = hdc;
  Record record_instance(&context, record);
  bool success = record_instance.SafePlayback(base_matrix);
  DCHECK(success);
  return 1;
}

Emf::Record::Record() {
}

Emf::Record::Record(const EnumerationContext* context,
                    const ENHMETARECORD* record)
    : record_(record),
      context_(context) {
  DCHECK(record_);
}

bool Emf::Record::Play() const {
  return 0 != PlayEnhMetaFileRecord(context_->hdc,
                                    context_->handle_table,
                                    record_,
                                    context_->objects_count);
}

bool Emf::Record::SafePlayback(const XFORM* base_matrix) const {
  // For EMF field description, see [MS-EMF] Enhanced Metafile Format
  // Specification.
  //
  // This is the second major EMF breakage I get; the first one being
  // SetDCBrushColor/SetDCPenColor/DC_PEN/DC_BRUSH being silently ignored.
  //
  // This function is the guts of the fix for bug 1186598. Some printer drivers
  // somehow choke on certain EMF records, but calling the corresponding
  // function directly on the printer HDC is fine. Still, playing the EMF record
  // fails. Go figure.
  //
  // The main issue is that SetLayout is totally unsupported on these printers
  // (HP 4500/4700). I used to call SetLayout and I stopped. I found out this is
  // not sufficient because GDI32!PlayEnhMetaFile internally calls SetLayout(!)
  // Damn.
  //
  // So I resorted to manually parse the EMF records and play them one by one.
  // The issue with this method compared to using PlayEnhMetaFile to play back
  // an EMF buffer is that the later silently fixes the matrix to take in
  // account the matrix currently loaded at the time of the call.
  // The matrix magic is done transparently when using PlayEnhMetaFile but since
  // I'm processing one field at a time, I need to do the fixup myself. Note
  // that PlayEnhMetaFileRecord doesn't fix the matrix correctly even when
  // called inside an EnumEnhMetaFile loop. Go figure (bis).
  //
  // So when I see a EMR_SETWORLDTRANSFORM and EMR_MODIFYWORLDTRANSFORM, I need
  // to fix the matrix according to the matrix previously loaded before playing
  // back the buffer. Otherwise, the previously loaded matrix would be ignored
  // and the EMF buffer would always be played back at its native resolution.
  // Duh.
  //
  // I also use this opportunity to skip over eventual EMR_SETLAYOUT record that
  // could remain.
  //
  // Note: I should probably care about view ports and clipping, eventually.
  bool res;
  switch (record()->iType) {
    case EMR_SETWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize, sizeof(DWORD) * 2 + sizeof(XFORM));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      HDC hdc = context_->hdc;
      if (base_matrix) {
        res = 0 != SetWorldTransform(hdc, base_matrix) &&
                   ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
      } else {
        res = 0 != SetWorldTransform(hdc, xform);
      }
      break;
    }
    case EMR_MODIFYWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize,
                sizeof(DWORD) * 2 + sizeof(XFORM) + sizeof(DWORD));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      const DWORD* option = reinterpret_cast<const DWORD*>(xform + 1);
      HDC hdc = context_->hdc;
      switch (*option) {
        case MWT_IDENTITY:
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix);
          } else {
            res = 0 != ModifyWorldTransform(hdc, xform, MWT_IDENTITY);
          }
          break;
        case MWT_LEFTMULTIPLY:
        case MWT_RIGHTMULTIPLY:
          res = 0 != ModifyWorldTransform(hdc, xform, *option);
          break;
        case 4:  // MWT_SET
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix) &&
                       ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
          } else {
            res = 0 != SetWorldTransform(hdc, xform);
          }
          break;
        default:
          res = false;
          break;
      }
      break;
    }
    case EMR_SETLAYOUT:
      // Ignore it.
      res = true;
      break;
    default: {
      res = Play();
      break;
    }
  }
  return res;
}

Emf::Enumerator::Enumerator(const Emf& emf, HDC context, const RECT* rect) {
  context_.handle_table = NULL;
  context_.objects_count = 0;
  context_.hdc = NULL;
  items_.clear();
  if (!EnumEnhMetaFile(context,
                       emf.emf(),
                       &Emf::Enumerator::EnhMetaFileProc,
                       reinterpret_cast<void*>(this),
                       rect)) {
    NOTREACHED();
    items_.clear();
  }
  DCHECK_EQ(context_.hdc, context);
}

Emf::Enumerator::const_iterator Emf::Enumerator::begin() const {
  return items_.begin();
}

Emf::Enumerator::const_iterator Emf::Enumerator::end() const {
  return items_.end();
}

int CALLBACK Emf::Enumerator::EnhMetaFileProc(HDC hdc,
                                              HANDLETABLE* handle_table,
                                              const ENHMETARECORD* record,
                                              int objects_count,
                                              LPARAM param) {
  Enumerator& emf = *reinterpret_cast<Enumerator*>(param);
  if (!emf.context_.handle_table) {
    DCHECK(!emf.context_.handle_table);
    DCHECK(!emf.context_.objects_count);
    emf.context_.handle_table = handle_table;
    emf.context_.objects_count = objects_count;
    emf.context_.hdc = hdc;
  } else {
    DCHECK_EQ(emf.context_.handle_table, handle_table);
    DCHECK_EQ(emf.context_.objects_count, objects_count);
    DCHECK_EQ(emf.context_.hdc, hdc);
  }
  emf.items_.push_back(Record(&emf.context_, record));
  return 1;
}

}  // namespace gfx
