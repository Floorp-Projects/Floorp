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
 * Contributor(s): Frank Yung-Fong Tang <ftang@netscape.com>
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

#if TARGET_CARBON

//#define DEBUG_TRUE_TYPE
#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsDependentString.h"
#include "nsLiteralString.h"
#include <ATSTypes.h>
#include <SFNTTypes.h>
#include <SFNTLayoutTypes.h>

//#define TRACK_INIT_PERFORMANCE

#ifdef TRACK_INIT_PERFORMANCE
#include <DriverServices.h>
#endif


class nsFontCleanupObserver : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsFontCleanupObserver() { NS_INIT_ISUPPORTS(); }
  virtual ~nsFontCleanupObserver() {}
};

NS_IMPL_ISUPPORTS1(nsFontCleanupObserver, nsIObserver);

NS_IMETHODIMP nsFontCleanupObserver::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *someData)
{
  if (! nsCRT::strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID,aTopic))
  {
    nsMacUnicodeFontInfo::FreeGlobal();
  }
  return NS_OK;
}

static nsFontCleanupObserver *gFontCleanupObserver;


#ifdef IS_BIG_ENDIAN 
# undef GET_SHORT
# define GET_SHORT(p) (*((PRUint16*)p))
# undef GET_LONG
# define GET_LONG(p)  (*((PRUint32*)p))
#else
# ifdef IS_LITTLE_ENDIAN 
#  undef GET_SHORT
#  define GET_SHORT(p) (((p)[0] << 8) | (p)[1])
#  undef GET_LONG
#  define GET_LONG(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])
# endif
#endif

// The following should be defined in ATSTypes.h, but they ar not
enum {
  kFMOpenTypeFontTechnology     = FOUR_CHAR_CODE('OTTO')
};

// The following should be defined in SNFTTypes.h, but they ar not
enum {
  headFontTableTag = FOUR_CHAR_CODE('head'),
  locaFontTableTag = FOUR_CHAR_CODE('loca')
};

#define ADD_GLYPH(a,b) SET_REPRESENTABLE(a,b)
#define FONT_HAS_GLYPH(a,b) IS_REPRESENTABLE(a,b)

#undef SET_SPACE
#define SET_SPACE(c) ADD_GLYPH(spaces, c)
#undef SHOULD_BE_SPACE
#define SHOULD_BE_SPACE(c) FONT_HAS_GLYPH(spaces, c)

static PRInt8
GetIndexToLocFormat(FMFont aFont)
{
  PRUint16 indexToLocFormat;
  ByteCount len = 0;
  OSStatus err = ::FMGetFontTable(aFont, headFontTableTag, 50, 2, &indexToLocFormat, nsnull);
  if (err != noErr) 
    return -1;
    
  if (!indexToLocFormat) 
    return 0;
    
  return 1;
}

static PRUint8*
GetSpaces(FMFont aFont, PRUint32* aMaxGlyph)
{
  PRInt8 isLong = GetIndexToLocFormat(aFont);
  if (isLong < 0) 
    return nsnull;

  ByteCount len = 0;
  OSStatus err = ::FMGetFontTable(aFont, locaFontTableTag, 0, 0,  NULL, &len);
  
  if ((err != noErr) || (!len))
    return nsnull;
  
  PRUint8* buf = (PRUint8*) nsMemory::Alloc(len);
  NS_ASSERTION(buf, "cannot read 'loca' table because out of memory");
  if (!buf) 
    return nsnull;
  
  ByteCount newLen = 0;
  err = ::FMGetFontTable(aFont, locaFontTableTag, 0, len, buf, &newLen);
  NS_ASSERTION((newLen == len), "cannot read 'loca' table from the font");
  
  if (newLen != len) 
  {
    nsMemory::Free(buf);
    return nsnull;
  }

  if (isLong) 
  {
    PRUint32 longLen = ((len / 4) - 1);
    *aMaxGlyph = longLen;
    PRUint32* longBuf = (PRUint32*) buf;
    for (PRUint32 i = 0; i < longLen; i++) 
    {
      if (longBuf[i] == longBuf[i+1]) 
        buf[i] = 1;
      else 
        buf[i] = 0;
    }
  }
  else 
  {
    PRUint32 shortLen = ((len / 2) - 1);
    *aMaxGlyph = shortLen;
    PRUint16* shortBuf = (PRUint16*) buf;
    for (PRUint16 i = 0; i < shortLen; i++) 
    {
      if (shortBuf[i] == shortBuf[i+1]) 
        buf[i] = 1;
      else 
        buf[i] = 0;
    }
  }

  return buf;
}

