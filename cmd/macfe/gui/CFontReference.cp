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

#include "CFontReference.h"

#include <Printing.h>

#include "uprefd.h"
#include "xp_hash.h"
#include "xp_mem.h"
#include "lo_ele.h"

#include "FontTypes.h"
#include "shist.h"

#include "resgui.h"

// Set this to 1 to force WebFont lookup instead of native font lookup
#define TEST_NATIVE_DISPLAYER 0

#if TEST_NATIVE_DISPLAYER
#include "mcfp.h"
#endif

#include "libi18n.h"
#include "UPropFontSwitcher.h"
#include "UFixedFontSwitcher.h"
#include "UUTF8TextHandler.h"

#define WEIGHT_BOLD 700
#define WEIGHT_NORMAL 400	 

class NameList
{
public:
	NameList(char *list);
	~NameList();
	
	Boolean Empty();
	void Next();
	char *Current();
	
private:
	char *list, *name;
	int start, next;
};

NameList::NameList(char *list)
{
	int length = strlen(list);
	
	this->list = list;
	start = next = 0;
	
	
	if (length > 0)
	{
		name = new char[length + 1];
		this->Next();
	}
	else
	{
		name = NULL;
	}
}

NameList::~NameList()
{
	if (name != NULL)
		delete[] name;
}

char *NameList::Current()
{
	if (Empty())
		return NULL;
	
	return name;
}

Boolean NameList::Empty()
{
	return list == NULL || list[start] == '\0';
}

void NameList::Next()
{
	start = next;
	
	if (Empty())
		return;
	
	// skip over leading whitespace and commas to find the start of the name
	while (list[start] != '\0' && (list[start] == ',' || isspace(list[start])))
	{
		start += 1;
	}
	
	// find the end of the name
	next = start;
	while(list[next] != '\0' && list[next] != ',')
	{
		if (list[next] == '"' || list[next] == '\'')
		{
			char final = list[next];
			
			// find the matching quote
			next += 1;
			while (list[next] != '\0' && list[next] != final)
			{
				next += 1;
			}
			
			// if list[next] is null, there was no matching quote, so bail
			if (list[next] == '\0')
			{
				break;
			}
		}
		
		next += 1;
	}
	
	// strip off trailing whitespace
	int end = next - 1;
	
	while (end >= start && isspace(list[end]))
	{
		end -= 1;
	}
	
	// copy what's between start and end into name
	int src = start;
	int dst = 0;
	
	// if it's quoted, strip off the quotes
	if ((list[start] == '"' || list[start] == '\'') && list[end] == list[start])
	{
		src += 1;
		end -= 1;
	}
	
	// copy the characters from src to end into the name
	while (src <= end)
	{
		name[dst++] = list[src++];
	}
	
	name[dst] = '\0';
}

void FE_ReleaseTextAttrFeData(MWContext * /* context */, LO_TextAttr *attr)
{
	CFontReference *fontReference = (CFontReference *) attr->FE_Data;
	
	if (fontReference != NULL)
		delete fontReference;
}

short CFontReference::GetScaledTextSize(short size, short attrSize)
{
	short scaledSize;
	
	if ( attrSize < 1 )
		attrSize = 1;
	else if ( attrSize > 7 )
		attrSize = 7;
	
	// ¥ Houck's font table from WINFE
	switch ( attrSize )
	{
		case 0: scaledSize =      size / 2   ; break;
		case 1: scaledSize =  7 * size / 10  ; break;
		case 2: scaledSize = 85 * size / 100 ; break;
		case 3: scaledSize =      size       ; break;
		case 4: scaledSize = 12 * size / 10  ; break;
		case 5: scaledSize =  3 * size / 2   ; break;
		case 6: scaledSize =  2 * size       ; break;
		case 7: scaledSize =  3 * size       ; break;
	}
	
	// should this test be at a higher level?
	if ( scaledSize < 9 ) // looks bad at anything lower
		scaledSize = 9;
	
	return scaledSize;
}

