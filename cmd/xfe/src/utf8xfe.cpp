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
fe_Font UnicodePseudoFontFactory::make( Display *dpy, XmFontList defFontList)
{
	fe_Font ret = NULL;
	XmFontContext context;
	XmFontListEntry entry;
	XmFontListInitFontContext(&context, defFontList);
	while(NULL != (entry = XmFontListNextEntry(context)))
	{
		XtPointer font;
		XmFontType type;
		char* tag;

		tag = XmFontListEntryGetTag(entry);
		if((NULL == ret) ||
                   (0 == strcmp(XmFONTLIST_DEFAULT_TAG, tag)))
		{
			font = XmFontListEntryGetFont(entry, &type);
			if(XmFONT_IS_FONT == type)
				ret = UnicodePseudoFontFactory::make(dpy, 
                                         (XFontStruct*) font);
			else
				ret = UnicodePseudoFontFactory::make(dpy, 
                                         (XFontSet) font);
		}
		free(tag);
	}
	XmFontListFreeFontContext(context);
	return ret;
}
fe_Font UnicodePseudoFontFactory::make( Display *dpy, XFontSet xfset)
{
	char **fnl;
	XFontStruct ** fsl;
	if(XFontsOfFontSet(xfset, &fsl, &fnl)>0)
		return UnicodePseudoFontFactory::make(dpy, fsl[0]);
	else
		return UnicodePseudoFontFactory::make(dpy, "", 120);
}
fe_Font UnicodePseudoFontFactory::make( Display *dpy, XFontStruct* xfstruct)
{
	Atom fmlAtom;
	char fam[32];
	int ptSize = 0;

	if(XGetFontProperty( xfstruct, XA_FAMILY_NAME, (unsigned long* )&fmlAtom))
	{
		char * famName = XGetAtomName(dpy, fmlAtom);
		PR_snprintf(fam, 31, famName);
		fam[31] = '\0';
		XFree((void*)famName);
	} 
	else
	{
		PR_snprintf(fam, 31, "");
	}

	if(! XGetFontProperty( xfstruct, XA_POINT_SIZE, (unsigned long*)&ptSize))
	    ptSize = 120;	
	return UnicodePseudoFontFactory::make(dpy, fam, ptSize);
}
extern "C" {
extern int fe_UnicodePointToPixelSize(Display *dpy, int ptSize);
};
fe_Font UnicodePseudoFontFactory::make( Display *dpy, const char* family, int ptSize)
{
	return (fe_Font) fe_LoadUnicodeFont(
		NULL,
		(char*)family,
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
	XmFontList fontlist;
	XmFontType type = XmFONT_IS_FONT;
	XmString xmstr = fe_ConvertToXmString((unsigned char*)utf8text, CS_UTF8, fUFont, type , &fontlist);

	int newNumberOfFonts = numberOfFonts(fUFont);
	if(fLastNumberOfFonts != newNumberOfFonts)
	{	
		fNotifier->notifyFontListChanged(fontlist);
		fLastNumberOfFonts = newNumberOfFonts; 
	}
	return xmstr;
}


/* some debugger function here */
void DebugXmString(XmString inString)
{
	XmStringContext context;
	char *text;
	XmStringCharSet tag;
	XmStringDirection dir;
	Boolean sep;
	if(! XmStringInitContext(&context, inString) )
	{
		fprintf(stderr, "cannot exam this XmString\n");
		return;
	}
	fprintf(stderr, "dump XmString:");
	while(XmStringGetNextSegment(context, &text,&tag, &dir,&sep))
	{
		fprintf(stderr,"[%s:%s:%s:%s:%s]",
			(sep ? "Sep" : ""),
			((dir == XmSTRING_DIRECTION_L_TO_R) ? "L2R" :
			 ((dir == XmSTRING_DIRECTION_R_TO_L) ? "R2L" : "Def")),
			tag,
			text,
			(sep ? "Sep" : ""));
		
		XtFree(text);
	}	
	fprintf(stderr,"\n");
	fflush(stderr);	
	XmStringFreeContext(context);	
}
void DebugFontList(XmFontList inFontList)
{
	XmFontContext context;
	XmFontListEntry entry;
	XmFontListInitFontContext(&context, inFontList);
	fprintf(stderr, "dump XmFontList:");
	while(NULL != (entry = XmFontListNextEntry(context)))
	{
		XtPointer font;
		XmFontType type;
		char* tag;
		font = XmFontListEntryGetFont(entry, &type);
		tag = XmFontListEntryGetTag(entry);
		fprintf(stderr, "[%s:%s:%xl]",
			((type == XmFONT_IS_FONT) ? "F" : "L"),
			tag, (int)font );
		free(tag);
	}
	fprintf(stderr,"\n");
	XmFontListFreeFontContext(context);
}



