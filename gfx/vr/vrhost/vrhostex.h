/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// vrhostex.h
// This file declares the functions and their typedefs for functions exported
// by vrhost.dll

#pragma once
#include <stdint.h>

// void SampleExport();
typedef void (*PFN_SAMPLE)();

typedef void (*PFN_CREATEVRWINDOW)(char* firefoxFolderPath,
                                   char* firefoxProfilePath,
                                   uint32_t dxgiAdapterID, uint32_t widthHost,
                                   uint32_t heightHost, uint32_t* windowId,
                                   void** hTex, uint32_t* width,
                                   uint32_t* height);

typedef void (*PFN_CLOSEVRWINDOW)(uint32_t nVRWindowID, bool waitForTerminate);

typedef void (*PFN_SENDUIMSG)(uint32_t nVRWindowID, uint32_t msg,
                              uint64_t wparam, uint64_t lparam);
