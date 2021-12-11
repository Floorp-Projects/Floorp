/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPC_GLUE_IPCCORE_H_
#define IPC_GLUE_IPCCORE_H_

namespace mozilla {

struct void_t {
  constexpr bool operator==(const void_t&) const { return true; }
};

struct null_t {
  constexpr bool operator==(const null_t&) const { return true; }
};

}  // namespace mozilla

#endif  // IPC_GLUE_IPCCORE_H_
