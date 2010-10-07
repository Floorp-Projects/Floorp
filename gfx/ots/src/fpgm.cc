// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fpgm.h"

// fpgm - Font Program
// http://www.microsoft.com/opentype/otspec/fpgm.htm

namespace ots {

bool ots_fpgm_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeFPGM *fpgm = new OpenTypeFPGM;
  file->fpgm = fpgm;

  if (length >= 128 * 1024u) {
    return OTS_FAILURE();  // almost all fpgm tables are less than 5k bytes.
  }

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  fpgm->data = data;
  fpgm->length = length;
  return true;
}

bool ots_fpgm_should_serialise(OpenTypeFile *file) {
  if (!file->glyf) return false;  // this table is not for CFF fonts.
  return g_transcode_hints && file->fpgm;
}

bool ots_fpgm_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeFPGM *fpgm = file->fpgm;

  if (!out->Write(fpgm->data, fpgm->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_fpgm_free(OpenTypeFile *file) {
  delete file->fpgm;
}

}  // namespace ots
