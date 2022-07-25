# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# WARNING: the syntax of the builtin types is not checked, so please
# don't add something syntactically invalid.  It will not be fun to
# track down the bug.

Types = (
    # C types
    "bool",
    "char",
    "short",
    "int",
    "long",
    "float",
    "double",
    # stdint types
    "int8_t",
    "uint8_t",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
    "intptr_t",
    "uintptr_t",
    # You may be tempted to add size_t. Do not! See bug 1525199.
    # Mozilla types: "less" standard things we know how serialize/deserialize
    "nsresult",
    "nsString",
    "nsCString",
    "mozilla::ipc::Shmem",
    "mozilla::ipc::ByteBuf",
    "mozilla::UniquePtr",
    "mozilla::ipc::FileDescriptor",
)


# XXX(Bug 1677487) Can we restrict including ByteBuf.h, FileDescriptor.h,
# MozPromise.h and Shmem.h to those protocols that really use them?
HeaderIncludes = (
    "mozilla/Attributes.h",
    "IPCMessageStart.h",
    "mozilla/RefPtr.h",
    "nsString.h",
    "nsTArray.h",
    "nsTHashtable.h",
    "mozilla/MozPromise.h",
    "mozilla/OperatorNewExtensions.h",
    "mozilla/UniquePtr.h",
    "mozilla/ipc/ByteBuf.h",
    "mozilla/ipc/FileDescriptor.h",
    "mozilla/ipc/ProtocolUtilsFwd.h",
    "mozilla/ipc/Shmem.h",
)

CppIncludes = (
    "ipc/IPCMessageUtils.h",
    "ipc/IPCMessageUtilsSpecializations.h",
    "nsIFile.h",
    "mozilla/ipc/Endpoint.h",
    "mozilla/ipc/ProtocolMessageUtils.h",
    "mozilla/ipc/ProtocolUtils.h",
    "mozilla/ipc/ShmemMessageUtils.h",
    "mozilla/ipc/TaintingIPCUtils.h",
)
