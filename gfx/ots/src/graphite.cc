// Copyright (c) 2010 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "graphite.h"

// Support for preserving (NOT sanitizing) the Graphite tables

namespace ots {

// 'Silf'
bool ots_silf_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeSILF *silf = new OpenTypeSILF;
  file->silf = silf;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  silf->data = data;
  silf->length = length;
  return true;
}

bool ots_silf_should_serialise(OpenTypeFile *file) {
  return file->preserve_graphite && file->silf;
}

bool ots_silf_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeSILF *silf = file->silf;

  if (!out->Write(silf->data, silf->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_silf_free(OpenTypeFile *file) {
  delete file->silf;
}

// 'Sill'
bool ots_sill_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeSILL *sill = new OpenTypeSILL;
  file->sill = sill;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  sill->data = data;
  sill->length = length;
  return true;
}

bool ots_sill_should_serialise(OpenTypeFile *file) {
  return file->preserve_graphite && file->sill;
}

bool ots_sill_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeSILL *sill = file->sill;

  if (!out->Write(sill->data, sill->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_sill_free(OpenTypeFile *file) {
  delete file->sill;
}

// 'Gloc'
bool ots_gloc_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeGLOC *gloc = new OpenTypeGLOC;
  file->gloc = gloc;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  gloc->data = data;
  gloc->length = length;
  return true;
}

bool ots_gloc_should_serialise(OpenTypeFile *file) {
  return file->preserve_graphite && file->gloc;
}

bool ots_gloc_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeGLOC *gloc = file->gloc;

  if (!out->Write(gloc->data, gloc->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_gloc_free(OpenTypeFile *file) {
  delete file->gloc;
}

// 'Glat'
bool ots_glat_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeGLAT *glat = new OpenTypeGLAT;
  file->glat = glat;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  glat->data = data;
  glat->length = length;
  return true;
}

bool ots_glat_should_serialise(OpenTypeFile *file) {
  return file->preserve_graphite && file->glat;
}

bool ots_glat_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeGLAT *glat = file->glat;

  if (!out->Write(glat->data, glat->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_glat_free(OpenTypeFile *file) {
  delete file->glat;
}

// 'Feat'
bool ots_feat_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeFEAT *feat = new OpenTypeFEAT;
  file->feat = feat;

  if (!table.Skip(length)) {
    return OTS_FAILURE();
  }

  feat->data = data;
  feat->length = length;
  return true;
}

bool ots_feat_should_serialise(OpenTypeFile *file) {
  return file->preserve_graphite && file->feat;
}

bool ots_feat_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeFEAT *feat = file->feat;

  if (!out->Write(feat->data, feat->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_feat_free(OpenTypeFile *file) {
  delete file->feat;
}

}  // namespace ots
