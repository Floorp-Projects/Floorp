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

// apiprint.h contains the page setup api

#ifndef __APIPAGE_H
#define __APIPAGE_H

#ifndef __APIAPI_H
	#include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
	#include "nsguids.h"
#endif

// APIID_Printer (IPrinter interface)
// this interface is used to retrieve and set information from
// the page setup object.   The functions are pretty self
// explanatory.  A -1 gets the current value in the single
// argument function calls.

class IPageSetup {
public:

	virtual	void GetMargins ( 
		long * plLeft,
		long * plRight,
		long * plTop,
		long * plBottom 
		) = 0;

	virtual void SetMargins (
		long lLeft,
		long lRight,
		long lTop,
		long lBottom
		) = 0;

    virtual void GetPageSize (
        long * lWidth,
        long * lHeight
        ) = 0;

    virtual void SetPageSize (
        long lWidth,
        long lHeight
        ) = 0;
    virtual void SetPrintingBkImage (BOOL flag) = 0;
    virtual BOOL IsPrintingBkImage (void) = 0;

#define	PRINT_TITLE		1
#define PRINT_URL		2

	// -1 = Get state
	//  0 = disable
	//
	//  1 (01) = title
	//  2 (10) = location (URL)
	//  combinable with OR

	virtual int Header ( 
		int flag = -1
		) = 0;

#define	PRINT_PAGENO	1
#define PRINT_PAGECOUNT	2
#define	PRINT_DATE		4

	// -1 = Get state
	//  0 = disable
	//
	//  1 (001) = page #
	//  2 (010) = total page count
	//  4 (100) = date printed
	//  combinable with OR

	virtual int Footer (
		int flag = -1
		) = 0;

	virtual int SolidLines (
		int flag = -1
		) = 0;
		
	virtual int BlackText (
		int flag = -1
		) = 0;

	virtual int BlackLines (
		int flag = -1
		) = 0;

    virtual int ReverseOrder (
        int flag = -1
        ) = 0;
};

typedef IPageSetup * LPPAGESETUP;

#define	APICLASS_PAGESETUP	"PageSetup"
#define ApiPageSetup(v,unk) APIPTRDEF(IID_IPageSetup,IPageSetup,v,unk)

#endif