CFontReference *CFontReference::GetFontReference(const CCharSet *charSet, LO_TextAttr* attr, MWContext *context, Boolean underlineLinks)
{
	CFontReference *result = NULL;
	
	if (attr->FE_Data != NULL)
	{
		result = (CFontReference *) attr->FE_Data;
		
		result->SynchToPort(qd.thePort);
		
		return result;
	}

	for (NameList list(attr->font_face); !list.Empty(); list.Next())
	{
		
		result = CWebFontReference::LookupWebFont(list.Current(), charSet, attr, context, underlineLinks);
		
		if (result != NULL)
			goto cache_result;

		result = CNativeFontReference::LookupNativeFont(list.Current(), charSet, attr, underlineLinks);
		
		if (result != NULL)
			goto cache_result;

		result = CNativeFontReference::LookupGenericFont(list.Current(), charSet, attr, underlineLinks);
		
		if (result != NULL)
			goto cache_result;
	}
		
		// if we get this far, we don't have any matching fonts
		// just return the default font for the character set.
	
		short fontID;
		
		if ( attr->fontmask & LO_FONT_FIXED )
			fontID = charSet->fFixedFontNum;
		else
			fontID = charSet->fPropFontNum;
			
		result =  new CNativeFontReference(fontID, charSet, attr, underlineLinks);

cache_result:		
		attr->FE_Data = (void *) result;
		return result;
}
	 
CFontReference::CFontReference(const CCharSet *charSet, const LO_TextAttr *attr)
{
	fIsGetFontInfoDirty = true;

	fMode = srcOr;
	
	if (attr->point_size == 0)
	{
		short textSize;
		
		if (attr->fontmask & LO_FONT_FIXED)
			textSize = charSet->fFixedFontSize;
		else
			textSize = charSet->fPropFontSize;
		
		fSize = GetScaledTextSize(textSize, attr->size);
	}
	else
	{
		fSize = attr->point_size;
	}
}

CFontReference::~CFontReference()
{
	// **** is there anything to do here?
}

void CNativeFontReference::Apply()
{
	if ( qd.thePort->txFont != fFont )
		::TextFont( fFont );
		
	if ( qd.thePort->txSize != fSize )
		::TextSize( fSize );
		
	if ( qd.thePort->txFace != fStyle )
		::TextFace( fStyle );
		
	if ( qd.thePort->txMode != fMode )
		::TextMode( fMode );
}

CFontReference *CNativeFontReference::LookupNativeFont(char *fontName, const CCharSet *charSet, const LO_TextAttr *attr,
		Boolean underlineLinks)
{
	short fontID;
	CStr255 pName(fontName);
	
	::GetFNum(pName, &fontID);
	
	if (fontID == 0)
	{

	/*
		Font ID 0 is the system font. Did we get this
		ID because the named font doesn't exist, or
		because we asked for the system font by name?
	*/	
		static CStr255 systemFontName;
		
		if (systemFontName[0] == 0)
			::GetFontName(0, systemFontName);
		
		if (pName != systemFontName)
			return NULL;
	}
	
	return new CNativeFontReference(fontID, charSet, attr, underlineLinks);
}

CFontReference *CNativeFontReference::LookupGenericFont(char *fontName, const CCharSet *charSet, const LO_TextAttr *attr,
		Boolean underlineLinks)
{
	struct GenericFontFamily
	{
		char *genericName;
		char *nativeName;
	};
	
	// NOTE: These need to be in the same order as they are
	// in the resource.
	static GenericFontFamily genericNames[] =
	{
		{"serif",		NULL},
		{"sans-serif",	NULL},
		{"cursive",		NULL},
		{"fantasy",		NULL},
		{"monospace",	NULL}
	};
	
	static const int genericNameCount = sizeof(genericNames) / sizeof(GenericFontFamily);
	
	for (int nameIndex = 0; nameIndex < genericNameCount; nameIndex += 1)
	{
		if (!XP_STRCASECMP(	fontName, genericNames[nameIndex].genericName))
		{
			if (genericNames[nameIndex].nativeName == NULL)
			{
				Str255 nativeName;
				
				::GetIndString(nativeName, GENERIC_FONT_NAMES_RESID, nameIndex + 1);
				
				XP_ASSERT(nativeName[0] != 0);
				
				// allocate memory for a copy of the name
				genericNames[nameIndex].nativeName = (char *) XP_ALLOC(nativeName[0] + 1);
				
				// bail if no memory for the name
				if (genericNames[nameIndex].nativeName == NULL)
					return NULL;
				
				// copy it as a C string	
				strncpy(genericNames[nameIndex].nativeName, (char *) &nativeName[1], nativeName[0]);
				genericNames[nameIndex].nativeName[nativeName[0]] = '\0';
			}
			
			return CNativeFontReference::LookupNativeFont(genericNames[nameIndex].nativeName, charSet, attr, underlineLinks);
		}
	}
	
	return NULL;
}

