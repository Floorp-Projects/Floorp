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


#ifndef XmLTreeH
#define XmLTreeH

#include <XmL/XmL.h>
#include <XmL/Grid.h>

#ifdef XmL_CPP
extern "C" {
#endif

extern WidgetClass xmlTreeWidgetClass;
typedef struct _XmLTreeClassRec *XmLTreeWidgetClass;
typedef struct _XmLTreeRec *XmLTreeWidget;
typedef struct _XmLTreeRowRec *XmLTreeRow;

#define XmLIsTree(w) XtIsSubclass((w), xmlTreeWidgetClass)

Widget XmLCreateTree(Widget parent, char *name, ArgList arglist,
	Cardinal argcount);
void XmLTreeAddRow(Widget w, int level, Boolean expands, Boolean isExpaned,
	int position, Pixmap pixmap, Pixmap pixmask, XmString string);
void XmLTreeAddRows(Widget w, XmLTreeRowDefinition *rows,
	int count, int position);
void XmLTreeDeleteChildren(Widget w, int position);

#ifdef XmL_CPP
}
#endif
#endif
