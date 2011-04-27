// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vmtx.h"

#include "gsub.h"
#include "maxp.h"
#include "vhea.h"

// vmtx - Vertical Metrics Table
// http://www.microsoft.com/opentype/otspec/vmtx.htm

namespace ots {

bool ots_vmtx_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeVMTX *vmtx = new OpenTypeVMTX;
  file->vmtx = vmtx;

  if (!file->vhea || !file->maxp) {
    return OTS_FAILURE();
  }

  if (!ParseMetricsTable(&table, file->maxp->num_glyphs,
                         &file->vhea->header, &vmtx->metrics)) {
    return OTS_FAILURE();
  }

  return true;
}

bool ots_vmtx_should_serialise(OpenTypeFile *file) {
  // vmtx should serialise when vhea and GSUB are preserved.
  // See the comment in ots_vhea_should_serialise().
  return file->vmtx != NULL && file->vhea != NULL &&
      ots_gsub_should_serialise(file);
}

bool ots_vmtx_serialise(OTSStream *out, OpenTypeFile *file) {
  if (!SerialiseMetricsTable(out, &file->vmtx->metrics)) {
    return OTS_FAILURE();
  }
  return true;
}

void ots_vmtx_free(OpenTypeFile *file) {
  delete file->vmtx;
}

}  // namespace ots

