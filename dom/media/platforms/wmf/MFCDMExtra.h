/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFCDMEXTRA_H
#define DOM_MEDIA_PLATFORM_WMF_MFCDMEXTRA_H

#include <mfcontentdecryptionmodule.h>

// See
// https://github.com/microsoft/media-foundation/issues/37#issuecomment-1198317488
EXTERN_GUID(GUID_ObjectStream, 0x3e73735c, 0xe6c0, 0x481d, 0x82, 0x60, 0xee,
            0x5d, 0xb1, 0x34, 0x3b, 0x5f);
EXTERN_GUID(GUID_ClassName, 0x77631a31, 0xe5e7, 0x4785, 0xbf, 0x17, 0x20, 0xf5,
            0x7b, 0x22, 0x48, 0x02);
EXTERN_GUID(CLSID_EMEStoreActivate, 0x2df7b51e, 0x797b, 0x4d06, 0xbe, 0x71,
            0xd1, 0x4a, 0x52, 0xcf, 0x84, 0x21);

#endif  // DOM_MEDIA_PLATFORM_WMF_MFCDMEXTRA_H
