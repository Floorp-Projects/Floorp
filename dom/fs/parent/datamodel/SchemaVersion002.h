/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_DATAMODEL_SCHEMAVERSION002_H_
#define DOM_FS_PARENT_DATAMODEL_SCHEMAVERSION002_H_

#include "SchemaVersion001.h"

namespace mozilla::dom::fs {

namespace data {
class FileSystemFileManager;
}  // namespace data

struct SchemaVersion002 {
  static Result<DatabaseVersion, QMResult> InitializeConnection(
      ResultConnection& aConn, data::FileSystemFileManager& aFileManager,
      const Origin& aOrigin);

  static constexpr DatabaseVersion sVersion = 2;
};

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_PARENT_DATAMODEL_SCHEMAVERSION002_H_
