/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsMacUnicodeFontInfo.h"
#include "nsCRT.h"
#include "prmem.h"
#include <Fonts.h>
#include "nsICharRepresentable.h"

//#define TTDEBUG
//#define TTDEBUG1
//#define TTDEBUG2
//#define TRACK_INIT_PERFORMANCE

#ifdef TRACK_INIT_PERFORMANCE
#include <DriverServices.h>
#endif

// this is an big endian version, not good for little endian machine
#undef GET_SHORT
#define GET_SHORT(p) (*((PRUint16*)p))
#undef GET_LONG
#define GET_LONG(p)  (*((PRUint32*)p))

#define ADD_GLYPH(a,b) SET_REPRESENTABLE(a,b)
#define FONT_HAS_GLYPH(a,b) IS_REPRESENTABLE(a,b)

#undef SET_SPACE
#define SET_SPACE(c) ADD_GLYPH(spaces, c)
#undef SHOULD_BE_SPACE
#define SHOULD_BE_SPACE(c) FONT_HAS_GLYPH(spaces, c)


#define HEAD FOUR_CHAR_CODE('head')
#define LOCA FOUR_CHAR_CODE('loca')
#define CMAP FOUR_CHAR_CODE('cmap')

#if TARGET_CARBON
static PRInt32
GetIndexToLocFormat(FMFont aFont)
{
  PRUint16 indexToLocFormat;
  ByteCount len = 0;
  OSStatus err = FMGetFontTable(aFont, HEAD, 50, 2, &indexToLocFormat,nsnull);
  if (err != noErr) {
    return -1;
  }
  if (!indexToLocFormat) {
    return 0;
  }
  return 1;
}

static PRUint8*
GetSpaces(FMFont aFont, PRUint32* aMaxGlyph)
{
  int isLong = GetIndexToLocFormat(aFont);
  if (isLong < 0) {
    return nsnull;
  }
  ByteCount len;
  OSStatus err = FMGetFontTable(aFont, LOCA, 0, 0,  NULL, &len);
  
  if((err != noErr) || (!len)) {
    return nsnull;
  }
  PRUint8* buf = (PRUint8*) PR_Malloc(len);
  if (!buf) {
    return nsnull;
  }
  ByteCount newLen;
  err = FMGetFontTable(aFont, LOCA, 0, len, buf, &newLen);
  if (newLen != len) {
    PR_Free(buf);
    return nsnull;
  }
  if (isLong) {
    PRUint32 longLen = ((len / 4) - 1);
    *aMaxGlyph = longLen;
    PRUint32* longBuf = (PRUint32*) buf;
    for (PRUint32 i = 0; i < longLen; i++) {
      if (longBuf[i] == longBuf[i+1]) {
        buf[i] = 1;
      }
      else {
        buf[i] = 0;
      }
    }
  }
  else {
    PRUint32 shortLen = ((len / 2) - 1);
    *aMaxGlyph = shortLen;
    PRUint16* shortBuf = (PRUint16*) buf;
    for (PRUint16 i = 0; i < shortLen; i++) {
      if (shortBuf[i] == shortBuf[i+1]) {
        buf[i] = 1;
      }
      else {
        buf[i] = 0;
      }
    }
  }

  return buf;
}

