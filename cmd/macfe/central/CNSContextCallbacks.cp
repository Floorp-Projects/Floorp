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

//	CNSContextCallbacks.cp


#include "CNSContextCallbacks.h"
#include "CNSContext.h"

CNSContextCallbacks* CNSContextCallbacks::sContextCallbacks = NULL;		// singleton class

CNSContextCallbacks::CNSContextCallbacks()
{
	#define MAKE_FE_FUNCS_PREFIX(f) 	CNSContextCallbacks::##f
	#define MAKE_FE_FUNCS_ASSIGN 		mCallbacks.
	#include "mk_cx_fn.h"

	Assert_(sContextCallbacks == NULL);
	sContextCallbacks = this;
}

CNSContextCallbacks::~CNSContextCallbacks()
{
	sContextCallbacks = NULL;
}

MWContext* CNSContextCallbacks::CreateNewDocWindow(
	MWContext* 			inContext,
	URL_Struct*			inURL)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	
	return theNSContext->CreateNewDocWindow(inURL);
}

void CNSContextCallbacks::LayoutNewDocument(
	MWContext*			inContext,
	URL_Struct*			inURL,
	int32*				inWidth,
	int32*				inHeight,
	int32*				inMarginWidth,
	int32*				inMarginHeight)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->LayoutNewDocument(inURL, inWidth, inHeight, inMarginWidth, inMarginHeight);
}
	
void CNSContextCallbacks::SetDocTitle(
	MWContext* 				inContext,
	char* 					inTitle)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetDocTitle(inTitle);
}

void CNSContextCallbacks::FinishedLayout(MWContext* inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->FinishedLayout();
}

char* CNSContextCallbacks::TranslateISOText(											
	MWContext* 				inContext,
	int 					inCharset,
	char*					inISOText)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->TranslateISOText(inCharset, inISOText);
}

int CNSContextCallbacks::GetTextInfo(
	MWContext* 				inContext,
	LO_TextStruct*			inText,
	LO_TextInfo*			inTextInfo)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->GetTextInfo(inText, inTextInfo);
}

int CNSContextCallbacks::MeasureText(
	MWContext* 				inContext,
	LO_TextStruct*			inText,
	short*					outCharLocs)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->MeasureText(inText, outCharLocs);
}

void CNSContextCallbacks::GetEmbedSize(
	MWContext* 				inContext,
	LO_EmbedStruct*			inEmbedStruct,
	NET_ReloadMethod		inReloadMethod)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetEmbedSize(inEmbedStruct, inReloadMethod);
}

void CNSContextCallbacks::GetJavaAppSize(
	MWContext* 				inContext,
	LO_JavaAppStruct*		inJavaAppStruct,
	NET_ReloadMethod		inReloadMethod)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetJavaAppSize(inJavaAppStruct, inReloadMethod);
}

void CNSContextCallbacks::GetFormElementInfo(
	MWContext* 				inContext,
	LO_FormElementStruct* 	inElement)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetFormElementInfo(inElement);
}

void CNSContextCallbacks::GetFormElementValue(
	MWContext* 				inContext,
	LO_FormElementStruct* 	inElement,
	XP_Bool 				inHide)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetFormElementValue(inElement, inHide);
}

void CNSContextCallbacks::ResetFormElement(
	MWContext* 				inContext,
	LO_FormElementStruct* 	inElement)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->ResetFormElement(inElement);
}

void CNSContextCallbacks::SetFormElementToggle(
	MWContext* 				inContext,
	LO_FormElementStruct* 	inElement,
	XP_Bool 				inToggle)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetFormElementToggle(inElement, inToggle);
}

void CNSContextCallbacks::FreeEmbedElement(
	MWContext* 				inContext,
	LO_EmbedStruct*			inEmbedStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->FreeEmbedElement(inEmbedStruct);

}

void CNSContextCallbacks::CreateEmbedWindow(
	MWContext*				inContext,
	NPEmbeddedApp*			inEmbeddedApp)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->CreateEmbedWindow(inEmbeddedApp);
}

