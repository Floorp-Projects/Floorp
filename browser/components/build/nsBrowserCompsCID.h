/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/////////////////////////////////////////////////////////////////////////////

#ifdef XP_WIN
#define NS_WINIEHISTORYENUMERATOR_CID \
{ 0x93480624, 0x806e, 0x4756, { 0xb7, 0xcb, 0x0f, 0xb7, 0xdd, 0x74, 0x6a, 0x8f } }

#define NS_IEHISTORYENUMERATOR_CONTRACTID \
  "@mozilla.org/profile/migrator/iehistoryenumerator;1"
#endif

#define NS_SHELLSERVICE_CID \
{ 0x63c7b9f4, 0xcc8, 0x43f8, { 0xb6, 0x66, 0xa, 0x66, 0x16, 0x55, 0xcb, 0x73 } }

#define NS_SHELLSERVICE_CONTRACTID \
  "@mozilla.org/browser/shell-service;1"

#define NS_RDF_FORWARDPROXY_INFER_DATASOURCE_CID \
{ 0x7a024bcf, 0xedd5, 0x4d9a, { 0x86, 0x14, 0xd4, 0x4b, 0xe1, 0xda, 0xda, 0xd3 } }

// 136e2c4d-c5a4-477c-b131-d93d7d704f64
#define NS_PRIVATE_BROWSING_SERVICE_WRAPPER_CID \
{ 0x136e2c4d, 0xc5a4, 0x477c, { 0xb1, 0x31, 0xd9, 0x3d, 0x7d, 0x70, 0x4f, 0x64 } }

// 7e4bb6ad-2fc4-4dc6-89ef-23e8e5ccf980
#define NS_BROWSER_ABOUT_REDIRECTOR_CID \
{ 0x7e4bb6ad, 0x2fc4, 0x4dc6, { 0x89, 0xef, 0x23, 0xe8, 0xe5, 0xcc, 0xf9, 0x80 } }

// {6DEB193C-F87D-4078-BC78-5E64655B4D62}
#define NS_BROWSERDIRECTORYPROVIDER_CID \
{ 0x6deb193c, 0xf87d, 0x4078, { 0xbc, 0x78, 0x5e, 0x64, 0x65, 0x5b, 0x4d, 0x62 } }

#if defined(MOZ_WIDGET_COCOA)
#define NS_MACATTRIBUTIONSERVICE_CONTRACTID \
  "@mozilla.org/mac-attribution;1"

#define NS_MACATTRIBUTIONSERVICE_CID \
{ 0x6FC66A78, 0x6CBC, 0x4B3F, { 0xB7, 0xBA, 0x37, 0x92, 0x89, 0xB2, 0x92, 0x76 } }
#endif
