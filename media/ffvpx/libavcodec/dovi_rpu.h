/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Stubs for dovi_rpu.{c,h} */

typedef struct AVCtx AVContext;

typedef struct DOVICtx {
  int dv_profile;
  void* logctx;
  int operating_point;
} DOVIContext;

typedef struct AVDOVICConfRecord {
} AVDOVIDecoderConfigurationRecord;

static void ff_dovi_ctx_unref(DOVIContext* ctx) {}
static void ff_dovi_update_cfg(DOVIContext* ctx,
                               AVDOVIDecoderConfigurationRecord* record) {}
static int ff_dovi_rpu_parse(DOVIContext* ctx, uint8_t* buf, size_t len,
                             int err_recognition) {
  return 0;
}
static int ff_dovi_attach_side_data(DOVIContext* ctx, AVFrame* frame) {
  return 0;
}
