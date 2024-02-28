/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef FFmpegRDFTTypes_h
#define FFmpegRDFTTypes_h

struct RDFTContext;

using FFTSample = float;

enum RDFTransformType {
  DFT_R2C,
  IDFT_C2R,
  IDFT_R2C,
  DFT_C2R,
};

extern "C" {

using AvRdftInitFn = RDFTContext* (*)(int, enum RDFTransformType);
using AvRdftCalcFn = void (*)(RDFTContext*, FFTSample*);
using AvRdftEndFn = void (*)(RDFTContext*);
}

struct FFmpegRDFTFuncs {
  AvRdftInitFn init;
  AvRdftCalcFn calc;
  AvRdftEndFn end;
};

#endif  // FFmpegRDFTTypes_h
