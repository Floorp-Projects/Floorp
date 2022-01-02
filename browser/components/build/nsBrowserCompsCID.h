/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/////////////////////////////////////////////////////////////////////////////

#ifdef XP_WIN
#  define NS_WINIEHISTORYENUMERATOR_CID                \
    {                                                  \
      0x93480624, 0x806e, 0x4756, {                    \
        0xb7, 0xcb, 0x0f, 0xb7, 0xdd, 0x74, 0x6a, 0x8f \
      }                                                \
    }

#  define NS_IEHISTORYENUMERATOR_CONTRACTID \
    "@mozilla.org/profile/migrator/iehistoryenumerator;1"
#endif

#define NS_SHELLSERVICE_CID                         \
  {                                                 \
    0x63c7b9f4, 0xcc8, 0x43f8, {                    \
      0xb6, 0x66, 0xa, 0x66, 0x16, 0x55, 0xcb, 0x73 \
    }                                               \
  }

#define NS_SHELLSERVICE_CONTRACTID "@mozilla.org/browser/shell-service;1"

#define NS_RDF_FORWARDPROXY_INFER_DATASOURCE_CID     \
  {                                                  \
    0x7a024bcf, 0xedd5, 0x4d9a, {                    \
      0x86, 0x14, 0xd4, 0x4b, 0xe1, 0xda, 0xda, 0xd3 \
    }                                                \
  }

// 136e2c4d-c5a4-477c-b131-d93d7d704f64
#define NS_PRIVATE_BROWSING_SERVICE_WRAPPER_CID      \
  {                                                  \
    0x136e2c4d, 0xc5a4, 0x477c, {                    \
      0xb1, 0x31, 0xd9, 0x3d, 0x7d, 0x70, 0x4f, 0x64 \
    }                                                \
  }