static PRBool FillFontInfoFromCMAP(FMFont aFont, PRUint32 *aFontInfo)
{
  ByteCount len;
  OSErr err = FMGetFontTable(aFont, CMAP, 0, 0, NULL, &len);
  if((err!=noErr) || (!len))
  {
    return PR_FALSE;
  }
  PRUint8* buf = (PRUint8*) PR_Malloc(len);
  if (!buf) {
    return PR_FALSE;
  }
  ByteCount newLen;
  err = FMGetFontTable(aFont, CMAP, 0, len,  buf, &newLen);
  if (newLen != len) {
    PR_Free(buf);
    return PR_FALSE;
  }
  PRUint8* p = buf + 2;
  PRUint16 n = GET_SHORT(p);
  p += 2;
  PRUint16 i;
  PRUint32 offset;
  PRUint32 platformUnicodeOffset = 0;
  // we look for platform =3 and encoding =1, 
  // if we cannot find it but there are a platform = 0 there, we 
  // remmeber that one and use it.
  
  for (i = 0; i < n; i++) {
    PRUint16 platformID = GET_SHORT(p);
    p += 2;
    PRUint16 encodingID = GET_SHORT(p);
    p += 2;
    offset = GET_LONG(p);
    p += 4;

#ifdef TTDEBUG
    printf("platformID = %d encodingID = %d\n", platformID, encodingID);
#endif

    if (platformID == 3) {
      if (encodingID == 1) { // Unicode
        // Some fonts claim to be unicode when they are actually
        // 'pseudo-unicode' fonts that require a converter...
#ifdef TTDEBUG
        printf("Get a font which have Window Unicode CMAP\n");
#endif
        break;  // break out from for(;;) loop
      } //if (encodingID == 1)
      else if (encodingID == 0) { // symbol      
        PR_Free(buf);
#ifdef TTDEBUG
        printf("Get a Symbol font\n");
#endif
        return PR_FALSE;
      } 
    } // if (platformID == 3)  
    else {
	  if (platformID == 0) { // Unicode
#ifdef TTDEBUG
        printf("Get a font which have Unicode CMAP\n");
#endif
        platformUnicodeOffset = offset;
	  }
    }
  } // for loop

  if ((i == n) && ( 0 == platformUnicodeOffset)) {  
#ifdef TTDEBUG
    printf("Get a font which do not have Window Unicode CMAP\n");
#endif
    PR_Free(buf);
    return PR_FALSE;
  }

  // usually, we come here for the entry platform = 3 encoding = 1
  // or if we don't have it but we have a platform = 0 (platformUnicodeOffset != 0)
  if(platformUnicodeOffset)
  	offset = platformUnicodeOffset;

  p = buf + offset;
  PRUint16 format = GET_SHORT(p);
  if (format != 4) {
#ifdef TTDEBUG
    printf("Unknow CMAP format\n");
#endif
    PR_Free(buf);
    return PR_FALSE;
  }
  PRUint8* end = buf + len;

#if 0
  // XXX byte swapping only required for little endian (ifdef?)
  while (p < end) {
    PRUint8 tmp = p[0];
    p[0] = p[1];
    p[1] = tmp;
    p += 2;
  }
#endif

  	
  PRUint16* s = (PRUint16*) (buf + offset);
  PRUint16 segCount = s[3] / 2;
  PRUint16* endCode = &s[7];
  PRUint16* startCode = endCode + segCount + 1;
  PRUint16* idDelta = startCode + segCount;
  PRUint16* idRangeOffset = idDelta + segCount;
  PRUint16* glyphIdArray = idRangeOffset + segCount;

  static int spacesInitialized = 0;
  static PRUint32 spaces[2048];
  if (!spacesInitialized) {
    spacesInitialized = 1;
    SET_SPACE(0x0020);
    SET_SPACE(0x00A0);
    for (PRUint16 c = 0x2000; c <= 0x200B; c++) {
      SET_SPACE(c);
    }
    SET_SPACE(0x3000);
  }
  PRUint32 maxGlyph;
  PRUint8* isSpace = GetSpaces(aFont, &maxGlyph);
  if (!isSpace) {
    PR_Free(buf);
#ifdef TTDEBUG
    printf("Cannot get LOCA table\n");
#endif
    return PR_FALSE;
  }

  for (i = 0; i < segCount; i++) {
    if (idRangeOffset[i]) {
      PRUint16 startC = startCode[i];
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startC; c <= endC; c++) {
        PRUint16* g =
          (idRangeOffset[i]/2 + (c - startC) + &idRangeOffset[i]);
        if ((PRUint8*) g < end) {
          if (*g) {
            PRUint16 glyph = idDelta[i] + *g;
            if (glyph < maxGlyph) {
              if (isSpace[glyph]) {
                if (SHOULD_BE_SPACE(c)) {
                  ADD_GLYPH(aFontInfo, c);
#ifdef TTDEBUG1
                  printf("%4x\n", c);
#endif
                }
              }
              else {
                ADD_GLYPH(aFontInfo, c);
#ifdef TTDEBUG1
                printf("%4x\n", c);
#endif
              }
            }
          }
        }
        else {
          // XXX should we trust this font at all if it does this?
        }
      }
#ifdef TTDEBUG
      printf("0x%04X-0x%04X ", startC, endC);
#endif
    }
    else {
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startCode[i]; c <= endC; c++) {
        PRUint16 glyph = idDelta[i] + c;
        if (glyph < maxGlyph) {
          if (isSpace[glyph]) {
            if (SHOULD_BE_SPACE(c)) {
              ADD_GLYPH(aFontInfo, c);
#ifdef TTDEBUG1
              printf("%4x\n", c);
#endif
            }
          }
          else {
            ADD_GLYPH(aFontInfo, c);
#ifdef TTDEBUG1
            printf("%4x\n", c);
#endif
          }
        }
      }
#ifdef TTDEBUG
      printf("0x%04X-0x%04X ", startCode[i], endC);
#endif
    }
  }
