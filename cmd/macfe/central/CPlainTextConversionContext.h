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

// CPlainTextConversionContext.h

//	This is a subclass of CNSContext to handle plain text translation.
//	This replaces code in ns/lib/xlate/text.c where a new text MWContext is
//	created.

#pragma once

#include "CNSContext.h"

class CPlainTextConversionContext : public CNSContext
{
public:
					CPlainTextConversionContext(MWContext* inUIContext);
	virtual			~CPlainTextConversionContext() { }

protected:
	// Overrides of base CNSContext methods

	virtual	void 				LayoutNewDocument(
										URL_Struct*				inURL,
										Int32*					inWidth,
										Int32*					inHeight,
										Int32*					inMarginWidth,
										Int32*					inMarginHeight);

	virtual	void 				DisplaySubtext(
										int 					inLocation,
										LO_TextStruct*			inText,
										Int32 					inStartPos,
										Int32					inEndPos,
										XP_Bool 				inNeedBG);

	virtual	void 				DisplayText(
										int 					inLocation,
										LO_TextStruct*			inText,
										XP_Bool 				inNeedBG);

	virtual	void 				DisplaySubDoc(
										int 					inLocation,
										LO_SubDocStruct*		inSubdocStruct);

	virtual	void 				DisplayTable(
										int 					inLocation,
										LO_TableStruct*			inTableStruct);

	virtual	void 				DisplayLineFeed(
										int 					inLocation,
										LO_LinefeedStruct*		inLinefeedStruct,
										XP_Bool 				inNeedBG);

	virtual	void 				DisplayHR(
										int 					inLocation,
										LO_HorizRuleStruct*		inRuleStruct);

	virtual	char* 				TranslateISOText(
										int 					inCharset,
										char*					inISOText);

	virtual	int 				GetTextInfo(
										LO_TextStruct*			inText,
										LO_TextInfo*			inTextInfo);

	virtual	int 				MeasureText(
										LO_TextStruct*			inText,
										short*					outCharLocs);
										
	virtual	void 				DisplayBullet(
										int 					inLocation,
										LO_BulletStruct*		inBulletStruct);

	virtual	void 				FinishedLayout(void);

	virtual	void 				AllConnectionsComplete(void);

	virtual	void 				Progress(
										const char*				inMessageText);

	virtual	void 				GraphProgressInit(
										URL_Struct*				inURL,
										Int32 					inContentLength);

	virtual	void 				GraphProgressDestroy(
										URL_Struct*				inURL,
										Int32 					inContentLength,
										Int32 					inTotalRead);

	virtual	void 				GraphProgress(
										URL_Struct*				inURL,
										Int32 					inBytesReceived,
										Int32 					inBytesSinceLast,
										Int32 					inContentLength);

	// Methods to stub out.

		virtual	void 				Alert(
											const char*				inAlertText);

		virtual	XP_Bool				Confirm(
											const char* 			inMessage);

		virtual	char* 				Prompt(
											const char* 			inMessage,
											const char*				inDefaultText);

		virtual	XP_Bool 			PromptUsernameAndPassword(
											const char*				inMessage,
											char**					outUserName,
											char**					outPassword);

		virtual	char* 				PromptPassword(
											const char* 			inMessage);

		CNSContext*					mUIContext;
};
