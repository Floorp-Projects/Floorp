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
#ifndef __APIOUTLN_H
#define __APIOUTLN_H

#ifndef __APIAPI_H
    #include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
    #include "nsguids.h"
#endif

#define	APICLASS_OUTLINERPARENT  	"OutlinerParent"
#define APICLASS_OUTLINER			"Outliner"

typedef enum {
    ColumnFixed,
    ColumnVariable
} Column_t;

typedef enum {
    CropNone = 0,
    CropCenter,
    CropRight,
    CropLeft
} CropType_t;

typedef enum {
	AlignLeft,
	AlignRight,
	AlignCenter
} AlignType_t;

#define OUTLINER_RETURN         1
#define OUTLINER_LBUTTONDOWN	2
#define OUTLINER_LBUTTONUP		3
#define OUTLINER_RBUTTONDOWN	4
#define OUTLINER_RBUTTON		4	// For compatibility
#define OUTLINER_RBUTTONUP		5
#define OUTLINER_LBUTTONDBLCLK  6
#define OUTLINER_KEYDOWN		7
#define OUTLINER_PROPERTIES     8
#define OUTLINER_TIMER          9
#define OUTLINER_SET			10

class COutliner;		// temporary hack

class IOutliner {
public:
	virtual void EnableTips (
		BOOL = TRUE
		) = 0;

	virtual BOOL GetTipsEnabled (
		void 
		) = 0;

	virtual void SetCSID ( 
		int csid 
		) = 0;

	virtual int GetCSID (
		void 
		) = 0;

    virtual int AddColumn ( 
    	LPCTSTR header, 
    	UINT idCol, 
		int iMinCol, 
		int iMaxCol = 10000, 
		Column_t cType = ColumnFixed, 
		int iPercent = 50,
		BOOL bIsButton = TRUE, 
		CropType_t ct = CropRight, 
		AlignType_t at = AlignLeft 
		) = 0;

    virtual int GetColumnSize ( 
    	UINT idCol 
    	) = 0;

    virtual void SetColumnSize ( 
    	UINT idCol, 
    	int iSize 
    	) = 0;

    virtual int GetColumnPercent ( 
    	UINT idCol 
    	) = 0;

    virtual void SetColumnPercent ( 
    	UINT idCol, 
    	int iPercent 
    	) = 0;

	virtual int GetColumnPos( 
		UINT idCol 
		) = 0;

	virtual void SetColumnPos( 
		UINT idCol, 
		int iColumn 
		) = 0;

    virtual void SetColumnName ( 
    	UINT idCol, 
    	LPCTSTR pName 
    	) = 0;

	virtual LPCTSTR GetColumnName ( 
		UINT idCol 
		) = 0;

	virtual void SetImageColumn( 
		UINT idCol 
		) = 0;

	virtual void SetHasPipes( 
		BOOL bPipes 
		) = 0;

	virtual void SetVisibleColumns( 
		UINT iVisCol 
		) = 0;

	virtual UINT GetVisibleColumns(
		void
		) = 0;
    
    virtual void SelectItem ( 
    	int iSel, 
    	int mode = OUTLINER_SET, 
    	UINT flags = 0 
    	) = 0;

    virtual BOOL DeleteItem ( 
    	int iLine 
    	) = 0;

	virtual void ScrollIntoView( 
		int iVisibleLine 
		) = 0;

	virtual int GetFocusLine(
		void 
		) = 0;

	virtual void SetTotalLines( 
		int 
		) = 0;

	virtual int GetTotalLines(
		void 
		) = 0;
};

typedef IOutliner * LPIOUTLINER;
#define ApiOutliner(v,unk) APIPTRDEF(IID_IOutliner,IOutliner,v,unk)

class IOutlinerParent {
public:
    virtual void EnableBorder ( 
		BOOL = TRUE
    	) = 0;

    virtual void EnableHeaders ( 
		BOOL = TRUE
    	) = 0;

    virtual void SetOutliner ( 
    	COutliner * pIOutliner 
    	) = 0;

    virtual COutliner * GetOutliner ( 
    	void 
    	) = 0;

    virtual void CreateColumns ( 
    	void 
    	) = 0;

    virtual BOOL ColumnCommand ( 
    	int idColumn 
    	) = 0;

    virtual BOOL RenderData ( 
    	int idColumn, 
    	CRect & rect, 
    	CDC & dc, 
    	LPCTSTR lpsz = NULL 
    	) = 0;
};

typedef IOutlinerParent * LPIOUTLINERPARENT;
#define ApiOutlinerParent(v,unk) APIPTRDEF(IID_IOutlinerParent,IOutlinerParent,v,unk)

#endif

