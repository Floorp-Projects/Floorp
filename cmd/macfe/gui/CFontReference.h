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

#pragma once
/*
	Font References
	Classes for holding a native Macintosh font id or a Web Font handle.
*/
#include "uprefd.h"
#include "xp_hash.h"
#include "lo_ele.h"

#include "FontTypes.h"

#include "UPropFontSwitcher.h"
#include "UFixedFontSwitcher.h"
#include "UUTF8TextHandler.h"

typedef struct LO_TextAttr_struct LO_TextAttr;

class CFontReference
{
public:
	CFontReference(const CCharSet *charSet, const LO_TextAttr *attr);
	virtual ~CFontReference();
	virtual void DrawText(int x, int y, char *text, int start, int end) = 0;
	virtual short TextWidth(char *text, int firstByte, int byteCount) = 0;
	virtual void GetFontInfo(FontInfo *fontInfo) = 0;
	virtual void Apply() = 0;
	
	short GetMode() {return fMode;}
	void SetMode(short mode) {fMode = mode;}
	
	static CFontReference *GetFontReference(const CCharSet *charSet, LO_TextAttr *attr, MWContext *context, Boolean underlineLinks);
	
protected:

	short fSize;
	short fMode;
	Boolean fIsGetFontInfoDirty;
	FontInfo fCachedFontInfoValues;
	
private:
	virtual void SynchToPort(GrafPtr port) = 0;
	static short GetScaledTextSize(short size, short attrSize);
};

class CNativeFontReference : public CFontReference
{
public:
	CNativeFontReference(short font, const CCharSet *charSet, const LO_TextAttr *attr, Boolean underlineLinks);
	virtual ~CNativeFontReference();
	void DrawText(int x, int y, char *text, int start, int end);
	short TextWidth(char *text, int firstByte, int byteCount);
	short MeasureText(char *text, int firstByte, int byteCount, short* charLocs);
	void GetFontInfo(FontInfo *fontInfo);
	void Apply();
	
	static CFontReference *LookupNativeFont(char *fontName, const CCharSet *charSet, const LO_TextAttr *attr, Boolean underlineLinks);
	static CFontReference *LookupGenericFont(char *fontName, const CCharSet *charSet, const LO_TextAttr *attr, Boolean underlineLinks);

private:
	short fFont;
	Style fStyle;
	UFontSwitcher *fFontSwitcher;
	UMultiFontTextHandler *fTextHandler;

	void SynchToPort(GrafPtr /*port*/) {};
};

class CWebFontReference : public CFontReference
{
public:
	CWebFontReference(WebFontHandle webFont, const CCharSet *charSet, const LO_TextAttr *attr);
	virtual ~CWebFontReference();
	void DrawText(int x, int y, char *text, int start, int end);
	short TextWidth(char *text, int firstByte, int byteCount);
	 void GetFontInfo(FontInfo *fontInfo);
	void Apply();

	static CFontReference *LookupWebFont(char *fontName, const CCharSet *charSet, const LO_TextAttr *attr, MWContext *context, Boolean underlineLinks);
	static void Init();
	static void Finish();
	static Boolean NeedToReload(MWContext *context) { return nffbu_WebfontsNeedReload(sUtility, context, NULL); };

private:
	RenderingContextHandle fRenderingContext;
	RenderableFontHandle fRenderableFont;

	static FontBrokerHandle sBroker;
	static FontBrokerUtilityHandle sUtility;
	
	static char *sCatalogPath;
	
	static XP_HashTable sPortHash;
	
	void SynchToPort(GrafPtr port);
	static uint32 PortHash(const void *port);
	static int PortCompFunction(const void *port1, const void *port2);
	static RenderingContextHandle GetCachedRenderingContext(GrafPtr port);
};

