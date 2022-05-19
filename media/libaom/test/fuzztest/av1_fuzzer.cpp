/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Copyright 2018 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

/* This file was originally imported from Google's oss-fuzz project at
 * https://github.com/google/oss-fuzz/tree/master/projects/libaom */

#define DECODE_MODE 1
#include "FuzzingInterface.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>

#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "aom_ports/mem_ops.h"
#include "common/ivfdec.h"

static const char *const kIVFSignature = "DKIF";

static void close_file(FILE *file) { fclose(file); }

void usage_exit(void) { exit(EXIT_FAILURE); }

static int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::unique_ptr<FILE, decltype(&close_file)> file(
    fmemopen((void *)data, size, "rb"), &close_file);

  if (file == nullptr) {
    return 0;
  }

  char header[32];
  if (fread(header, 1, 32, file.get()) != 32) {
    return 0;
  }

  const AvxInterface *decoder = get_aom_decoder_by_name("av1");
  if (decoder == nullptr) {
    return 0;
  }

  aom_codec_ctx_t codec;
#if defined(DECODE_MODE)
  const int threads = 1;
#elif defined(DECODE_MODE_threaded)
  const int threads = 16;
#else
#error define one of DECODE_MODE or DECODE_MODE_threaded
#endif
  aom_codec_dec_cfg_t cfg = {threads, 0, 0};
  if (aom_codec_dec_init(&codec, decoder->codec_interface(), &cfg, 0)) {
    return 0;
  }

  uint8_t *buffer = nullptr;
  size_t buffer_size = 0;
  size_t frame_size = 0;
  while (!ivf_read_frame(file.get(), &buffer, &frame_size, &buffer_size,
                         nullptr)) {
    const aom_codec_err_t err =
        aom_codec_decode(&codec, buffer, frame_size, nullptr);
    aom_codec_iter_t iter = nullptr;
    aom_image_t *img = nullptr;
    while ((img = aom_codec_get_frame(&codec, &iter)) != nullptr) {
    }
  }
  aom_codec_destroy(&codec);
  free(buffer);
  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(nullptr, LLVMFuzzerTestOneInput, AV1Decode);
