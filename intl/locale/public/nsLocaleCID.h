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
#ifndef nsLocaleCID_h__
#define nsLocaleCID_h__

// {D92D57C3-BA1D-11d2-AF0C-0060089FE59B}
#define NS_WIN32LOCALE_CID								\
{	0xd92d57c3, 0xba1d, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }

#define NS_WIN32LOCALE_CONTRACTID "@mozilla.org/locale/win32-locale;1"

// {D92D57C4-BA1D-11d2-AF0C-0060089FE59B}
#define NS_MACLOCALE_CID								\
{	0xd92d57c4, 0xba1d, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }		

#define NS_MACLOCALE_CONTRACTID "@mozilla.org/locale/mac-locale;1"

// {D92D57C5-BA1D-11d2-AF0C-0060089FE59B}
#define NS_POSIXLOCALE_CID								\
{	0xd92d57c5, 0xba1d, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }

#define NS_POSIXLOCALE_CONTRACTID "@mozilla.org/locale/posix-locale;1"

// {F25F74F1-FB59-11d3-A9F2-00203522A03C}
#define NS_OS2LOCALE_CID			 \
{ 0xf25f74f1, 0xfb59, 0x11d3,                  \
{ 0xa9, 0xf2, 0x0, 0x20, 0x35, 0x22, 0xa0, 0x3c }}

#define NS_OS2LOCALE_CONTRACTID "@mozilla.org/locale/os2-locale;1"

#endif // nsLocaleCID_h__

