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

// CPlainTextConversionContext.cp


// This might seem like a lot of work to go around the code in ns/lib/xlate/text.c,
// but doing this allows us not to have to put if (inContext == NULL) inside
// CNSContextCallbacks.cp; and by calling the TXFE_* functions from the overridden
// methods, we pickup any modifcations made for free.

//#include <yvals.h>

#include "CPlainTextConversionContext.h"

#include "xlate.h"

//_EXTERN_C
__extern_c

// prototypes for text.c functions

extern void TXFE_DisplayTable(MWContext *cx, int iLoc, LO_TableStruct *table);
extern void TXFE_DisplayLineFeed(MWContext *cx, int iLocation, LO_LinefeedStruct *line_feed, XP_Bool notused);
extern void TXFE_DisplayHR(MWContext *cx, int iLocation , LO_HorizRuleStruct *HR);
extern char *TXFE_TranslateISOText(MWContext *cx, int charset, char *ISO_Text);
extern void TXFE_DisplayBullet(MWContext *cx, int iLocation, LO_BullettStruct *bullet);
extern void TXFE_FinishedLayout(MWContext *cx);
extern void TXFE_AllConnectionsComplete(MWContext *cx);
extern void TXFE_DisplaySubtext(MWContext *cx, int iLocation, LO_TextStruct *text,
		    int32 start_pos, int32 end_pos, XP_Bool notused);
extern void TXFE_DisplayText(MWContext *cx, int iLocation, LO_TextStruct *text, XP_Bool needbg);
extern void TXFE_DisplaySubDoc(MWContext *cx, int iLocation, LO_SubDocStruct *subdoc_struct);
extern int TXFE_GetTextInfo(MWContext *cx, LO_TextStruct *text, LO_TextInfo *text_info);
extern void TXFE_LayoutNewDocument(MWContext *cx, URL_Struct *url, int32 *w, int32 *h, int32* mw, int32* mh);

// These are here because the backend files are .c files, which are only
// run through the C compiler. Thus we need to create and destroy the
// CPlainTextConversionContext from a .cp file

MWContext* CreatePlainTextConversionContext(MWContext* inUIContext);
void DisposePlainTextConversionContext(MWContext* inContext);

__end_extern_c

#pragma mark --- CALLBACKS ---

MWContext* CreatePlainTextConversionContext(MWContext* inUIContext)
{
	try {
		CPlainTextConversionContext* theContext = new CPlainTextConversionContext(inUIContext);
		// Very slimey, but somebody needs to have an interest in the context
		theContext->AddUser(theContext);
		return theContext->operator MWContext*();
	} catch (...) {
		return NULL;
	}
}

void DisposePlainTextConversionContext(MWContext* inContext)
{
	CPlainTextConversionContext* theContext =
		dynamic_cast<CPlainTextConversionContext*>(ExtractNSContext(inContext));
	Assert_(theContext != NULL);
	// One of these days, this call might break
	theContext->RemoveUser(theContext);
}

CPlainTextConversionContext::CPlainTextConversionContext(MWContext* inUIContext) :
	CNSContext(MWContextText)
{
	mUIContext = ExtractNSContext(inUIContext);
	Assert_(mUIContext != NULL);
}

#pragma mark --- OVERRIDES ---

void CPlainTextConversionContext::LayoutNewDocument(
	URL_Struct*				inURL,
	Int32*					inWidth,
	Int32*					inHeight,
	Int32*					inMarginWidth,
	Int32*					inMarginHeight)
{
	TXFE_LayoutNewDocument(*this, inURL, inWidth, inHeight, inMarginWidth, inMarginHeight);
}

void CPlainTextConversionContext::DisplaySubtext(
	int 					inLocation,
	LO_TextStruct*			inText,
	Int32 					inStartPos,
	Int32					inEndPos,
	XP_Bool 				inNeedBG)
{
	TXFE_DisplaySubtext(*this, inLocation, inText, inStartPos, inEndPos, inNeedBG);
}

void CPlainTextConversionContext::DisplayText(
	int 					inLocation,
	LO_TextStruct*			inText,
	XP_Bool 				inNeedBG)
{
	TXFE_DisplayText(*this, inLocation, inText, inNeedBG);
}