void CNSContextCallbacks::SaveEmbedWindow(
	MWContext*				inContext,
	NPEmbeddedApp*			inEmbeddedApp)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SaveEmbedWindow(inEmbeddedApp);
}

void CNSContextCallbacks::RestoreEmbedWindow(
	MWContext*				inContext,
	NPEmbeddedApp*			inEmbeddedApp)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->RestoreEmbedWindow(inEmbeddedApp);
}

void CNSContextCallbacks::DestroyEmbedWindow(
	MWContext*				inContext,
	NPEmbeddedApp*			inEmbeddedApp)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DestroyEmbedWindow(inEmbeddedApp);
}

void CNSContextCallbacks::FreeJavaAppElement(
	MWContext* 				inContext,
	LJAppletData*			inAppletData)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->FreeJavaAppElement(inAppletData);
}

void CNSContextCallbacks::HideJavaAppElement(
	MWContext* 				inContext,
	LJAppletData*			inAppletData)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->HideJavaAppElement(inAppletData);
}

void CNSContextCallbacks::FreeEdgeElement(
	MWContext* 				inContext,
	LO_EdgeStruct*			inEdgeStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->FreeEdgeElement(inEdgeStruct);
}

void CNSContextCallbacks::FormTextIsSubmit(
	MWContext* 				inContext,
	LO_FormElementStruct* 	inElement)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->FormTextIsSubmit(inElement);
}

void CNSContextCallbacks::DisplaySubtext(
	MWContext* 				inContext,
	int 					inLocation,
	LO_TextStruct*			inText,
	Int32 					inStartPos,
	Int32					inEndPos,
	XP_Bool 				inNeedBG)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplaySubtext(inLocation, inText, inStartPos, inEndPos, inNeedBG);
}

void CNSContextCallbacks::DisplayText(
	MWContext* 				inContext,
	int 					inLocation,
	LO_TextStruct*			inText,
	XP_Bool 				inNeedBG)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayText(inLocation, inText, inNeedBG);
}

void CNSContextCallbacks::DisplayEmbed(
	MWContext* 				inContext,
	int 					inLocation,
	LO_EmbedStruct*			inEmbedStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayEmbed(inLocation, inEmbedStruct);
}

void CNSContextCallbacks::DisplayJavaApp(
	MWContext* 				inContext,
	int 					inLocation,
	LO_JavaAppStruct*		inJavaAppStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayJavaApp(inLocation, inJavaAppStruct);
}

void CNSContextCallbacks::DisplayEdge(
	MWContext* 				inContext,
	int 					inLocation,
	LO_EdgeStruct*			inEdgeStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayEdge(inLocation, inEdgeStruct);
}

void CNSContextCallbacks::DisplayTable(
	MWContext* 				inContext,
	int 					inLocation,
	LO_TableStruct*			inTableStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayTable(inLocation, inTableStruct);
}

void CNSContextCallbacks::DisplayCell(
	MWContext* 				inContext,
	int 					inLocation,
	LO_CellStruct*			inCellStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayCell(inLocation, inCellStruct);
}

void CNSContextCallbacks::InvalidateEntireTableOrCell(
	MWContext* 				inContext,
	LO_Element*				inElement)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	if (inElement)
		theNSContext->InvalidateEntireTableOrCell(inElement);
}

void CNSContextCallbacks::DisplayAddRowOrColBorder(
	MWContext* 				inContext,
	XP_Rect*				inRect,
	XP_Bool					inDoErase)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayAddRowOrColBorder(inRect, inDoErase);
}

void CNSContextCallbacks::DisplaySubDoc(
	MWContext* 				inContext,
	int 					inLocation,
	LO_SubDocStruct*		inSubdocStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplaySubDoc(inLocation, inSubdocStruct);
}

