// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cvt.h"

// cvt - Control Value Table
// http://www.microsoft.com/opentype/otspec/cvt.htm

namespace ots {

bool ots_cvt_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeCVT *cvt = new OpenTypeCVT;
  file->cvt = cvt;

  if (length >= 128 * 1024u) {
    return OTS_FAILURE();  // almost all cvt tables are less than 4k bytes.
  }

  if (length % 2 != 0) {
    return OTS_FAILURE();
  }

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  cvt->data = data;
  cvt->length = length;
  return true;
}

bool ots_cvt_should_serialise(OpenTypeFile *file) {
  if (!file->glyf) {
    return false;  // this table is not for CFF fonts.
  }
  return g_transcode_hints && file->cvt;
}

bool ots_cvt_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeCVT *cvt = file->cvt;

  if (!out->Write(cvt->data, cvt->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_cvt_free(OpenTypeFile *file) {
  delete file->cvt;
}

}  // namespace ots