CNativeFontReference::CNativeFontReference(short font, const CCharSet *charSet, const LO_TextAttr *attr,
		Boolean /* underlineLinks */) :
		CFontReference(charSet, attr)
{
	fFont = font;
	fStyle = 0;
	
	if ((charSet->fCSID & MULTIBYTE) != SINGLEBYTE)
	{
		switch(charSet->fCSID)
		{
		case CS_UTF8:
			fTextHandler = UUTF8TextHandler::Instance();
			break;
	
	/*
		// **** really want handlers for all multi-byte text
		// use system for now...
		case CS_SJIS:
			fTextHandler = SJISTextHandler::Instance();
			break;
		
		default:
			fTextHandler = MBTextHandler::Instance();
	*/
		default:
			fTextHandler = NULL;
		}
		
		if (attr->fontmask & LO_FONT_FIXED)
			fFontSwitcher = UFixedFontSwitcher::Instance();
		else
			fFontSwitcher = UPropFontSwitcher::Instance();
	}
	else
	{
		fTextHandler = NULL;
		fFontSwitcher = NULL;
	}
	
	if (attr->font_weight >= WEIGHT_BOLD || (attr->fontmask & LO_FONT_BOLD) != 0)
		fStyle |= bold;
	
	if (attr->fontmask & LO_FONT_ITALIC)
		fStyle |= italic;
	
	if (attr->attrmask & LO_ATTR_UNDERLINE)
		fStyle |= underline;
}

CNativeFontReference::~CNativeFontReference()
{
	// **** Anything interesting to do here?
}

void CNativeFontReference::DrawText(int x, int y, char *text, int start, int end)
{
	::MoveTo(x, y);
	
	if (fTextHandler != NULL)
		fTextHandler->DrawText(fFontSwitcher, &text[start], end - start + 1);
	else
		::DrawText(text, start, end - start + 1);
	
}

short CNativeFontReference::TextWidth(char *text, int firstByte, int byteCount)
{
	if (fTextHandler != NULL)
		return fTextHandler->TextWidth(fFontSwitcher, &text[firstByte], byteCount);
	else
		return ::TextWidth(text, firstByte, byteCount);
}

short CNativeFontReference::MeasureText(char *text, int firstByte, int byteCount, short* charLocs)
{
	if (fTextHandler != NULL)
		return 0;
	else
	{
		text = &text[firstByte];
		::MeasureText(byteCount, text, charLocs);

		// remove null chars widths
		short widthOffset = 0;
		short nullCharWidth = 0;
		for (int i = 1; i <= byteCount; i ++)
		{
			if (text[i-1] == '\0')
			{
				if (nullCharWidth == 0)
				{
					nullCharWidth = charLocs[i] - charLocs[i-1];
					if (nullCharWidth == 0)
						break;
				}
				widthOffset += nullCharWidth;
			}
			charLocs[i] -= widthOffset;
		}

		return byteCount;
	}
}

void CNativeFontReference::GetFontInfo(FontInfo *fontInfo)
{
	if (fIsGetFontInfoDirty)
	{
		if (fTextHandler != NULL)
			fTextHandler->GetFontInfo(fFontSwitcher, &fCachedFontInfoValues);
		else
			::GetFontInfo(&fCachedFontInfoValues);
			
		fIsGetFontInfoDirty = false;
	}
	
	*fontInfo = fCachedFontInfoValues;
}

FontBrokerHandle CWebFontReference::sBroker = NULL;
FontBrokerUtilityHandle CWebFontReference::sUtility = NULL;
XP_HashTable CWebFontReference::sPortHash = NULL;

char *CWebFontReference::sCatalogPath = NULL;

#define REQUIRED_GUTS_PATH "/usr/local/netscape/RequiredGuts/"