static int spacesInitialized = 0;
static PRUint32 spaces[2048];
static void InitSpace()
{
  if (!spacesInitialized) 
  {
    spacesInitialized = 1;
    SET_SPACE(0x0020);
    SET_SPACE(0x00A0);
    for (PRUint16 c = 0x2000; c <= 0x200B; c++) 
      SET_SPACE(c);
    SET_SPACE(0x3000);
  }
}

static void HandleFormat4(PRUint16* aEntry, PRUint8* aEnd, 
                            PRUint8* aIsSpace,  PRUint32 aMaxGlyph,
                            PRUint32* aFontInfo)
{
  // notice aIsSpace could be nsnull in case of OpenType font
  PRUint8* end = aEnd;  
  PRUint16* s = aEntry;
  PRUint16 segCount = s[3] / 2;
  PRUint16* endCode = &s[7];
  PRUint16* startCode = endCode + segCount + 1;
  PRUint16* idDelta = startCode + segCount;
  PRUint16* idRangeOffset = idDelta + segCount;
  PRUint16* glyphIdArray = idRangeOffset + segCount;

  PRUint16 i;
  InitSpace();
  
  for (i = 0; i < segCount; i++) 
  {
    if (idRangeOffset[i]) 
    {
      PRUint16 startC = startCode[i];
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startC; c <= endC; c++) 
      {
        PRUint16* g = (idRangeOffset[i]/2 + (c - startC) + &idRangeOffset[i]);
        if ((PRUint8*) g < end) 
        {
          if (*g) 
          {
            PRUint16 glyph = idDelta[i] + *g;
            if (glyph < aMaxGlyph) 
            {
              if (aIsSpace && aIsSpace[glyph]) 
              {
                if (SHOULD_BE_SPACE(c)) 
                  ADD_GLYPH(aFontInfo, c);
              }
              else 
              {
                ADD_GLYPH(aFontInfo, c);
              }
            }
          }
        }
        else 
        {
          // XXX should we trust this font at all if it does this?
        }
      }
    }
    else 
    {
      PRUint16 endC = endCode[i];
      for (PRUint32 c = startCode[i]; c <= endC; c++) 
      {
        PRUint16 glyph = idDelta[i] + c;
        if (glyph < aMaxGlyph) 
        {
          if (aIsSpace && aIsSpace[glyph]) 
          {
            if (SHOULD_BE_SPACE(c)) 
              ADD_GLYPH(aFontInfo, c);
          }
          else 
          {
            ADD_GLYPH(aFontInfo, c);
          }
        }
      }
    }
  }
}
static PRBool FillFontInfoFromCMAP(FMFont aFont, PRUint32 *aFontInfo, FourCharCode aFontFormat)
{
  ByteCount len;
  OSErr err = ::FMGetFontTable(aFont, cmapFontTableTag, 0, 0, NULL, &len);
  if((err!=noErr) || (!len))
    return PR_FALSE;

  PRUint8* buf = (PRUint8*) nsMemory::Alloc(len);
  NS_ASSERTION(buf, "cannot read cmap because out of memory");
  if (!buf) 
    return PR_FALSE;

  ByteCount newLen;
  err = ::FMGetFontTable(aFont, cmapFontTableTag, 0, len,  buf, &newLen);
  NS_ASSERTION(newLen == len, "cannot read cmap from the font");
  if (newLen != len) 
  {
    nsMemory::Free(buf);
    return PR_FALSE;
  }
  
  PRUint8* p = buf + sizeof(PRUint16); // skip version, move to numberSubtables
  PRUint16 n = GET_SHORT(p); // get numberSubtables
  p += sizeof(PRUint16); // skip numberSubtables, move to the encoding subtables

  PRUint16 i;
  PRUint32 offset;
  PRUint32 platformUnicodeOffset = 0;
  // we look for platform =3 and encoding =1, 
  // if we cannot find it but there are a platform = 0 there, we 
  // remmeber that one and use it.
  
  for (i = 0; i < n; i++) 
  {
    PRUint16 platformID = GET_SHORT(p); // get platformID
    p += sizeof(PRUint16); // move to platformSpecificID
    PRUint16 encodingID = GET_SHORT(p); // get platformSpecificID
    p += sizeof(PRUint16); // move to offset
    offset = GET_LONG(p);  // get offset
    p += sizeof(PRUint32); // move to next entry
#ifdef DEBUG_TRUE_TYPE
    printf("p=%d e=%d offset=%x\n", platformID, encodingID, offset);
#endif
    if (platformID == kFontMicrosoftPlatform) 
    { 
      if (encodingID == kFontMicrosoftStandardScript) 
      { // Unicode
        // Some fonts claim to be unicode when they are actually
        // 'pseudo-unicode' fonts that require a converter...
        break;  // break out from for(;;) loop
      } //if (encodingID == kFontMicrosoftStandardScript)
#if 0
      // if we have other encoding, we can still handle it, so... don't return this early
      else if (encodingID == kFontMicrosoftSymbolScript) 
      { // symbol      
        NS_ASSERTION(false, "cannot handle symbol font");
        nsMemory::Free(buf);
        return PR_FALSE;
      } 
#endif
    } // if (platformID == kFontMicrosoftPlatform)  
    else {
      if (platformID == kFontUnicodePlatform) 
      { // Unicode
        platformUnicodeOffset = offset;
      }
    }
  } // for loop
  
  NS_ASSERTION((i != n) || ( 0 != platformUnicodeOffset), "do not know the TrueType encoding");
  if ((i == n) && ( 0 == platformUnicodeOffset)) 
  {  
    nsMemory::Free(buf);
    return PR_FALSE;
  }

  // usually, we come here for the entry platform = 3 encoding = 1
  // or if we don't have it but we have a platform = 0 (platformUnicodeOffset != 0)
  if(platformUnicodeOffset)
    offset = platformUnicodeOffset;

  p = buf + offset;
  PRUint16 format = GET_SHORT(p);
  NS_ASSERTION((kSFNTLookupSegmentArray == format), "hit some unknow format");
  switch(format) {
    case kSFNTLookupSegmentArray: // format 4
    {
      PRUint32 maxGlyph;
      PRUint8* isSpace = GetSpaces(aFont, &maxGlyph);
      // isSpace could be nsnull if the font do not have 'loca' table on Mac.
      // Two reason for that:
      // First, 'loca' table is not required in OpenType font.
      // 
      // Second, on Mac, the sfnt-housed font may not have 'loca' table. 
      // exmaple are Beijing and Taipei font.
      // see the WARNING section at the following link for details
      // http://developer.apple.com/fonts/TTRefMan/RM06/Chap6.html

      HandleFormat4((PRUint16*) (buf + offset), buf+len, isSpace, maxGlyph, aFontInfo);

      if (isSpace)
        nsMemory::Free(isSpace);
      nsMemory::Free(buf);                                
      return PR_TRUE;
    }
    break;
    default:
    {
      nsMemory::Free(buf);
      return PR_FALSE;
    }
    break;    
  }
  
}



