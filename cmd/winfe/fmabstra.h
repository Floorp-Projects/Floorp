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

#ifndef __FORM_ABSTRACT_H
#define __FORM_ABSTRACT_H

/*  Author:
 *      Garrett Arch Blythe
 *      blythe@netscape.com
 */

//	Various form widgets used in derivations.
#include "medit.h"
#include "button.h"

//	Class to define common base functionality of all form elements
//		in the front end in an object oriented manner.

class CFormElement {
//	Construction/destruction.
//	Do not construct directly, use GetFormElement instead.
protected:
	CFormElement();
	virtual ~CFormElement();

//	End all form creation/retrieval routine.
public:
	static CFormElement *GetFormElement(CAbstractCX *pCX, LO_FormElementStruct *pFormElement);
	static CFormElement *GetFormElement(CAbstractCX *pCX, LO_FormElementData *pFormData);

//	The form element pointing to us.
//	A method to set/get it.
//	Override the method for casting in derived class (to a specific form element type).
private:
	LO_FormElementStruct *m_pFormElement;
protected:
	virtual void SetElement(LO_FormElementStruct *pFormElement);
public:
	LO_FormElementStruct *GetElement() const;
	LO_FormElementData *GetElementData() const;
	lo_FormElementTextData *GetElementTextData() const;
	lo_FormElementTextareaData *GetElementTextareaData() const;
	lo_FormElementMinimalData *GetElementMinimalData() const;
	lo_FormElementToggleData *GetElementToggleData() const;
	lo_FormElementSelectData *GetElementSelectData() const;
	void *GetElementFEData() const;

//	Each form element is of course owned by a specific context.
//	Might want to switch on context capabilities here
//		and perform cleanup (window to printing for example).
//		Override if needed.
private:
	CAbstractCX *m_pCX;
protected:
	virtual void SetContext(CAbstractCX *pCX);

public:
	CAbstractCX *GetContext() const;
	LO_TextAttr* GetTextAttr() {return GetElement()->text_attr;}
	void SetWidgetFont(HDC hdc, HWND hWidget);
//	Whether or not we will restore ourselves from default or current data held in the
//		LAYOUT structure.  Initially, it should always be from default, and use
//		current only when we've previously been turned off in GetFormElementValue.
//	Please do not make this public.  Its logic does not map to
//		whether or not the derived classes have the specified
//		behaviour, and it would be a mistake to assume so.
//		It is used to flag this class to make certain function calls.
private:
	BOOL m_bUseCurrentData;
	BOOL ShouldUseCurrentData() { return m_bUseCurrentData; }

//	Whether or not a widget is allocated and/or its size data is cached (does not
//		need to be recalculated).
//	Please do not make this public.  Its logic does not map to
//		whether or not the derived classes have a the specified
//		behaviour, and it would be a mistake to assume so.
//		It is used to flag this class to make certain function calls.
private:
	BOOL m_bWidgetPresent;
	BOOL IsWidgetPresent() { return m_bWidgetPresent; }

//	A way to get the next control ID for a form widget in a derived class.
private:
	static UINT m_uNextControlID;
protected:
	static UINT GetDynamicControlID();

//	Virts that might be overridden in rare cases (operations they must know
//		how to perform on themselves).  These are called by the owning
//		context of the same name (not args).
public:
	virtual void FreeFormElement(LO_FormElementData *pFormData);
	virtual void GetFormElementInfo();
	virtual void GetFormElementValue(BOOL bTurnOff);
	virtual void ResetFormElement();
	virtual void FormTextIsSubmit();
	virtual void SetFormElementToggle(BOOL bState);

//	Additional function to return secondary widgets (buttons, etc) in 
//	elements such as File forms.
	virtual CWnd *GetSecondaryWidget() {return 0;}

//	You probably want to override these!!!
public:
	virtual void DisplayFormElement(LTRB& Rect);

//	Non context funcs that should have been context funcs - whatever.
//	Add more as needed, of course.
//	Override if really needed.
public:
	virtual void FocusInputElement();
	virtual void BlurInputElement();
	virtual void SelectInputElement();
	virtual void ChangeInputElement();
    virtual void EnableInputElement();
    virtual void DisableInputElement();
	virtual void MoveWindow(HWND hWnd, int32 lX, int32 lY);

//	Funcs that must be overriden so that logic in base class implementations
//		will work correctly.
protected:
	//	Destroy the widget (window) implementation of the form.
	virtual void DestroyWidget() = 0;

	//	Create the widget (window) implementation of the form
	//		but DO NOT DISPLAY.
	virtual void CreateWidget() = 0;

	//	Copy the current data out of the layout struct into the form
	//		widget, or mark that you should represent using the current data.
	virtual void UseCurrentData() = 0;

	//	Copy the default data out of the layout struct into the form
	//		widget, or mark that you should represent using the default data.
	virtual void UseDefaultData() = 0;

	//	Fill in the layout size information in the layout struct regarding
	//		the dimensions of the widget.
	virtual void FillSizeInfo() = 0;

	//	Copy the current data out of the form element back into the
	//		layout struct.
	virtual void UpdateCurrentData() = 0;

public:
    virtual HWND GetRaw() = 0;

//	Misc helpers called by derived classes.
protected:
#ifdef XP_WIN16
	//	Call this to get the allocated segment for each edit field under 16 bits.
	HINSTANCE GetSegment();
#endif
};

#endif // __FORM_ABSTRACT_H
