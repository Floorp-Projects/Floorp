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


#ifndef XmLProgressPH
#define XmLProgressPH

#include <Xm/XmP.h>
#include <X11/StringDefs.h>
#ifdef MOTIF11
#else
#include <Xm/PrimitiveP.h>
#include <Xm/DrawP.h>
#endif

#include <sys/types.h>
#include "Progress.h"

typedef struct _XmLProgressPart
	{
	int completeValue, value;
	Boolean showTime;
	Boolean showPercentage;
	XmFontList fontList;
	GC gc;
	time_t startTime;
	XFontStruct *fallbackFont;
	unsigned char meterStyle;
	int numBoxes;
	} XmLProgressPart;

typedef struct _XmLProgressRec
	{
	CorePart core;
	XmPrimitivePart primitive;
	XmLProgressPart progress;
	} XmLProgressRec;

typedef struct _XmLProgressClassPart
	{
	int null;
	} XmLProgressClassPart;

typedef struct _XmLProgressClassRec
	{
	CoreClassPart core_class;
	XmPrimitiveClassPart primitive_class;
	XmLProgressClassPart progress_class;
	} XmLProgressClassRec;

extern XmLProgressClassRec xmlProgressClassRec;

#endif
