/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileHandleStorage_h
#define mozilla_dom_FileHandleStorage_h

namespace mozilla {
namespace dom {

enum FileHandleStorage
{
  FILE_HANDLE_STORAGE_IDB = 0,
  // A placeholder for bug 997471
  //FILE_HANDLE_STORAGE_FS
  FILE_HANDLE_STORAGE_MAX
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileHandleStorage_h
