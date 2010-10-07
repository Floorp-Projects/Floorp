// Copyright (c) 2010 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gdef.h"

// GDEF - Glyph Definition Table
// http://www.microsoft.com/typography/otspec/gdef.htm

namespace ots {

bool ots_gdef_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeGDEF *gdef = new OpenTypeGDEF;
  file->gdef = gdef;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  gdef->data = data;
  gdef->length = length;
  return true;
}

bool ots_gdef_should_serialise(OpenTypeFile *file) {
  return file->preserve_otl && file->gdef;
}

bool ots_gdef_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeGDEF *gdef = file->gdef;

  if (!out->Write(gdef->data, gdef->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_gdef_free(OpenTypeFile *file) {
  delete file->gdef;
}

}  // namespace ots