static PRUint16* InitGlobalCCMap()
{
  PRUint32 info[2048];
  nsCRT::zero(info, sizeof(info));

#ifdef TRACK_INIT_PERFORMANCE
  AbsoluteTime startTime;
  AbsoluteTime endTime;
  startTime = UpTime();
#endif  
  
  FMFontFamilyIterator aFontIterator;
  OSStatus status = 0;
  FMFont aFont; 
  FMFontFamily aFontFamily;
  status = ::FMCreateFontFamilyIterator(NULL, NULL, kFMDefaultOptions,
                                        &aFontIterator);
  while (status == noErr)
  {
    FourCharCode aFormat;
    status = ::FMGetNextFontFamily(&aFontIterator, &aFontFamily);
    OSStatus status2;
    FMFontStyle aStyle;
    status2 = ::FMGetFontFromFontFamilyInstance(aFontFamily, 0, &aFont, &aStyle);
    NS_ASSERTION(status2 == noErr, "cannot get font from family");
    if (status2 == noErr)
    {
      status2 = ::FMGetFontFormat(aFont, &aFormat);
#ifdef DEBUG_TRUE_TYPE
      OSStatus status3 = ::FMGetFontFormat(aFont, &aFormat);
      const char *four = (const char*) &aFormat;
      Str255 familyName;
      status3 = ::FMGetFontFamilyName(aFontFamily, familyName);
      familyName[familyName[0]+1] = '\0';
      printf("%s format = %c%c%c%c\n", familyName+1, *four, *(four+1), *(four+2), *(four+3));
#endif
     if ((status2 == noErr) && 
         ((kFMTrueTypeFontTechnology == aFormat) ||
          (kFMOpenTypeFontTechnology == aFormat)))
     {
       PRBool ret = FillFontInfoFromCMAP(aFont, info, aFormat);
     }
    }
  }
  // Dispose of the contents of the font iterator.
  status = ::FMDisposeFontFamilyIterator(&aFontIterator);  
  
  PRUint16* map = MapToCCMap(info);
  NS_ASSERTION(map, "cannot create the compressed map");

  //register an observer to take care of cleanup
  gFontCleanupObserver = new nsFontCleanupObserver();
  NS_ASSERTION(gFontCleanupObserver, "failed to create observer");
  if (gFontCleanupObserver) {
    // register for shutdown
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService(do_GetService("@mozilla.org/observer-service;1", &rv));
    if (NS_SUCCEEDED(rv)) {
      rv = observerService->AddObserver(gFontCleanupObserver, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
    }    
  }
    
#ifdef TRACK_INIT_PERFORMANCE
  endTime = UpTime();
  Nanoseconds diff = ::AbsoluteToNanoseconds(SubAbsoluteFromAbsolute(endTime, startTime));
  printf("nsMacUnicodeFontInfo::InitGolbal take %d %d nanosecond\n", diff.hi, diff.lo);
#endif

  return map;
}

static PRUint16* gCCMap = nsnull;

PRBool nsMacUnicodeFontInfo::HasGlyphFor(PRUnichar aChar)
{
  if (0xfffd == aChar)
    return PR_FALSE;

  if (!gCCMap) 
    gCCMap = InitGlobalCCMap();

  NS_ASSERTION( gCCMap, "cannot init global ccmap");
  
  if (gCCMap)
    return CCMAP_HAS_CHAR(gCCMap, aChar);

  return PR_FALSE;
}

void nsMacUnicodeFontInfo::FreeGlobal()
{
  if (gCCMap)
    FreeCCMap(gCCMap);
}

#else // TARGET_CARBON

PRBool nsMacUnicodeFontInfo::HasGlyphFor(PRUnichar aChar)
{
  return PR_FALSE;
}
void nsMacUnicodeFontInfo::FreeGlobal()
{
}

#endif


