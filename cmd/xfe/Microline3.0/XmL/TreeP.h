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


#ifndef XmLTreePH
#define XmLTreePH

#include <Xm/XmP.h>
#ifndef MOTIF11
#include <Xm/ManagerP.h>
#include <Xm/DrawP.h>
#endif

#include "Tree.h"
#include "GridP.h"

/* row value mask for get/set values */
#define RVML XmLGridRowValueMaskLen
#define XmLTreeRowLevel      (1L << (RVML))
#define XmLTreeRowExpands    (1L << (RVML + 1))
#define XmLTreeRowIsExpanded (1L << (RVML + 2))
 
typedef struct _XmLTreeRowPart
	{
	Boolean expands;
	int level;
	Boolean hasChildren, hasSiblings, isExpanded;
	Dimension stringWidth;
	Boolean stringWidthValid;
	} XmLTreeRowPart;

struct _XmLTreeRowRec
	{
	XmLGridRowPart grid;
	XmLTreeRowPart tree;
	};

typedef struct _XmLTreePart
	{
	/* resources */
	Dimension levelSpacing;
	Pixel lineColor, pmColor;
	XtCallbackList collapseCallback, expandCallback;

	/* private data */
	char *linesData;
	int linesSize, linesMaxLevel;
	int recalcTreeWidth;

	char defaultPixmapsCreated;
	Pixel pixColors[4];
	Pixmap filePixmask, folderPixmask, folderOpenPixmask;
	Pixmap filePixmap, folderPixmap, folderOpenPixmap;

	/* row resources */
	int rowLevel;
	Boolean rowExpands, rowIsExpanded;
	} XmLTreePart;

typedef struct _XmLTreeRec
	{
	CorePart core;
	CompositePart composite;
	ConstraintPart constraint;
	XmManagerPart manager;
	XmLGridPart grid;
	XmLTreePart tree;
	} XmLTreeRec;

typedef struct _XmLTreeClassPart
	{
	int unused;
	} XmLTreeClassPart;

typedef struct _XmLTreeClassRec
	{
	CoreClassPart core_class;
	CompositeClassPart composite_class;
	ConstraintClassPart constraint_class;
	XmManagerClassPart manager_class;
	XmLGridClassPart grid_class;
	XmLTreeClassPart tree_class;
	} XmLTreeClassRec;

extern XmLTreeClassRec xmlTreeClassRec;

typedef struct _XmLTreeConstraintPart
	{
	int unused;
	} XmLTreeConstraintPart;

typedef struct _XmLTreeConstraintRec
	{
	XmManagerConstraintPart manager;
	XmLGridConstraintPart grid;
	XmLTreeConstraintPart tree;
	} XmLTreeConstraintRec, *XmLTreeConstraintPtr;

#endif 
