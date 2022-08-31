/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMMANAGER_H_
#define DOM_FS_PARENT_FILESYSTEMMANAGER_H_

#include <functional>

enum class nsresult : uint32_t;

namespace mozilla {

namespace ipc {

template <class T>
class Endpoint;

class IPCResult;
class PrincipalInfo;

}  // namespace ipc

namespace dom {

class PFileSystemManagerParent;

mozilla::ipc::IPCResult CreateFileSystemManagerParent(
    const mozilla::ipc::PrincipalInfo& aPrincipalInfo,
    mozilla::ipc::Endpoint<mozilla::dom::PFileSystemManagerParent>&&
        aParentEndpoint,
    std::function<void(const nsresult&)>&& aResolver);

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_PARENT_FILESYSTEMMANAGER_H_
