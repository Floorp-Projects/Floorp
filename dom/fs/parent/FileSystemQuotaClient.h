/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMQUOTACLIENT_H_
#define DOM_FS_PARENT_FILESYSTEMQUOTACLIENT_H_

template <class T>
struct already_AddRefed;

namespace mozilla::dom {

namespace quota {
class Client;
}  // namespace quota

namespace fs {

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient();

}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMQUOTACLIENT_H_
