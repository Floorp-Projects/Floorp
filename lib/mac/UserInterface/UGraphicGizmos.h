/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#pragma once

#include <PP_Prefix.h>
#include <LPane.h>

#include "UStdBevels.h"

#ifndef __TEXTEDIT__
#include <TextEdit.h>
#endif

#ifndef __ICONS__
#include <Icons.h>
#endif

enum {
	teFlushBottom = teFlushRight,
	teFlushTop = teFlushLeft
};

enum {
	wHiliteLight 	= 5,
	wHiliteDark,
	wTitleBarLight,
	wTitleBarDark,
	wDialogLight,
	wDialogDark,
	wTingeLight,
	wTingeDark
};

class CGWorld;

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Class UGraphicGizmos
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class UGraphicGizmos
{
	public:

		static void		LoadBevelTraits(
							ResIDT 				inBevelTraitsID,
							SBevelColorDesc& 	outBevelDesc);
							
		static void 	BevelRect(
								const Rect&		inRect,
								Int16 			inBevel,
								Int16 			inTopColor,
								Int16			inBottomColor);

		static void 	BevelPartialRect(
								const Rect &	inRect,
								Int16 			inBevel,
								Int16 			inTopColor,
								Int16			inBottomColor,
								const SBooleanRect&	inSides);

		static void 	BevelRect(
								const Rect&		inRect,
								Int16 			inBevel,
								const RGBColor&	inTopColor,
								const RGBColor&	inBottomColor);

		static void 	BevelRectAGA(
								const Rect&		inRect,
								Int16 			inBevel,
								const RGBColor&	inTopColor,
								const RGBColor&	inBottomColor);

		static void 	BevelPartialRect(
								const Rect &	inRect,
								Int16 			inBevel,
								const RGBColor&	inTopColor,
								const RGBColor&	inBottomColor,
								const SBooleanRect&	inSides);

		static void		BevelTintRect(
								const Rect&		inRect,
								Int16 			inBevel,
								Uint16			inTopTint,
								Uint16			inBottomTint);

		static void		BevelTintRoundRect(
								const Rect&		inRect,
								Int16			inOvalWidth,
								Int16			inOvalHeight,
								Uint16			inTint,
								Boolean			inLighten);

		static void		BevelTintPartialRect(
								const Rect &	inRect,
								Int16 			inBevel,
								Uint16			inTopTint,
								Uint16			inBottomTint,
								const SBooleanRect& inSides);
								
		static void		BevelTintLine(
								Int16			inStartX,
								Int16			inStartY,
								Int16			inEndX,
								Int16			inEndY,
								Uint16			inTint,
								Boolean			inLighten);

		static void		BevelTintPixel(
								Int16			inX,
								Int16			inY,
								Uint16			inTint,
								Boolean			inLighten);

		static void		RaiseColorVolume(
								const Rect&		inRect,
								Uint16			inTint = 0x8000);
								
		static void		LowerColorVolume(
								const Rect&		inRect,
								Uint16			inTint = 0x8000);
								
		static void		LowerRoundRectColorVolume(
								const Rect&		inRect,
								Int16			inOvalWidth,
								Int16			inOvalHeight,
								Uint16			inTint = 0x8000);

		static void 	PaintDisabledRect(
								const Rect&		inRect,
								Uint16			inTint = 0x8000,
								Boolean			inMakeLighter = true);

		static void 	CenterRectOnRect(
								Rect&			inRect,
								const Rect&		inOnRect);

		static void		AlignRectOnRect(
								Rect&			inRect,
								const Rect&		inOnRect,
								Uint16			inAlignment);

		static void		PadAlignedRect(
								Rect&			ioRect,
								Uint16			inPad,
								Uint16			inAlignment);

		static void		CenterStringInRect(
								const StringPtr inString,
								const Rect		&inRect);
			
		static void 	PlaceStringInRect(
								const StringPtr inString,
								const Rect 		&inRect,
								Int16			inHorizJustType = teCenter,
								Int16			inVertJustType = teCenter,
								const FontInfo*	inFontInfo = NULL);
								
		static void 	PlaceTextInRect(
								const char* 	inText,
								Uint32			inTextLength,
								const Rect 		&inRect,
								Int16			inHorizJustType = teCenter,
								Int16			inVertJustType = teCenter,
								const FontInfo*	inFontInfo = NULL,
								Boolean			inDoTruncate = false,
								TruncCode		inTruncWhere = truncMiddle);
								
		static void		DrawUTF8TextString(	const char*		inText, 
								const FontInfo*	inFontInfo,
								SInt16			inMargin,
								const Rect&		inBounds,
								SInt16			inJustification = teFlushLeft,
								Boolean			doTruncate = true,
								TruncCode		truncWhere = truncMiddle);

		static void 	PlaceUTF8TextInRect(
								const char* 	inText,
								Uint32			inTextLength,
								const Rect 		&inRect,
								Int16			inHorizJustType = teCenter,
								Int16			inVertJustType = teCenter,
								const FontInfo*	inFontInfo = NULL,
								Boolean			inDoTruncate = false,
								TruncCode		inTruncWhere = truncMiddle);	

		static int 	    GetUTF8TextWidth(
								const char* 	inText,
								int				inTextLength);	

		static Point	CalcStringPosition(
								const Rect		&inRect,
								Int16			inStringWidth,
								Int16			inHorizJustType,
								Int16			inVertJustType,
								const FontInfo	*inFontInfo);							
								
		static void 	CalcWindowTingeColors(
								WindowPtr		inWindowPtr,
								RGBColor		&outLightTinge,
								RGBColor		&outDarkTinge);
								
		static void		FindInColorTable(
								CTabHandle 		inColorTable,
								Int16 			inColorID,
								RGBColor&		outColor);
		
		static void		MixColor(
								const RGBColor& inLightColor,
								const RGBColor&	inDarkColor,
								Int16 			inShade,
								RGBColor&		outColor);
	
		static void		DrawArithPattern(
								const Rect& 		inFrame,
								const Pattern&		inPattern,
								Uint16				inTint,
								Boolean				inLighten);
								
		static void		DrawPopupArrow(
								const Rect&			inLocalFrame,
								Boolean				inIsEnabled			= true,
								Boolean				inIsActive			= true,
								Boolean				inIsHilited			= false);
		
		static RGBColor	sLighter;
		static RGBColor	sDarker;

private:

		static Boolean 	GetTingeColorsFromColorTable (
								CTabHandle		inColorTable,
								RGBColor		&outLightTinge,
								RGBColor		&outDarkTinge);

};