void CNSContextCallbacks::DisplayLineFeed(
	MWContext* 				inContext,
	int 					inLocation,
	LO_LinefeedStruct*		inLinefeedStruct,
	XP_Bool 				inNeedBG)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayLineFeed(inLocation, inLinefeedStruct, inNeedBG);
}

void CNSContextCallbacks::DisplayHR(
	MWContext*				inContext,
	int 					inLocation,
	LO_HorizRuleStruct*		inRuleStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayHR(inLocation, inRuleStruct);
}

void CNSContextCallbacks::DisplayBullet(
	MWContext*				inContext,
	int 					inLocation,
	LO_BullettStruct*		inBullettStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayBullet(inLocation, inBullettStruct);
}

void CNSContextCallbacks::DisplayFormElement(
	MWContext*				inContext,
	int 					inLocation,
	LO_FormElementStruct* 	inFormElement)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayFormElement(inLocation, inFormElement);
}

void CNSContextCallbacks::DisplayBorder(
	MWContext*				inContext,
	int 					inLocation,
	int					inX,
	int					inY,
	int					inWidth,
	int					inHeight,
	int					inBW,
	LO_Color*	 			inColor,
	LO_LineStyle			inStyle)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayBorder(inLocation, inX, inY, inWidth, inHeight, inBW, inColor, inStyle);
}


void CNSContextCallbacks::UpdateEnableStates( MWContext*	inContext )
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->UpdateEnableStates();
}

void CNSContextCallbacks::DisplayFeedback(
	MWContext*				inContext,
	int 					inLocation,
	LO_Element* 			inElement)
{
	// bail out if non-editor context
	// this function is to be used only for the editor
	if ( !inContext->is_editor )
		return;
	
	// called even if the element is not selected
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DisplayFeedback(inLocation, inElement);
}

void CNSContextCallbacks::ClearView(
	MWContext* 				inContext,
	int 					inWhich)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->ClearView(inWhich);
}

void CNSContextCallbacks::SetDocDimension(
	MWContext*				inContext,
	int 					inLocation,
	Int32 					inWidth,
	Int32 					inLength)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetDocDimension(inLocation, inWidth, inLength);
}

void CNSContextCallbacks::SetDocPosition(
	MWContext*				inContext,
	int 					inLocation,
	Int32 					inX,
	Int32 					inY)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetDocPosition(inLocation, inX, inY);
}

void CNSContextCallbacks::GetDocPosition(
	MWContext*				inContext,
	int 					inLocation,
	Int32*					outX,
	Int32*					outY)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetDocPosition(inLocation, outX, outY);
}

void CNSContextCallbacks::BeginPreSection(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->BeginPreSection();
}

void CNSContextCallbacks::EndPreSection(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->EndPreSection();
}

void CNSContextCallbacks::SetProgressBarPercent(
	MWContext*				inContext,
	Int32 					inPercent)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetProgressBarPercent(inPercent);
}

void CNSContextCallbacks::SetBackgroundColor(
	MWContext*				inContext,
	Uint8 					inRed,
	Uint8					inGreen,
	Uint8 					inBlue)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetBackgroundColor(inRed, inGreen, inBlue);
}

void CNSContextCallbacks::Progress(
	MWContext* 				inContext,
	const char*				inMessageText)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->Progress(inMessageText);
}

void CNSContextCallbacks::Alert(
	MWContext* 				inContext,
	const char*				inAlertText)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->Alert(inAlertText);
}

void CNSContextCallbacks::SetCallNetlibAllTheTime(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetCallNetlibAllTheTime();
}

void CNSContextCallbacks::ClearCallNetlibAllTheTime(
	MWContext*				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->ClearCallNetlibAllTheTime();
}

void CNSContextCallbacks::GraphProgressInit(
	MWContext*				inContext,
	URL_Struct*				inURL,
	Int32 					inContentLength)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GraphProgressInit(inURL, inContentLength);
}