void CPlainTextConversionContext::DisplaySubDoc(
	int 					inLocation,
	LO_SubDocStruct*		inSubdocStruct)
{
	TXFE_DisplaySubDoc(*this, inLocation, inSubdocStruct);
}

void CPlainTextConversionContext::DisplayTable(
	int 					inLocation,
	LO_TableStruct*			inTableStruct)
{
	TXFE_DisplayTable(*this, inLocation, inTableStruct);
}

void CPlainTextConversionContext::DisplayLineFeed(
	int 					inLocation,
	LO_LinefeedStruct*		inLinefeedStruct,
	XP_Bool 				inNeedBG)
{
	TXFE_DisplayLineFeed(*this, inLocation, inLinefeedStruct, inNeedBG);
}

void CPlainTextConversionContext::DisplayHR(
	int 					inLocation,
	LO_HorizRuleStruct*		inRuleStruct)
{
	TXFE_DisplayHR(*this, inLocation, inRuleStruct);
}

char* CPlainTextConversionContext::TranslateISOText(
	int 					inCharset,
	char*					inISOText)
{
	return TXFE_TranslateISOText(*this, inCharset, inISOText);
}

void CPlainTextConversionContext::DisplayBullet(
	int 					inLocation,
	LO_BullettStruct*		inBulletStruct)
{
	TXFE_DisplayBullet(*this, inLocation, inBulletStruct);
}

void CPlainTextConversionContext::FinishedLayout(void)
{
	TXFE_FinishedLayout(*this);
}

int CPlainTextConversionContext::GetTextInfo(
	LO_TextStruct*			inText,
	LO_TextInfo*			inTextInfo)
{
	return TXFE_GetTextInfo(*this, inText, inTextInfo);
}

int CPlainTextConversionContext::MeasureText(
	LO_TextStruct*			/*inText*/,
	short*					/*outCharLocs*/)
{
	return 0;
}

void CPlainTextConversionContext::AllConnectionsComplete(void)
{
	if (mProgress)
	{
		mProgress->RemoveUser(this);
		mProgress = NULL;
	}
	TXFE_AllConnectionsComplete(*this);
	mUIContext->AllConnectionsComplete();
	CNSContext::AllConnectionsComplete();
}

void CPlainTextConversionContext::GraphProgressInit(
	URL_Struct*		inURL,
	Int32			inContentLength)
{
	try {
		Assert_(mUIContext != NULL);
		if (mUIContext && mUIContext->GetContextProgress())
			mProgress = mUIContext->GetContextProgress();
		else
		{
			mProgress = new CContextProgress;
			mUIContext->SetContextProgress(mProgress);
		}
		mProgress->AddUser(this);
	} catch (...) {
		mProgress = NULL;
	}
	mUIContext->GraphProgressInit(inURL, inContentLength);
}

void CPlainTextConversionContext::Progress(const char* inMessageText )
{
	Assert_(mUIContext != NULL);
	mUIContext->Progress(inMessageText);
}

void CPlainTextConversionContext::GraphProgressDestroy(
	URL_Struct*		inURL,
	Int32 			inContentLength,
	Int32 			inTotalRead)
{
	Assert_(mUIContext != NULL);
	mUIContext->GraphProgressDestroy(inURL, inContentLength, inTotalRead);
}

void CPlainTextConversionContext::GraphProgress(
	URL_Struct*		inURL,
	Int32			inBytesReceived,
	Int32			inBytesSinceLast,
	Int32			inContentLength)
{
	Assert_(mUIContext != NULL);
	mUIContext->GraphProgress(inURL, inBytesReceived, inBytesSinceLast, inContentLength);
}

#pragma mark --- STUBS ---

// FIX ME? Do we really wan't to override these methods?
void CPlainTextConversionContext::Alert(const char* /* inAlertText */) {}
XP_Bool CPlainTextConversionContext::Confirm(const char* /* inMessage */) { return false; }
char* CPlainTextConversionContext::Prompt(
	const char* /* inMessage */,
	const char* /* inDefaultText */) { return NULL; }
XP_Bool CPlainTextConversionContext::PromptUsernameAndPassword(
	const char* /* inMessage */,
	char** /* outUserName */,
	char** /* outPassword */) { return false; }
char* CPlainTextConversionContext::PromptPassword(const char* /* inMessage */) { return NULL; }
