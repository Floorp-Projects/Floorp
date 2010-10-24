// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <new>
#include "hmtx.h"

#include "hhea.h"
#include "maxp.h"

// hmtx - Horizontal Metrics
// http://www.microsoft.com/opentype/otspec/hmtx.htm

namespace ots {

bool ots_hmtx_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHMTX *hmtx = new OpenTypeHMTX;
  file->hmtx = hmtx;

  if (!file->hhea || !file->maxp) {
    return OTS_FAILURE();
  }

  // |num_hmetrics| is a uint16_t, so it's bounded < 65536. This limits that
  // amount of memory that we'll allocate for this to a sane amount.
  const unsigned num_hmetrics = file->hhea->num_hmetrics;

  if (num_hmetrics > file->maxp->num_glyphs) {
    return OTS_FAILURE();
  }
  if (!num_hmetrics) {
    return OTS_FAILURE();
  }
  const unsigned num_lsbs = file->maxp->num_glyphs - num_hmetrics;

  hmtx->metrics.reserve(num_hmetrics);
  for (unsigned i = 0; i < num_hmetrics; ++i) {
    uint16_t adv = 0;
    int16_t lsb = 0;
    if (!table.ReadU16(&adv) || !table.ReadS16(&lsb)) {
      return OTS_FAILURE();
    }

    // Since so many fonts don't have proper value on |adv| and |lsb|,
    // we should not call ots_failure() here. For example, about 20% of fonts
    // in http://www.princexml.com/fonts/ (200+ fonts) fails these tests.
    if (adv > file->hhea->adv_width_max) {
      OTS_WARNING("bad adv: %u > %u", adv, file->hhea->adv_width_max);
      adv = file->hhea->adv_width_max;
    }
    if (lsb < file->hhea->min_lsb) {
      OTS_WARNING("bad lsb: %d < %d", lsb, file->hhea->min_lsb);
      lsb = file->hhea->min_lsb;
    }

    hmtx->metrics.push_back(std::make_pair(adv, lsb));
  }

  hmtx->lsbs.reserve(num_lsbs);
  for (unsigned i = 0; i < num_lsbs; ++i) {
    int16_t lsb;
    if (!table.ReadS16(&lsb)) {
      // Some Japanese fonts (e.g., mona.ttf) fail this test.
      return OTS_FAILURE();
    }

    if (lsb < file->hhea->min_lsb) {
      // The same as above. Three fonts in http://www.fontsquirrel.com/fontface
      // (e.g., Notice2Std.otf) have weird lsb values.
      OTS_WARNING("bad lsb: %d < %d", lsb, file->hhea->min_lsb);
      lsb = file->hhea->min_lsb;
    }

    hmtx->lsbs.push_back(lsb);
  }

  return true;
}

bool ots_hmtx_should_serialise(OpenTypeFile *file) {
  return file->hmtx;
}

bool ots_hmtx_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeHMTX *hmtx = file->hmtx;

  for (unsigned i = 0; i < hmtx->metrics.size(); ++i) {
    if (!out->WriteU16(hmtx->metrics[i].first) ||
        !out->WriteS16(hmtx->metrics[i].second)) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < hmtx->lsbs.size(); ++i) {
    if (!out->WriteS16(hmtx->lsbs[i])) {
      return OTS_FAILURE();
    }
  }

  return true;
}

void ots_hmtx_free(OpenTypeFile *file) {
  delete file->hmtx;
}

}  // namespace ots
