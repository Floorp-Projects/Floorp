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

/*
 * The following source code is part of the Microline Widget Library.
 * The Microline widget library is made available to Mozilla developers
 * under the Netscape Public License (NPL) by Neuron Data.  To learn
 * more about Neuron Data, please visit the Neuron Data Home Page at
 * http://www.neurondata.com.
 */


#ifndef XmLFolderPH
#define XmLFolderPH

#include <Xm/XmP.h>

#ifdef MOTIF11
#else
#include <Xm/ManagerP.h>
#endif

#include "Folder.h"

typedef struct _XmLFolderPart
	{
	int debugLevel;
	Boolean serverDrawsArcsLarge;
	unsigned char cornerStyle, tabPlacement, resizePolicy;
	Boolean allowRotate, autoSelect;
	GC gc;
	Pixel inactiveBg, inactiveFg, blankBg;
	Pixmap blankPix;
	WidgetList tabs;
	int tabCount, tabAllocCount;
	Dimension marginWidth, marginHeight, spacing;
	Dimension cornerDimension, highlightThickness;
	Dimension pixmapMargin;
	Dimension tabHeight, tabWidth, tabBarHeight;
	int tabsPerRow, activeRow;
	XtTranslations primTrans;
	Widget focusW, activeW;
	int activeTab;
	char allowLayout;
	XtCallbackList activateCallback;
	XmFontList fontList;

	WidgetClass tabWidgetClass;

	} XmLFolderPart;

typedef struct _XmLFolderRec
	{
	CorePart core;
	CompositePart composite;
	ConstraintPart constraint;
	XmManagerPart manager;
	XmLFolderPart folder;
	} XmLFolderRec;

typedef struct _XmLFolderClassPart
	{
	int unused;
	} XmLFolderClassPart;

typedef struct _XmLFolderClassRec
	{
	CoreClassPart core_class;
	CompositeClassPart composite_class;
	ConstraintClassPart constraint_class;
	XmManagerClassPart manager_class;
	XmLFolderClassPart folder_class;
	} XmLFolderClassRec;

extern XmLFolderClassRec xmlFolderClassRec;

typedef struct _XmLFolderConstraintPart
	{
	Position x, y;
	Dimension width, height;
	Dimension maxPixWidth, maxPixHeight;
	Dimension pixWidth, pixHeight;
	Dimension inactPixWidth, inactPixHeight;
	int row;
	Boolean firstInRow;
	Boolean freePix;
	Pixmap pix, inactPix;
	char *managedName;
	Widget managedW;
	} XmLFolderConstraintPart;

typedef struct _XmLFolderConstraintRec
	{
	XmManagerConstraintPart manager;
	XmLFolderConstraintPart folder;
	} XmLFolderConstraintRec, *XmLFolderConstraintPtr;

#endif 
