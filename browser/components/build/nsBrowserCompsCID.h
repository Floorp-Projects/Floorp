/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/////////////////////////////////////////////////////////////////////////////

#define NS_BROWSERPROFILEMIGRATOR_CONTRACTID_PREFIX "@mozilla.org/profile/migrator;1?app=browser&type="

#ifdef XP_WIN
#define NS_WINIEPROFILEMIGRATOR_CID \
{ 0xbc15c73d, 0xc05b, 0x497b, { 0xa3, 0x73, 0x4b, 0xae, 0x6c, 0x17, 0x86, 0x31 } }
#endif

#ifdef XP_MACOSX
#define NS_SAFARIPROFILEMIGRATOR_CID \
{ 0x29e3b139, 0xad19, 0x44f3, { 0xb2, 0xc2, 0xe9, 0xf1, 0x3b, 0xa2, 0xbb, 0xc6 } }

#define NS_MACIEPROFILEMIGRATOR_CID \
{ 0xf1a4e549, 0x5c4b, 0x41ff, { 0xb5, 0xe3, 0xeb, 0x87, 0xae, 0x31, 0x41, 0x9b } }

#define NS_OMNIWEBPROFILEMIGRATOR_CID \
{ 0xb80ae6d8, 0x766c, 0x43da, { 0x9c, 0x7a, 0xd, 0x82, 0x44, 0x52, 0x61, 0x6a } }

#define NS_CAMINOPROFILEMIGRATOR_CID \
{ 0x01d88ea9, 0x0feb, 0x495e, { 0x8c, 0x9b, 0x41, 0x65, 0x99, 0x55, 0x52, 0x65 } }

#define NS_ICABPROFILEMIGRATOR_CID \
{ 0xf394a036, 0xc5e1, 0x46d8, { 0x99, 0x39, 0x6b, 0x35, 0xe1, 0x13, 0x0a, 0x27 } }

#endif

#define NS_OPERAPROFILEMIGRATOR_CID \
{ 0xf34ff792, 0x722e, 0x4490, { 0xb1, 0x95, 0x47, 0xd2, 0x42, 0xed, 0xca, 0x1c } }

#define NS_DOGBERTPROFILEMIGRATOR_CID \
{ 0x24f92fae, 0xf793, 0x473b, { 0x80, 0x61, 0x71, 0x34, 0x8, 0xbd, 0x11, 0xd5 } }

#define NS_SEAMONKEYPROFILEMIGRATOR_CID \
{ 0x9a28ffa7, 0xe6ef, 0x4b52, { 0xa1, 0x27, 0x6a, 0xd9, 0x51, 0xde, 0x8e, 0x9b } }

#define NS_PHOENIXPROFILEMIGRATOR_CID \
{ 0x78481e4a, 0x50e4, 0x4489, { 0xb6, 0x8a, 0xef, 0x82, 0x67, 0xe, 0xd6, 0x3f } }

#define NS_SHELLSERVICE_CID \
{ 0x63c7b9f4, 0xcc8, 0x43f8, { 0xb6, 0x66, 0xa, 0x66, 0x16, 0x55, 0xcb, 0x73 } }

#define NS_SHELLSERVICE_CONTRACTID \
  "@mozilla.org/browser/shell-service;1"

#define NS_RDF_FORWARDPROXY_INFER_DATASOURCE_CID \
{ 0x7a024bcf, 0xedd5, 0x4d9a, { 0x86, 0x14, 0xd4, 0x4b, 0xe1, 0xda, 0xda, 0xd3 } }

#define NS_NAVHISTORYSERVICE_CID \
{ 0x88cecbb7, 0x6c63, 0x4b3b, { 0x8c, 0xd4, 0x84, 0xf3, 0xb8, 0x22, 0x8c, 0x69 } }

#define NS_NAVHISTORYSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-history-service;1"

#define NS_NAVHISTORYRESULTTREEVIEWER_CID \
{ 0x2ea8966f, 0x0671, 0x4c02, { 0x9c, 0x70, 0x94, 0x59, 0x56, 0xd4, 0x54, 0x34 } }

#define NS_NAVHISTORYRESULTTREEVIEWER_CONTRACTID \
  "@mozilla.org/browser/nav-history/result-tree-viewer;1"

#define NS_ANNOTATIONSERVICE_CID \
{ 0x5e8d4751, 0x1852, 0x434b, { 0xa9, 0x92, 0x2c, 0x6d, 0x2a, 0x25, 0xfa, 0x46 } }

#define NS_ANNOTATIONSERVICE_CONTRACTID \
  "@mozilla.org/browser/annotation-service;1"

#define NS_NAVBOOKMARKSSERVICE_CID \
{ 0x9de95a0c, 0x39a4, 0x4d64, {0x9a, 0x53, 0x17, 0x94, 0x0d, 0xd7, 0xca, 0xbb}}

#define NS_NAVBOOKMARKSSERVICE_CONTRACTID \
  "@mozilla.org/browser/nav-bookmarks-service;1"

#define NS_FAVICONSERVICE_CID \
{ 0x984e3259, 0x9266, 0x49cf, { 0xb6, 0x05, 0x60, 0xb0, 0x22, 0xa0, 0x07, 0x56 } }

#define NS_FAVICONSERVICE_CONTRACTID \
  "@mozilla.org/browser/favicon-service;1"

#define NS_LIVEMARKSERVICE_CID \
{ 0xb1257934, 0x86cf, 0x4143, { 0x83, 0x86, 0x73, 0x4a, 0xc3, 0x52, 0xb6, 0xba } }

#define NS_LIVEMARKSERVICE_CONTRACTID \
  "@mozilla.org/browser/livemark-service;1"

#define NS_MORKHISTORYIMPORTER_CID \
{ 0x428e6d12, 0x9c6d, 0x436f, {0xb7, 0xa3, 0x6c, 0xa5, 0xf4, 0x80, 0x92, 0x12}}

#define NS_MORKHISTORYIMPORTER_CONTRACTID \
  "@mozilla.org/browser/history-importer;1"

#define NS_FEEDSNIFFER_CID \
{ 0x6893e69, 0x71d8, 0x4b23, { 0x81, 0xeb, 0x80, 0x31, 0x4d, 0xaf, 0x3e, 0x66 } }

#define NS_FEEDSNIFFER_CONTRACTID \
  "@mozilla.org/browser/feeds/sniffer;1"

#define NS_DOCNAVSTARTPROGRESSLISTENER_CID \
{ 0x7baf8179, 0xa4fd, 0x4bc0, { 0xbe, 0x43, 0xa9, 0xb1, 0x22, 0xc5, 0xde, 0xb6 } }

#define NS_DOCNAVSTARTPROGRESSLISTENER_CONTRACTID \
  "@mozilla.org/browser/safebrowsing/navstartlistener;1"
