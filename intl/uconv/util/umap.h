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
		uint16_t srcBegin;		/* 2 byte	*/
		uint16_t srcEnd;			/* 2 byte	*/
		uint16_t destBegin;		/* 2 byte	*/
} uFormat0;

typedef struct {
		uint16_t srcBegin;		/* 2 byte	*/
		uint16_t srcEnd;			/* 2 byte	*/
		uint16_t	mappingOffset;	/* 2 byte	*/
} uFormat1;

typedef struct {
		uint16_t srcBegin;		/* 2 byte	*/
		uint16_t srcEnd;			/* 2 byte	-waste	*/
		uint16_t destBegin;		/* 2 byte	*/
} uFormat2;

typedef struct  {
	union {
		uFormat0	format0;
		uFormat1	format1;
		uFormat2	format2;
	} fmt;
} uMapCell;

#define UMAPCELL_SIZE (3*sizeof(uint16_t))

typedef struct  {
	uint16_t 		itemOfList;
	uint16_t		offsetToFormatArray;
	uint16_t		offsetToMapCellArray;
	uint16_t		offsetToMappingTable;
	uint16_t		data[1];
} uTable;

#endif
