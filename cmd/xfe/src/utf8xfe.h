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
/* 
   utf8xfe.h UTF8 to XmString conversion and XmFontList monitor stuff
   we need for RDF related UI
   Created: Frank Tang tftang@netscape.com>, 30-Sep-98
 */
#ifndef __utfxfe_h__
#define __utfxfe_h__
#include "mozilla.h"
#include "xfe.h"
#include "fonts.h"

class  FontListNotifier {
public:
	virtual void notifyFontListChanged(XmFontList inFontList) = 0;
};

class UTF8ToXmStringConverter
{
public:
	virtual XmString convertToXmString(const char* utf8text) = 0;
};

class UTF8ToXmStringConverterFactory {
public:	
	static UTF8ToXmStringConverter* make(
		FontListNotifier* inNotifier, fe_Font inUFont);
};

class UnicodePseudoFontFactory
{
public:
	static fe_Font  make(Display* dpy, XmFontList defFontList);
	static fe_Font  make(Display* dpy, XFontStruct *xfstruc);
	static fe_Font  make(Display* dpy, XFontSet  xfset);
	static fe_Font  make(Display* dpy, const char* fam, int pt);
};

#endif /* __utfxfe_h__ */
