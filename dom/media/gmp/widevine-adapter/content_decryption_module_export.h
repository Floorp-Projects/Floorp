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

// Define CDM_CLASS_API to export class types. We have to add visibility
// attributes to make sure virtual tables in CDM consumer and CDM implementation
// are the same. Generally, it was always a good idea, as there're no guarantees
// about that for the internal symbols, but it has only become a practical issue
// after introduction of LTO devirtualization. See more details on
// https://crbug.com/609564#c35
#if defined(_WIN32)
#if defined(__clang__)
#define CDM_CLASS_API [[clang::lto_visibility_public]]
#else
#define CDM_CLASS_API
#endif
#else  // defined(_WIN32)
#define CDM_CLASS_API __attribute__((visibility("default")))
#endif  // defined(_WIN32)

#endif  // CDM_CONTENT_DECRYPTION_MODULE_EXPORT_H_
