/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
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
