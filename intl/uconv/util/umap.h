/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

typedef struct  {
	PRUint16 		itemOfList;
	PRUint16		offsetToFormatArray;
	PRUint16		offsetToMapCellArray;
	PRUint16		offsetToMappingTable;
	PRUint16		data[1];
} uTable;

#endif