void CNSContextCallbacks::GraphProgressDestroy(
	MWContext*				inContext,
	URL_Struct*				inURL,
	Int32 					inContentLength,
	Int32 					inTotalRead)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GraphProgressDestroy(inURL, inContentLength, inTotalRead);
}

void CNSContextCallbacks::GraphProgress(
	MWContext*				inContext,
	URL_Struct*				inURL,
	Int32 					inBytesReceived,
	Int32 					inBytesSinceLast,
	Int32 					inContentLength)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GraphProgress(inURL, inBytesReceived, inBytesSinceLast, inContentLength);
}

XP_Bool CNSContextCallbacks::UseFancyFTP(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->UseFancyFTP();
}

XP_Bool CNSContextCallbacks::UseFancyNewsgroupListing(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->UseFancyNewsgroupListing();
}

int CNSContextCallbacks::FileSortMethod(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->FileSortMethod();
}

XP_Bool CNSContextCallbacks::ShowAllNewsArticles(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->ShowAllNewsArticles();
}

XP_Bool	CNSContextCallbacks::Confirm(
	MWContext*				inContext,
	const char* 			inMessage)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->Confirm(inMessage);
}

char* CNSContextCallbacks::Prompt(
	MWContext*				inContext,
	const char* 			inMessage,
	const char*				inDefaultText)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->Prompt(inMessage, inDefaultText);
}

char* CNSContextCallbacks::PromptWithCaption(
	MWContext*				inContext,
	const char*				inCaption,
	const char* 			inMessage,
	const char*				inDefaultText)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->PromptWithCaption(inCaption, inMessage, inDefaultText);
}

XP_Bool CNSContextCallbacks::PromptUsernameAndPassword(
	MWContext* 				inContext,
	const char*				inMessage,
	char**					outUserName,
	char**					outPassword)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->PromptUsernameAndPassword(inMessage, outUserName, outPassword);
}

char* CNSContextCallbacks::PromptPassword(
	MWContext* 				inContext,
	const char* 			inMessage)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	return theNSContext->PromptPassword(inMessage);
}

void CNSContextCallbacks::EnableClicking(
	MWContext* 				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->EnableClicking();
}

void CNSContextCallbacks::AllConnectionsComplete(
	MWContext*				inContext)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	if (theNSContext)
		theNSContext->AllConnectionsComplete();
}

void CNSContextCallbacks::EraseBackground(
	MWContext*				inContext,
	int						inLocation,
	Int32					inX,
	Int32					inY,
	Uint32					inWidth,
	Uint32					inHeight,
	LO_Color*				inColor)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->EraseBackground(inLocation, inX, inY, inWidth, inHeight, inColor);
}

void CNSContextCallbacks::SetDrawable(
	MWContext*				inContext,
	CL_Drawable*			inDrawable)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->SetDrawable(inDrawable);
}

void CNSContextCallbacks::GetTextFrame(
	MWContext*				inContext,
	LO_TextStruct*			inTextStruct,
	Int32					inStartPos,
	Int32					inEndPos,
	XP_Rect*				outFrame)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetTextFrame(inTextStruct, inStartPos, inEndPos, outFrame);
}

void CNSContextCallbacks::GetDefaultBackgroundColor(
	MWContext*				inContext,
	LO_Color*				outColor)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->GetDefaultBackgroundColor(outColor);
}

void CNSContextCallbacks::DrawJavaApp(
	MWContext*				inContext,
	int 					inLocation,
	LO_JavaAppStruct*		inJavaAppStruct)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->DrawJavaApp(inLocation, inJavaAppStruct);
}

void CNSContextCallbacks::HandleClippingView(
	MWContext*				inContext,
	struct LJAppletData 	*appletD, 
	int 					x, 
	int 					y, 
	int 					width, 
	int 					height)
{
	CNSContext* theNSContext = ExtractNSContext(inContext);
	Assert_(theNSContext != NULL);
	theNSContext->HandleClippingView(appletD, x, y, width, height);
}
