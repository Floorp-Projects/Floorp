/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_scripterrorhelper_h__
#define mozilla_dom_indexeddb_scripterrorhelper_h__

#include <inttypes.h>

class nsACString;
class nsAString;

namespace mozilla {
namespace dom {
namespace indexedDB {

// Helper to report a script error to the main thread.
class ScriptErrorHelper
{
public:
  static void Dump(const nsAString& aMessage,
                   const nsAString& aFilename,
                   uint32_t aLineNumber,
                   uint32_t aColumnNumber,
                   uint32_t aSeverityFlag, /* nsIScriptError::xxxFlag */
                   bool aIsChrome,
                   uint64_t aInnerWindowID);

  static void DumpLocalizedMessage(const nsACString& aMessageName,
                                   const nsAString& aFilename,
                                   uint32_t aLineNumber,
                                   uint32_t aColumnNumber,
                                   uint32_t aSeverityFlag, /* nsIScriptError::xxxFlag */
                                   bool aIsChrome,
                                   uint64_t aInnerWindowID);
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_scripterrorhelper_h__