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

//	UFormElementFactory.h

#pragma once

// PowerPlant
#include <PP_Types.h>

// Backend
#include "ntypes.h"	// for LO_* struct typedefs
#include "libi18n.h"

class CHTMLView;
class CNSContext;

class UFormElementFactory
{
public:
// Form Class Registration
	static	void	RegisterFormTypes();
	
// Form Element Creation
	static 	void	MakeFormElem(
							CHTMLView* inHTMLView,
							CNSContext*	inNSContext,
							LO_FormElementStruct* formElem
							);



	static	LPane*	MakeTextFormElem(
							CHTMLView* inHTMLView,
							CNSContext*	inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakeReadOnlyFormElem(
							CHTMLView* inHTMLView,
							CNSContext*	inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakeTextArea(
							CHTMLView* inHTMLView,
							CNSContext*	inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakeButton(
							CHTMLView* inHTMLView,
							CNSContext* inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakeFilePicker(
							CHTMLView* inHTMLView,
							CNSContext* inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakeToggle(
							CHTMLView* inHTMLView,
							CNSContext* inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakePopup (
							CHTMLView* inHTMLView,
							CNSContext* inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);
	static	LPane*	MakeList(
							CHTMLView* inHTMLView,
							CNSContext* inNSContext,
							Int32 &width,
							Int32 &height,
							Int32& baseline,
							LO_FormElementStruct *formElem
							);

// Form Element Destruction
	static	void	FreeFormElement(
							LO_FormElementData *formElem
							);

// Utility Routines

	static	void	DisplayFormElement(
							CNSContext* inNSContext,
							LO_FormElementStruct* formElem
							);
	static	void	ResetFormElement(
							LO_FormElementStruct *formElem,
							Boolean redraw,
							Boolean fromDefaults
							);
	static	void	SetFormElementToggle(
							LO_FormElementStruct* formElem,
							Boolean value
							);
	static	void	FormTextIsSubmit(
							LO_FormElementStruct * formElem
							);
	static	void	GetFormElementValue(
							LO_FormElementStruct *formElem,
							Boolean hide
							);
	static	void	HideFormElement(
							LO_FormElementStruct* formElem
							);
	static	void	ResetFormElementData(
							LO_FormElementStruct *formElem,
							Boolean redraw,
							Boolean fromDefaults,
							Boolean reflect
							);
};