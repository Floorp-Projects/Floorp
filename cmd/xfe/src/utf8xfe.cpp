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
   utf8xfe.cpp UTF8 to XmString conversion and XmFontList monitor stuff
   we need for RDF related UI
   Created: Frank Tang tftang@netscape.com>, 30-Sep-98
 */
#include "utf8xfe.h"
#include "csid.h"
#include "xupfonts.h"
#include "felocale.h"


//---------------------------------------------------------------
class UTF8TextAndFontListTracer : public UTF8ToXmStringConverter
{
public:
	UTF8TextAndFontListTracer(
		FontListNotifier *inNotifier, 
		fe_Font inUFont);
	virtual ~UTF8TextAndFontListTracer();

	virtual XmString convertToXmString(const char* utf8text);
protected:
	int numberOfFonts(fe_Font f);
	void notifyFontListChanges(XmFontList fontlist);
private:
	fe_Font fUFont;
	FontListNotifier* fNotifier;
	int fLastNumberOfFonts;
};
//---------------------------------------------------------------
class WidgetFontListNotifier : public FontListNotifier {
public:
	WidgetFontListNotifier(Widget inWidget, String inParam) 
		{ fWidget = inWidget; fParam = inParam; }
	virtual void notify(XmFontList inFontList);
private:
	Widget fWidget;
	String	fParam;
};




void WidgetFontListNotifier::notify(XmFontList inFontlist)
{
	XtVaSetValues(fWidget, fParam, inFontlist, NULL);
}
//---------------------------------------------------------------
fe_Font UnicodePseudoFontFactory::make( Display *dpy)
{
	return (fe_Font) fe_LoadUnicodeFont(
		NULL,
		"",
		0,
		2,
		0,
		CS_UTF8,
		0,
		0,
		dpy
	);
}
//---------------------------------------------------------------
FontListNotifier* FontListNotifierFactory::make(
	Widget inWidget,
	String inParam
)
{
	return new WidgetFontListNotifier(inWidget, inParam);
}
//---------------------------------------------------------------
UTF8ToXmStringConverter* UTF8ToXmStringConverterFactory::make(
	FontListNotifier* inNotifier,
	fe_Font	inFont
)
{
	return new UTF8TextAndFontListTracer(inNotifier, inFont);
}
 
//---------------------------------------------------------------
UTF8TextAndFontListTracer::UTF8TextAndFontListTracer( 
	FontListNotifier *inNotifier,
	fe_Font inUFont
)
{
	fNotifier = inNotifier;
	fLastNumberOfFonts = 0;
	fUFont = inUFont;
}

UTF8TextAndFontListTracer::~UTF8TextAndFontListTracer()
{
	delete fNotifier;
	// fe_freeUnicodePseudoFont(fUFont);
}
int UTF8TextAndFontListTracer::numberOfFonts(fe_Font f)
{
	fe_UnicodePseudoFont *ufont = (fe_UnicodePseudoFont*) f;
	int i,num;
	for(i=num=0; i < INTL_CHAR_SET_MAX; i++)
		if(TRUE == ufont->xfont_inited[i])
			num++;
	return num;
}
XmString UTF8TextAndFontListTracer::convertToXmString(const char* utf8text)
{
	XmFontList *pFontlist;
	XmFontType type = XmFONT_IS_FONT;
	XmString xmstr = fe_ConvertToXmString((unsigned char*)utf8text, CS_UTF8, fUFont, type , pFontlist);

	int newNumberOfFonts = numberOfFonts(fUFont);
	if(fLastNumberOfFonts != newNumberOfFonts)
	{	
		fNotifier->notify(*pFontlist);
		fLastNumberOfFonts = newNumberOfFonts; 
	}
	return xmstr;
}