void CWebFontReference::Init()
{
	jint jresult;
	char *displayerPath;
	Str31 essentialFiles, dynamicFonts, dynamicFontCatalog;
	FontBrokerDisplayerHandle brokerDisplayer;
#if TEST_NATIVE_DISPLAYER
	FontDisplayerHandle nativeDisplayer;
#endif
	
	sPortHash = XP_HashTableNew(10, PortHash, PortCompFunction);
	
	sBroker = NF_FontBrokerInitialize();
	if (sBroker == NULL)
	{
		// error?
		return;
	}
	
	brokerDisplayer = (FontBrokerDisplayerHandle) nffbc_getInterface(sBroker, &nffbp_ID, NULL);
	if(brokerDisplayer == NULL)
	{
		// error?
		return;
	}
	
#if TEST_NATIVE_DISPLAYER
	nativeDisplayer = (FontDisplayerHandle) cfpFactory_Create(NULL, brokerDisplayer);
    if (nativeDisplayer != NULL)
    {
		nffbp_RegisterFontDisplayer(brokerDisplayer, nativeDisplayer, NULL);
	}
#endif

	sUtility = (FontBrokerUtilityHandle) nffbc_getInterface(sBroker, &nffbu_ID, NULL);
	if (sUtility == NULL)
	{
		// error?
		return;
	}
	
	::GetIndString(essentialFiles, 14000, 1);
	::GetIndString(dynamicFonts, 14000, 3);
	::GetIndString(dynamicFontCatalog, 14000, 4);
	
	XP_ASSERT(essentialFiles[0] != 0 && dynamicFonts[0] != 0 && dynamicFontCatalog[0] != 0);
	
	// "+ 4" is for three colons plus the null
	sCatalogPath = (char *) XP_ALLOC(essentialFiles[0] + dynamicFonts[0] + dynamicFontCatalog[0] + 4);
	if (sCatalogPath != NULL)
	{
		// Build ":Essential Files:DynamicFonts:Dynamic Font Catalog"
		strcpy(sCatalogPath, PATH_SEPARATOR_STR);
		strncat(sCatalogPath, (char *) &essentialFiles[1], essentialFiles[0]);
		strcat(sCatalogPath, PATH_SEPARATOR_STR);
		strncat(sCatalogPath, (char *) &dynamicFonts[1], dynamicFonts[0]);
		strcat(sCatalogPath, PATH_SEPARATOR_STR);
		strncat(sCatalogPath, (char *) &dynamicFontCatalog[1], dynamicFontCatalog[0]);
	
		// Don't care if this fails, file will be created when we save it.
		jresult = nffbu_LoadCatalog(sUtility, sCatalogPath, NULL);
	}
	
	// "+ 2" is for one slash and the null	
	displayerPath = (char *) XP_ALLOC(sizeof(REQUIRED_GUTS_PATH) + dynamicFonts[0] + 2);
	if (displayerPath != NULL)
	{
		// Build REQUIRED_GUTS_PATH "Dynamic Fonts/"
		strcpy(displayerPath, REQUIRED_GUTS_PATH);
		strncat(displayerPath, (char *) &dynamicFonts[1], dynamicFonts[0]);
		strcat(displayerPath, DIRECTORY_SEPARATOR_STR);

		// Result is number of displayers created, we don't really care
		jresult = nffbp_ScanForFontDisplayers(brokerDisplayer, displayerPath, NULL);
		
		XP_FREE(displayerPath);
	}
}

void CWebFontReference::Finish()
{
	jint result;
	
	if (sCatalogPath != NULL)
	{
		result = nffbu_SaveCatalog(sUtility, sCatalogPath, NULL);
		XP_FREE(sCatalogPath);
		sCatalogPath = NULL;
	}
}

// Make sure the rc's port is port...
void CWebFontReference::SynchToPort(GrafPtr port)
{
	struct rc_data rcd = nfrc_GetPlatformData(fRenderingContext, NULL);
	
	if (rcd.t.directRc.port != port)
	{
		rcd.t.directRc.port = port;
		nfrc_SetPlatformData(fRenderingContext, &rcd, NULL);
	}
	
}

// It's not clear to me that this is good hash, but it's fast!
uint32 CWebFontReference::PortHash(const void *port)
{
	return (uint32) port;
}

// Should I look at the port fields if the addresses aren't equal?
int CWebFontReference::PortCompFunction(const void *port1, const void *port2)
{
	return port1 != port2;
}

RenderingContextHandle CWebFontReference::GetCachedRenderingContext(GrafPtr port)
{
	RenderingContextHandle rc = (RenderingContextHandle) XP_Gethash(sPortHash, port, NULL);
	
	if (rc == NULL)
	{
		void *rcArgs[1];
		
		rcArgs[0] = port;
		rc = nffbu_CreateRenderingContext(sUtility, NF_RC_DIRECT, 0, rcArgs, 1, NULL);
		
		if (rc != NULL)
		{
			XP_Puthash(sPortHash, port, rc);
		}
		// error?
	}
	
	return rc;
}
	
