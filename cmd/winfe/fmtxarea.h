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

#ifndef __FORM_TEXTAREA_H
#define __FORM_TEXTAREA_H

//	This file is dedicated to form type select one elements
//		otherwise known as list boxes on windows and
//		their implementation as requried by the XP layout
//		library.

//	Required includes.
#include "fmabstra.h"
#include "odctrl.h"

class CFormTextarea : public CFormElement	{
//	Construction/destruction.
//	Do not construct directly, use GetFormElement instead.
protected:
	CFormTextarea();
	virtual ~CFormTextarea();
    friend CFormElement;

//	The form element we represent.
//	This can be called at various times, so always update
//		our information when this function is called
//		(refresh cached or calculated XP data).
protected:
	virtual void SetElement(LO_FormElementStruct *pFormElement);

//	This is called to inform us of the owning context.
//	Once set, we should query it to decide how we will represent our
//		selves (printing through a DC, existing as a window, etc).
protected:
	virtual void SetContext(CAbstractCX *pCX);

//	Actions that we've been told to take.
public:
	virtual void DisplayFormElement(LTRB& Rect);
protected:
	//	Destroy the widget (window) implemenation of the form.
	virtual void DestroyWidget();

	//	Create the widget (window) implementation of the form
	//		but DO NOT DISPLAY.
	virtual void CreateWidget();

	//	Copy the current data out of the layout struct into the form
	//		widget, or mark that you should represent using the current data.
	virtual void UseCurrentData();

	//	Copy the default data out of the layout struct into the form
	//		widget, or mark that you should represent using the default data.
	virtual void UseDefaultData();

	//	Fill in the layout size information in the layout struct regarding
	//		the dimensions of the widget.
	virtual void FillSizeInfo();

	//	Copy the current data out of the form element back into the
	//		layout struct.
	virtual void UpdateCurrentData();

public:
    virtual HWND GetRaw();

//	The textarea widget.
private:
	CODNetscapeEdit *m_pWidget;

#ifdef XP_WIN16
//	The segment for the widget.
private:
	HGLOBAL m_hSegment;
#endif
};

#endif // __FORM_TEXTAREA_H
