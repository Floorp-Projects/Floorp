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


#ifndef XmLFolderH
#define XmLFolderH

#include <XmL/XmL.h>

#ifdef XmL_CPP
extern "C" {
#endif

extern WidgetClass xmlFolderWidgetClass;
typedef struct _XmLFolderClassRec *XmLFolderWidgetClass;
typedef struct _XmLFolderRec *XmLFolderWidget;

#define XmLIsFolder(w) XtIsSubclass((w), xmlFolderWidgetClass)

Widget XmLCreateFolder(Widget parent, char *name, ArgList arglist,
		       Cardinal argcount);
Widget XmLFolderAddBitmapTab(Widget w, XmString string,
			     char *bitmapBits, int bitmapWidth, int bitmapHeight);
Widget XmLFolderAddBitmapTabForm(Widget w, XmString string,
				 char *bitmapBits, int bitmapWidth, int bitmapHeight);
Widget XmLFolderAddTab(Widget w, XmString string);
Widget XmLFolderAddTabFromClass(Widget w, XmString string);
Widget XmLFolderAddTabForm(Widget w, XmString string);
void XmLFolderSetActiveTab(Widget w, int position, Boolean notify);

#ifdef XmL_CPP
}
#endif
#endif
