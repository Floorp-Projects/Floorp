/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// ——— Security
//#define NADA_VERSION
//#define EXPORT_VERSION
#define US_VERSION

// ——— Misc
//#define NO_DBM		// define this to kill DBM
#define NEW_BOOKMARKS
// Enables us to switch profiling from project preferences

// ——— Version
//#define ALPHA
//#define BETA
// Comment out both ALPHA and BETA for the final version

// 98-06-03 pinkerton -- temorary defines to turn on features before they fully land.
#define CookieManagement 1
#define SingleSignon 1
#define ClientWallet 1

// 98-07-29 pinnkerton -- defines to turn on feature. REMOVE WHEN THIS LANDS.
#define PRIVACY_POLICIES 1

// 98-08-10 joe -- temporary item to turn on ENDER (html textareas) before it fully lands
#define ENDER 1

// 98-09-25 mlm - turn on javascript thread safety
#define JS_THREADSAFE 1

// 98-10-14 joe -- temporary item to turn on ENDER MIME support before it fully lands
#define MOZ_ENDER_MIME 1

#define USE_NSREG 1

// ———————————————————————————————————————————————————————————————————————————
//	•	You typically will not need to change things below here
// ———————————————————————————————————————————————————————————————————————————

#define MOCHA
#define MOZILLA_CLIENT	1
#ifndef NETSCAPE
#define NETSCAPE	1
#endif

// #define JAVA 	1
#define OJI		1

#ifdef JAVA
	#define UNICODE_FONTLIST 1
#endif

#define LAYERS	1
// #define NU_CACHE 1 // uncomment to turn on new memory cache features

#define CASTED_READ_OBJECT(stream, type, reference) (reference = NULL)

/* Defined in javaStubs prefix files
#define VERSION_NUMBER "4_0b0"
#define ZIP_NAME "java"##VERSION_NUMBER
*/

