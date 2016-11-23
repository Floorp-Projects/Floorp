/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASE_CHILD_PRIVILEGS_H_
#define BASE_CHILD_PRIVILEGS_H_

namespace base {

enum ChildPrivileges {
  PRIVILEGES_DEFAULT,
  PRIVILEGES_UNPRIVILEGED,
  PRIVILEGES_INHERIT,
  // PRIVILEGES_DEFAULT plus file read permissions, used for file content process.
  PRIVILEGES_FILEREAD,
  PRIVILEGES_LAST
};

} // namespace base

#endif  // BASE_CHILD_PRIVILEGS_H_
