/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
