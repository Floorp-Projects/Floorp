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

//------------------------------------------------------------------------------
//      Author: Frank Tang ftang@netscape.com x2913
//
//      Virtual Font Cacahe used for Unicode rendering
//------------------------------------------------------------------------------
#ifndef   __CVFFC_H
#define   __CVFFC_H
#include "stdafx.h"

class CVirtualFontFontCache {
public:
static void Init();
static void Exit();
static void Reset();

static void ResetCyacache();
static BOOL Get(	int16 encoding, int size, BOOL fixed, BOOL bold,
			BOOL italic, BOOL underline, CyaFont*& pFont );
static BOOL Add(	int16 encoding, int size, BOOL fixed, BOOL bold,
			BOOL italic, BOOL underline, CyaFont*& pFont );

static BOOL Get(	int16 encoding, int size, BOOL fixed, BOOL bold,
			BOOL italic, BOOL underline, CFont*& pFont );
static BOOL Add(	int16 encoding, int size, BOOL fixed, BOOL bold,
			BOOL italic, BOOL underline, CFont*& pFont );


private:
static CMapWordToOb* m_cache;

static CMapWordToOb* m_Cyacache;
};
#endif