CFontReference *CWebFontReference::LookupWebFont(char *fontName, const CCharSet *charSet, const LO_TextAttr *attr, MWContext *context, Boolean underlineLinks)
{
	FontMatchInfoHandle fmi;
	RenderingContextHandle renderingContext;
	WebFontHandle webFont;
	
	char		encoding[64];			// Should get the "64" from a header file...
	char		*charset = NULL;		// Is this the right value?
	int			weight;
	int			style = nfStyleNormal;
	int			escapement = nfSpacingProportional;
	int			underline = nfUnderlineNo;
	int			strikeout = nfStrikeOutNo;
	int			resolutionX, resolutionY;

	if (attr->font_weight != 0)
		weight = attr->font_weight;
	else if (attr->fontmask & LO_FONT_BOLD)
		weight = WEIGHT_BOLD;
	else
		weight = WEIGHT_NORMAL;

	if (attr->fontmask & LO_FONT_ITALIC)
		style = nfStyleItalic;
	
	if (attr->fontmask & LO_FONT_FIXED)
		escapement = nfSpacingMonospaced;
	
	if (attr->attrmask & LO_ATTR_UNDERLINE)
		underline = nfUnderlineYes;
	
	if ((attr->attrmask & LO_ATTR_ANCHOR) != 0 && underlineLinks)
		underline = nfUnderlineYes;
	
	if (attr->attrmask & LO_ATTR_STRIKEOUT)
		strikeout = nfStrikeOutYes;
	
	if (context->type == MWContextPrint)
	{
		THPrint hp = CPrefs::GetPrintRecord();
		
		if (hp != NULL)
		{
			::PrValidate(hp);
		
			resolutionX = (**hp).prInfo.iHRes;
			resolutionY = (**hp).prInfo.iVRes;
		}
		else
		{
			// **** What is a good default in this case?
			resolutionX = 72;
			resolutionY = 72;
		}
	}
	else
	{
		resolutionX = context->XpixelsPerPoint * 72.0;
		resolutionY = context->YpixelsPerPoint * 72.0;
	}
	
	INTL_CharSetIDToName(charSet->fCSID, encoding);
	
	fmi = nffbu_CreateFontMatchInfo(sUtility, fontName, charset, encoding,
				weight, escapement, style, underline, strikeout,
				resolutionX, resolutionY,
				NULL);
	
	if (fmi == NULL)
	{
		// if we can't get an fmi, just bail
		return NULL;
	}
	
	// **** Is qd.thePort the right place to get the port from?
	History_entry *he = SHIST_GetCurrent(&context->hist);
	char *url = NULL;
	
	if (he != NULL)
		url = he->address;
			
	renderingContext = GetCachedRenderingContext(qd.thePort);
	webFont = nffbc_LookupFont(sBroker, renderingContext, fmi, url, NULL);
	
	if (webFont == NULL)
	{
		nffbu_LookupFailed(sUtility, context, renderingContext, fmi, NULL);
		nffmi_release(fmi, NULL);
		
		return NULL;
	}
	
	return new CWebFontReference(webFont, charSet, attr);
}
	
CWebFontReference::CWebFontReference(WebFontHandle webFont, const CCharSet *charSet, const LO_TextAttr *attr) :
			CFontReference(charSet, attr)
{
	fRenderingContext = GetCachedRenderingContext(qd.thePort);
	fRenderableFont = nff_GetRenderableFont(webFont, fRenderingContext, fSize, NULL);
	
	nff_release(webFont, NULL);
}

CWebFontReference::~CWebFontReference()
{
	nfrf_release(fRenderableFont, NULL);
}

void CWebFontReference::Apply()
{
	if ( qd.thePort->txMode != fMode )
		::TextMode( fMode );
}

void CWebFontReference::DrawText(int x, int y, char *text, int start, int end)
{
	nfrf_DrawText(fRenderableFont, fRenderingContext, x, y, 0, &text[start], end - start + 1, NULL);
}

short CWebFontReference::TextWidth(char *text, int firstByte, int byteCount)
{
	jint totalLength;
	jint *charLocs;
	
	// **** Do we need to calculate the character count here?
	charLocs = (jint *) XP_ALLOC(byteCount * sizeof(jint));
	
	totalLength = nfrf_MeasureText(fRenderableFont, fRenderingContext, 0, &text[firstByte], byteCount, charLocs, byteCount, NULL);

	XP_FREE(charLocs);
	return (short) totalLength;
}

void CWebFontReference::GetFontInfo(FontInfo *fontInfo)
{
	if (fIsGetFontInfoDirty)
	{
		fCachedFontInfoValues.ascent = nfrf_GetFontAscent(fRenderableFont, NULL);
		fCachedFontInfoValues.descent = nfrf_GetFontDescent(fRenderableFont, NULL);
		fCachedFontInfoValues.widMax = nfrf_GetMaxWidth(fRenderableFont, NULL);
		
		// **** Can we do better here?
		fontInfo->leading = 0;
		
		fIsGetFontInfoDirty = false;
	}
	
	*fontInfo = fCachedFontInfoValues;
}
