# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# WARNING: the syntax of the builtin types is not checked, so please
# don't add something syntactically invalid.  It will not be fun to
# track down the bug.

Types = (
    # C types
    'bool',
    'char',
    'short',
    'int',
    'long',
    'float',
    'double',

    # stdint types
    'int8_t',
    'uint8_t',
    'int16_t',
    'uint16_t',
    'int32_t',
    'uint32_t',
    'int64_t',
    'uint64_t',
    'intptr_t',
    'uintptr_t',

    # stddef types
    'size_t',
    'ssize_t',

    # Mozilla types: "less" standard things we know how serialize/deserialize
    'nsresult',
    'nsString',
    'nsCString',
    'mozilla::ipc::Shmem',
    'mozilla::ipc::FileDescriptor'
)


Includes = (
    'mozilla/Attributes.h',
    'base/basictypes.h',
    'prtime.h',
    'nscore.h',
    'IPCMessageStart.h',
    'ipc/IPCMessageUtils.h',
    'nsAutoPtr.h',
    'nsStringGlue.h',
    'nsTArray.h',
    'nsIFile.h',
    'mozilla/ipc/ProtocolUtils.h',
    'GeckoProfiler.h'
)
