/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __UMAP__
#define __UMAP__

#define NOMAPPING 0xfffd

enum {
	uFormat0Tag = 0,
	uFormat1Tag,
	uFormat2Tag,
	uNumFormatTag
};

typedef struct {
		PRUint16 srcBegin;		/* 2 byte	*/
		PRUint16 srcEnd;			/* 2 byte	*/
		PRUint16 destBegin;		/* 2 byte	*/
} uFormat0;

typedef struct {
		PRUint16 srcBegin;		/* 2 byte	*/
		PRUint16 srcEnd;			/* 2 byte	*/
		PRUint16	mappingOffset;	/* 2 byte	*/
} uFormat1;

typedef struct {
		PRUint16 srcBegin;		/* 2 byte	*/
		PRUint16 srcEnd;			/* 2 byte	-waste	*/
		PRUint16 destBegin;		/* 2 byte	*/
} uFormat2;

typedef struct  {
	union {
		uFormat0	format0;
		uFormat1	format1;
		uFormat2	format2;
	} fmt;
} uMapCell;

#define UMAPCELL_SIZE (3*sizeof(PRUint16))

typedef struct  {
	PRUint16 		itemOfList;
	PRUint16		offsetToFormatArray;
	PRUint16		offsetToMapCellArray;
	PRUint16		offsetToMappingTable;
	PRUint16		data[1];
} uTable;

#endif
