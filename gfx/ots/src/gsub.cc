// Copyright (c) 2010 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gsub.h"

// GSUB - Glyph Substitution Table
// http://www.microsoft.com/typography/otspec/gsub.htm

namespace ots {

bool ots_gsub_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeGSUB *gsub = new OpenTypeGSUB;
  file->gsub = gsub;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  gsub->data = data;
  gsub->length = length;
  return true;
}

bool ots_gsub_should_serialise(OpenTypeFile *file) {
  return file->preserve_otl && file->gsub;
}

bool ots_gsub_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeGSUB *gsub = file->gsub;

  if (!out->Write(gsub->data, gsub->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_gsub_free(OpenTypeFile *file) {
  delete file->gsub;
}

}  // namespace ots