#ifdef TTDEBUG
  printf("\n");
#endif

  PR_Free(buf);
  PR_Free(isSpace);
  return PR_TRUE;
}
#endif

PRBool nsMacUnicodeFontInfo::gGlobalFontInfoInit = PR_FALSE;
PRUint32 nsMacUnicodeFontInfo::gFontsCount = 0;
PRUint32 nsMacUnicodeFontInfo::gTTFFontsCount = 0;
PRUint32 nsMacUnicodeFontInfo::gGlyphCount = 0;
PRUint32 nsMacUnicodeFontInfo::gGlobalFontInfo[2048];

void nsMacUnicodeFontInfo::InitGolbal()
{
	if(gGlobalFontInfoInit)
		return;
	nsCRT::zero(gGlobalFontInfo, sizeof(gGlobalFontInfo));
	
#if TARGET_CARBON

#ifdef TRACK_INIT_PERFORMANCE
    AbsoluteTime startTime;
    AbsoluteTime endTime;
    startTime = UpTime();
#endif	
	
    FMFontFamilyIterator            aFontIterator;
    OSStatus                        status = 0;
    FMFont                          aFont; 
    FourCharCode                    aFormat;
    FMFontFamily                    aFontFamily;
    status = FMCreateFontFamilyIterator (NULL, NULL, kFMDefaultOptions,
                            &aFontIterator);
    while (status == noErr)
    {
        status = FMGetNextFontFamily (&aFontIterator, &aFontFamily);
        OSStatus status2;
        FMFontStyle aStyle;
        status2 =  FMGetFontFromFontFamilyInstance(aFontFamily, 0, &aFont, &aStyle);
        status2 = FMGetFontFormat (aFont, &aFormat);  
#ifdef TTDEBUG 
        Str255 familyName;
        status2 = FMGetFontFamilyName (aFontFamily, familyName);
        const char *four = (const char*)&aFormat;
        familyName[familyName[0]+1] = '\0';
        printf("%d %s format = %c%c%c%c\n", gFontsCount, familyName+1, *four, *(four+1) , *(four+2), *(four+3));
#endif
        gFontsCount++;
       	PRBool ret = FillFontInfoFromCMAP(aFont,gGlobalFontInfo);

    }
    // Dispose of the contents of the font iterator.
    status = FMDisposeFontFamilyIterator (&aFontIterator);	
#ifdef TTDEBUG2
	printf("=== The following Unicode Could be render ===\n");
#endif
	
	PRUint32 i;
	for(i=0;i < 0x10000; i++)
	{
		if(IS_REPRESENTABLE(gGlobalFontInfo, i)) {
			gGlyphCount++;
#ifdef TTDEBUG2
			printf("%4X ", i);
			if(gGlyphCount % 16 == 0)
				printf("\n");
#endif
		}
	}
#ifdef TTDEBUG2
	printf("\n");
	printf("Total Glyph = %d\n", gGlyphCount);
#endif


#ifdef TRACK_INIT_PERFORMANCE
    endTime = UpTime();
    Nanoseconds diff =
     AbsoluteToNanoseconds( SubAbsoluteFromAbsolute(endTime, startTime) );
    printf("nsMacUnicodeFontInfo::InitGolbal take %d %d nanosecond\n", diff.hi, diff.lo);
    
#endif

#endif
	gGlobalFontInfoInit = PR_TRUE;
	return;	
}
PRUint32* nsMacUnicodeFontInfo::GetGlobalFontInfo()
{
	InitGolbal();
	return gGlobalFontInfo;
}