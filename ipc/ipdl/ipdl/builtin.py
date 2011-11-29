# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# Contributor(s):
#   Chris Jones <jones.chris.g@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

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

    # NSPR types
    'PRInt8',
    'PRUint8',
    'PRInt16',
    'PRUint16',
    'PRInt32',
    'PRUint32',
    'PRInt64',
    'PRUint64',
    'PRSize',

    # Mozilla types: "less" standard things we know how serialize/deserialize
    'nsresult',
    'nsString',
    'nsCString',
    'mozilla::ipc::Shmem',

    # quasi-stdint types used by "public" Gecko headers
    'int8',
    'uint8',
    'int16',
    'uint16',
    'int32',
    'uint32',
    'int64',
    'uint64',
    'intptr',
    'uintptr',
)


Includes = (
    'mozilla/Attributes.h',
    'base/basictypes.h',
    'prtime.h',
    'nscore.h',
    'IPCMessageStart.h',
    'IPC/IPCMessageUtils.h',
    'nsAutoPtr.h',
    'nsStringGlue.h',
    'nsTArray.h',
    'nsIFile.h',
    'mozilla/ipc/ProtocolUtils.h',
)