inline Int16 RectWidth(const Rect& inRect)
	{	return inRect.right - inRect.left;		}
inline Int16 RectHeight(const Rect& inRect)
	{	return inRect.bottom - inRect.top;		}


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Class CSicn
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

typedef Int16 SicnT[16];

class CSicn
{
	public:
						CSicn();
						CSicn(ResIDT inSicnID, Int16 inIndex = 0);

		void			LoadSicnData(ResIDT inSicnID, Int16 inIndex = 0);

		void			Plot(Int16 inMode = srcCopy);
		void			Plot(const Rect &inRect, Int16 inMode = srcCopy);
				
		operator		Rect*();
		operator		Rect&();
		
	protected:
		void			CopyImage(const Rect &inRect, Int16 inMode);
		
		SicnT			mSicnData;
		Rect			mSicnRect;
};

inline CSicn::operator Rect*()
	{	return &mSicnRect;		}
inline CSicn::operator Rect&()
	{ 	return mSicnRect;  		}

	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	Class CIconFamily
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CIconFamily
{
	public:
						CIconFamily(ResIDT inFamilyID);
						~CIconFamily();
		
		void			Plot(
							IconAlignmentType inAlign = atNone,
							IconTransformType inTransform = ttNone);
							
		void			Plot(
							const Rect& 		inRect,
							IconAlignmentType 	inAlign = atNone,
							IconTransformType 	inTransform = ttNone);

		operator		Rect*();
		operator		Rect&();

		void			CalcBestSize(const Rect& inContainer);
	
	protected:
		ResIDT			mFamilyID;
		Rect			mIconRect;

	private:
						CIconFamily();	// parameters required
};

inline CIconFamily::operator Rect*()
	{	return &mIconRect;		}
inline CIconFamily::operator Rect&()
	{ 	return mIconRect;  		}
