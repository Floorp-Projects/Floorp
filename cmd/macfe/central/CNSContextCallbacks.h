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

//	CNSContextCallbacks.h


#pragma once

#include "ntypes.h"
#include "structs.h"
#include "ctxtfunc.h"

class CNSContext;


class CNSContextCallbacks
{
	public:

		static CNSContextCallbacks* GetContextCallbacks(void);		// singleton class accessor
	
									CNSContextCallbacks();
		virtual						~CNSContextCallbacks();

		ContextFuncs&				GetInternalCallbacks(void);
		
	protected:

		static	MWContext* 			CreateNewDocWindow(
											MWContext* 				inContext,
											URL_Struct*				inURL);

		static	void 				LayoutNewDocument(
											MWContext*				inContext,
											URL_Struct*				inURL,
											Int32*					inWidth,
											Int32*					inHeight,
											Int32*					inMarginWidth,
											Int32*					inMarginHeight);

		static	void 				SetDocTitle(
											MWContext* 				inContext,
											char* 					inTitle);
											
		static	void 				FinishedLayout(
											MWContext* 				inContext);
											
		static	char* 				TranslateISOText(
											MWContext* 				inContext,
											int 					inCharset,
											char*					inISOText);
											
		static	int 				GetTextInfo(
											MWContext* 				inContext,
											LO_TextStruct*			inText,
											LO_TextInfo*			inTextInfo);
											
		static	int 				MeasureText(
											MWContext* 				inContext,
											LO_TextStruct*			inText,
											short*					outCharLocs);
											
		static	void 				GetEmbedSize(
											MWContext* 				inContext,
											LO_EmbedStruct*			inEmbedStruct,
											NET_ReloadMethod		inReloadMethod);
											
		static	void 				GetJavaAppSize(
											MWContext* 				inContext,
											LO_JavaAppStruct*		inJavaAppStruct,
											NET_ReloadMethod		inReloadMethod);
											
		static	void 				GetFormElementInfo(
											MWContext* 				inContext,
											LO_FormElementStruct* 	inElement);

		static	void 				GetFormElementValue(
											MWContext* 				inContext,
											LO_FormElementStruct* 	inElement,
											XP_Bool 				inHide);

		static	void 				ResetFormElement(
											MWContext* 				inContext,
											LO_FormElementStruct* 	inElement);

		static	void 				SetFormElementToggle(
											MWContext* 				inContext,
											LO_FormElementStruct* 	inElement,
											XP_Bool 				inToggle);

		static	void 				FreeEmbedElement(
											MWContext* 				inContext,
											LO_EmbedStruct*			inEmbedStruct);

		static	void 				CreateEmbedWindow(
											MWContext* 				inContext,
											NPEmbeddedApp*			inEmbeddedApp);

		static	void 				SaveEmbedWindow(
											MWContext* 				inContext,
											NPEmbeddedApp*			inEmbeddedApp);

		static	void 				RestoreEmbedWindow(
											MWContext* 				inContext,
											NPEmbeddedApp*			inEmbeddedApp);

		static	void 				DestroyEmbedWindow(
											MWContext* 				inContext,
											NPEmbeddedApp*			inEmbeddedApp);

		static	void 				FreeJavaAppElement(
											MWContext* 				inContext,
											LJAppletData*			inAppletData);

		static	void 				HideJavaAppElement(
											MWContext* 				inContext,
											LJAppletData*				inAppletData);

		static	void 				FreeEdgeElement(
											MWContext* 				inContext,
											LO_EdgeStruct*			inEdgeStruct);

		static	void 				FormTextIsSubmit(
											MWContext* 				inContext,
											LO_FormElementStruct* 	inElement);

		static	void 				DisplaySubtext(
											MWContext* 				inContext,
											int 					inLocation,
											LO_TextStruct*			inText,
											Int32 					inStartPos,
											Int32					inEndPos,
											XP_Bool 				inNeedBG);

		static	void 				DisplayText(
											MWContext* 				inContext,
											int 					inLocation,
											LO_TextStruct*			inText,
											XP_Bool 				inNeedBG);

		static	void 				DisplayEmbed(
											MWContext* 				inContext,
											int 					inLocation,
											LO_EmbedStruct*			inEmbedStruct);

		static	void 				DisplayJavaApp(
											MWContext* 				inContext,
											int 					inLocation,
											LO_JavaAppStruct*		inJavaAppStruct);

		static	void 				DisplayEdge (
											MWContext* 				inContext,
											int 					inLocation,
											LO_EdgeStruct*			inEdgeStruct);

		static	void 				DisplayTable(
											MWContext* 				inContext,
											int 					inLocation,
											LO_TableStruct*			inTableStruct);

		static	void 				DisplayCell(
											MWContext* 				inContext,
											int 					inLocation,
											LO_CellStruct*			inCellStruct);

		static	void 				InvalidateEntireTableOrCell(
											MWContext* 				inContext,
											LO_Element*				inElement);

		static	void 				DisplayAddRowOrColBorder(
											MWContext* 				inContext,
											XP_Rect*				inRect,
											XP_Bool					inErase);

		static	void 				DisplaySubDoc(
											MWContext* 				inContext,
											int 					inLocation,
											LO_SubDocStruct*		inSubdocStruct);

		static	void 				DisplayLineFeed(
											MWContext* 				inContext,
											int 					inLocation,
											LO_LinefeedStruct*		inLinefeedStruct,
											XP_Bool 				inNeedBG);

