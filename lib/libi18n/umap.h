/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef __UMAP__
#define __UMAP__

#define NOMAPPING 0xfffd

/* =================================================
					MapCellArray
================================================= */

enum {
	uFormat0Tag = 0,
	uFormat1Tag,
	uFormat2Tag,
	uNumFormatTag
};

typedef struct {
		uint16 srcBegin;		/* 2 byte	*/
		uint16 srcEnd;			/* 2 byte	*/
		uint16 destBegin;		/* 2 byte	*/
} uFormat0;

typedef struct {
		uint16 srcBegin;		/* 2 byte	*/
		uint16 srcEnd;			/* 2 byte	*/
		uint16	mappingOffset;	/* 2 byte	*/
} uFormat1;

typedef struct {
		uint16 srcBegin;		/* 2 byte	*/
		uint16 srcEnd;			/* 2 byte	-waste	*/
		uint16 destBegin;		/* 2 byte	*/
} uFormat2;

typedef struct  {
	union {
		uFormat0	format0;
		uFormat1	format1;
		uFormat2	format2;
	} fmt;
} uMapCell;

/* =================================================
					uTable 
================================================= */
typedef struct  {
	uint16 		itemOfList;
	uint16		offsetToFormatArray;
	uint16		offsetToMapCellArray;
	uint16		offsetToMappingTable;
	uint16		data[1];
} uTable;

#endif
