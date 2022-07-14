/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsConfigTriggers_h
#define nsConfigTriggers_h

#include "nscore.h"
#include "js/TypeDecls.h"

nsresult EvaluateAdminConfigScript(const char* js_buffer, size_t length,
                                   const char* filename, bool bGlobalContext,
                                   bool bCallbacks, bool skipFirstLine,
                                   bool isPrivileged = false);

nsresult EvaluateAdminConfigScript(JS::Handle<JSObject*> sandbox,
                                   const char* js_buffer, size_t length,
                                   const char* filename, bool bGlobalContext,
                                   bool bCallbacks, bool skipFirstLine);

#endif  // nsConfigTriggers_h