		static	void 				DisplayHR(
											MWContext*				inContext,
											int 					inLocation,
											LO_HorizRuleStruct*		inRuleStruct);

		static	void 				DisplayBullet(
											MWContext*				inContext,
											int 					inLocation,
											LO_BullettStruct*		inBullettStruct);

		static	void 				DisplayFormElement(
											MWContext*				inContext,
											int 					inLocation,
											LO_FormElementStruct* 	inFormElement);

		static	void 				DisplayBorder(
											MWContext*				inContext,
											int 					inLocation,
											int					inX,
											int					inY,
											int					inWidth,
											int					inHeight,
											int					inBW,
											LO_Color* 				inColor,
											LO_LineStyle		inStyle);

		static	void 				UpdateEnableStates(
											MWContext* 				inContext);

		static	void 				DisplayFeedback(
											MWContext*				inContext,
											int 					inLocation,
											LO_Element* 			inElement);

		static	void 				ClearView(
											MWContext* 				inContext,
											int 					inWhich);

		static	void 				SetDocDimension(
											MWContext*				inContext,
											int 					inLocation,
											Int32 					inWidth,
											Int32 					inLength);

		static	void 				SetDocPosition(
											MWContext*				inContext,
											int 					inLocation,
											Int32 					inX,
											Int32 					inY);

		static	void 				GetDocPosition(
											MWContext*				inContext,
											int 					inLocation,
											Int32*					outX,
											Int32*					outY);

		static	void 				BeginPreSection(
											MWContext*				inContext);

		static	void 				EndPreSection(
											MWContext*				inContext);

		static	void 				SetProgressBarPercent(
											MWContext*				inContext,
											Int32 					inPercent);

		static	void 				SetBackgroundColor(
											MWContext*				inContext,
											Uint8 					inRed,
											Uint8					inGreen,
											Uint8 					inBlue);

		static	void 				Progress(
											MWContext* 				inContext,
											const char*				inMessageText);

		static	void 				Alert(
											MWContext* 				inContext,
											const char*				inAlertText);

		static	void 				SetCallNetlibAllTheTime(
											MWContext* 				inContext);

		static	void 				ClearCallNetlibAllTheTime(
											MWContext* 				inContext);

		static	void 				GraphProgressInit(
											MWContext*				inContext,
											URL_Struct*				inURL,
											Int32 					inContentLength);

		static	void 				GraphProgressDestroy(
											MWContext*				inContext,
											URL_Struct*				inURL,
											Int32 					inContentLength,
											Int32 					inTotalRead);

		static	void 				GraphProgress(
											MWContext*				inContext,
											URL_Struct*				inURL,
											Int32 					inBytesReceived,
											Int32 					inBytesSinceLast,
											Int32 					inContentLength);

		static	XP_Bool 			UseFancyFTP(
											MWContext* 				inContext);

		static	XP_Bool 			UseFancyNewsgroupListing(
											MWContext* 				inContext);

		static	int 				FileSortMethod(
											MWContext* 				inContext);

		static	XP_Bool 			ShowAllNewsArticles(
											MWContext* 				inContext);

		static	XP_Bool				Confirm(
											MWContext*				inContext,
											const char* 			inMessage);

		static	char* 				PromptWithCaption(
											MWContext*				inContext,
											const char* 			inCaption,
											const char* 			inMessage,
											const char*				inDefaultText);

		static	char* 				Prompt(
											MWContext*				inContext,
											const char* 			inMessage,
											const char*				inDefaultText);

		static	XP_Bool 			PromptUsernameAndPassword(
											MWContext* 				inContext,
											const char*				inMessage,
											char**					outUserName,
											char**					outPassword);

		static	char* 				PromptPassword(
											MWContext* 				inContext,
											const char* 			inMessage);

		static	void 				EnableClicking(
											MWContext* 				inContext);

		static	void 				AllConnectionsComplete(
											MWContext* 				inContext);

		static	void 				EraseBackground(
											MWContext*				inContext,
											int						inLocation,
											Int32					inX,
											Int32					inY,
											Uint32					inWidth,
											Uint32					inHeight,
											LO_Color*				inColor);

		static	void 				SetDrawable(
											MWContext*				inContext,
											CL_Drawable*			inDrawable);

		static	void 				GetTextFrame(
											MWContext*				inContext,
											LO_TextStruct*			inTextStruct,
											Int32					inStartPos,
											Int32					inEndPos,
											XP_Rect*				outFrame);

		static	void 				GetDefaultBackgroundColor(
											MWContext*				inContext,
											LO_Color*				outColor);

		static	void 				DrawJavaApp(
											MWContext*				inContext,
											int 					inLocation,
											LO_JavaAppStruct*		inJavaAppStruct);
											
		static	void 				HandleClippingView(
											MWContext*				inContext,
											struct LJAppletData 	*appletD, 
											int 					x, 
											int 					y, 
											int 					width, 
											int 					height);

		ContextFuncs				mCallbacks;

		static	CNSContextCallbacks* sContextCallbacks;		// singleton class
};

inline _ContextFuncs& CNSContextCallbacks::GetInternalCallbacks(void)
	{	return mCallbacks;					}
inline CNSContextCallbacks* CNSContextCallbacks::GetContextCallbacks(void)
	{	return sContextCallbacks;			}
