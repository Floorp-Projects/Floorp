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

#include "nscore.h"
#include "nsISupports.h"

// {DAD86119-B643-11d2-AF0B-0060089FE59B}
#define NS_LOCALE_CID									\
{	0xdad86119, 0xb643, 0x11d2,							\
{	0xaf, 0xb, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }


// {DAD86117-B643-11d2-AF0B-0060089FE59B}				
#define NS_LOCALEFACTORY_CID							\
{	0xdad86117, 0xb643, 0x11d2,							\
{	0xaf, 0xb, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }

// {D92D57C3-BA1D-11d2-AF0C-0060089FE59B}
#define NS_WIN32LOCALE_CID								\
{	0xd92d57c3, 0xba1d, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }

// {9ADEE891-BAF8-11d2-AF0C-0060089FE59B}
#define NS_WIN32LOCALEFACTORY_CID						\
{	0x9adee891, 0xbaf8, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }

// {D92D57C4-BA1D-11d2-AF0C-0060089FE59B}
#define NS_MACLOCALE_CID								\
{	0xd92d57c4, 0xba1d, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }		

// {8C0E453F-EC7E-11d2-9E89-0060089FE59B}
#define NS_MACLOCALEFACTORY_CID         \
{ 0x8c0e453f, 0xec7e, 0x11d2,           \
{ 0x9e, 0x89, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}

// {D92D57C5-BA1D-11d2-AF0C-0060089FE59B}
#define NS_POSIXLOCALE_CID								\
{	0xd92d57c5, 0xba1d, 0x11d2,							\
{	0xaf, 0xc, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b } }

// {8C0E4531-EC7E-11d2-9E89-0060089FE59B}
#define NS_POSIXLOCALEFACTORY_CID          \
{ 0x8c0e4531, 0xec7e, 0x11d2,              \
{ 0x9e, 0x89, 0x0, 0x60, 0x8, 0x9f, 0xe5, 0x9b }}

// {F25F74F1-FB59-11d3-A9F2-00203522A03C}
#define NS_OS2LOCALE_CID			 \
{ 0xf25f74f1, 0xfb59, 0x11d3,                  \
{ 0xa9, 0xf2, 0x0, 0x20, 0x35, 0x22, 0xa0, 0x3c }}

// {F25F74F2-FB59-11d3-A9F2-00203522A03C}
#define NS_OS2LOCALEFACTORY_CID      \
{ 0xf25f74f2, 0xfb59, 0x11d3,                  \
{ 0xa9, 0xf2, 0x0, 0x20, 0x35, 0x22, 0xa0, 0x3c  }}

#endif  // nsLocaleCID_h__

