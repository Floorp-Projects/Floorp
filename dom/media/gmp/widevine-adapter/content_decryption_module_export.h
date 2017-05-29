// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CDM_CONTENT_DECRYPTION_MODULE_EXPORT_H_
#define CDM_CONTENT_DECRYPTION_MODULE_EXPORT_H_

// Define CDM_API so that functionality implemented by the CDM module
// can be exported to consumers.
#if defined(_WIN32)

#if defined(CDM_IMPLEMENTATION)
#define CDM_API __declspec(dllexport)
#else
#define CDM_API __declspec(dllimport)
#endif  // defined(CDM_IMPLEMENTATION)

#else  // defined(_WIN32)
#define CDM_API __attribute__((visibility("default")))
#endif  // defined(_WIN32)

#endif  // CDM_CONTENT_DECRYPTION_MODULE_EXPORT_H_
