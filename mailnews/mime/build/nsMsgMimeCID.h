/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMessageMimeCID_h__
#define nsMessageMimeCID_h__

#define NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID \
  "@mozilla.org/messenger/headerparser;1"

#define NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID \
	NS_ISTREAMCONVERTER_KEY "?from=message/rfc822&to=text/xul"

#define NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID1 \
	NS_ISTREAMCONVERTER_KEY "?from=message/rfc822&to=text/html"

#define NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID2 \
	NS_ISTREAMCONVERTER_KEY "?from=message/rfc822&to=*/*"

#define NS_MAILNEWS_MIME_STREAM_CONVERTER_CID                    \
{ /* FAF4F9A6-60AD-11d3-989A-001083010E9B */         \
 0xfaf4f9a6, 0x60ad, 0x11d3, { 0x98, 0x9a, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b } }

// {866A1E11-D0B9-11d2-B373-525400E2D63A}
#define NS_MIME_CONVERTER_CID   \
        { 0x866a1e11, 0xd0b9, 0x11d2,         \
        { 0xb3, 0x73, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } }

// {403B0540-B7C3-11d2-B35E-525400E2D63A}
#define NS_MIME_OBJECT_CLASS_ACCESS_CID   \
        { 0x403b0540, 0xb7c3, 0x11d2,         \
        { 0xb3, 0x5e, 0x52, 0x54, 0x0, 0xe2, 0xd6, 0x3a } }
	
#endif // nsMessageMimeCID_h__
