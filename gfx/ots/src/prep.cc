// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "prep.h"

// prep - Control Value Program
// http://www.microsoft.com/opentype/otspec/prep.htm

namespace ots {

bool ots_prep_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypePREP *prep = new OpenTypePREP;
  file->prep = prep;

  if (length >= 128 * 1024u) {
    return OTS_FAILURE();  // almost all prep tables are less than 9k bytes.
  }

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  prep->data = data;
  prep->length = length;
  return true;
}

bool ots_prep_should_serialise(OpenTypeFile *file) {
  if (!file->glyf) return false;  // this table is not for CFF fonts.
  return g_transcode_hints && file->prep;
}

bool ots_prep_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypePREP *prep = file->prep;

  if (!out->Write(prep->data, prep->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_prep_free(OpenTypeFile *file) {
  delete file->prep;
}

}  // namespace ots
