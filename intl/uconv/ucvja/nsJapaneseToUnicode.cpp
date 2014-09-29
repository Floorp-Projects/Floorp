/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsJapaneseToUnicode.h"

#include "nsUCSupport.h"

#include "japanese.map"

#include "mozilla/Assertions.h"
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

// HTML5 says to use Windows-31J instead of the real Shift_JIS for decoding
#define SJIS_INDEX gCP932Index[0]
#define JIS0208_INDEX gCP932Index[1]

#define JIS0212_INDEX gJIS0212Index
#define SJIS_UNMAPPED	0x30fb
#define UNICODE_REPLACEMENT_CHARACTER 0xfffd
#define IN_GR_RANGE(b) \
  ((uint8_t(0xa1) <= uint8_t(b)) && (uint8_t(b) <= uint8_t(0xfe)))

NS_IMETHODIMP nsShiftJISToUnicode::Convert(
   const char * aSrc, int32_t * aSrcLen,
     char16_t * aDest, int32_t * aDestLen)
{
   static const uint8_t sbIdx[256] =
   {
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x00 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x08 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x10 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x18 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x20 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x28 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x30 */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x38 */
        0,    1,    2,    3,    4,    5,    6,    7,  /* 0x40 */
        8,    9,   10,   11,   12,   13,   14,   15,  /* 0x48 */
       16,   17,   18,   19,   20,   21,   22,   23,  /* 0x50 */
       24,   25,   26,   27,   28,   29,   30,   31,  /* 0x58 */
       32,   33,   34,   35,   36,   37,   38,   39,  /* 0x60 */
       40,   41,   42,   43,   44,   45,   46,   47,  /* 0x68 */
       48,   49,   50,   51,   52,   53,   54,   55,  /* 0x70 */
       56,   57,   58,   59,   60,   61,   62, 0xFF,  /* 0x78 */
       63,   64,   65,   66,   67,   68,   69,   70,  /* 0x80 */
       71,   72,   73,   74,   75,   76,   77,   78,  /* 0x88 */
       79,   80,   81,   82,   83,   84,   85,   86,  /* 0x90 */
       87,   88,   89,   90,   91,   92,   93,   94,  /* 0x98 */
       95,   96,   97,   98,   99,  100,  101,  102,  /* 0xa0 */
      103,  104,  105,  106,  107,  108,  109,  110,  /* 0xa8 */
      111,  112,  113,  114,  115,  116,  117,  118,  /* 0xb0 */
      119,  120,  121,  122,  123,  124,  125,  126,  /* 0xb8 */
      127,  128,  129,  130,  131,  132,  133,  134,  /* 0xc0 */
      135,  136,  137,  138,  139,  140,  141,  142,  /* 0xc8 */
      143,  144,  145,  146,  147,  148,  149,  150,  /* 0xd0 */
      151,  152,  153,  154,  155,  156,  157,  158,  /* 0xd8 */
      159,  160,  161,  162,  163,  164,  165,  166,  /* 0xe0 */
      167,  168,  169,  170,  171,  172,  173,  174,  /* 0xe8 */
      175,  176,  177,  178,  179,  180,  181,  182,  /* 0xf0 */
      183,  184,  185,  186,  187, 0xFF, 0xFF, 0xFF,  /* 0xf8 */
   };

   const unsigned char* srcEnd = (unsigned char*)aSrc + *aSrcLen;
   const unsigned char* src =(unsigned char*) aSrc;
   char16_t* destEnd = aDest + *aDestLen;
   char16_t* dest = aDest;
   while (src < srcEnd) {
       switch (mState) {
          case 0:
          if (*src <= 0x80) {
            // ASCII
            *dest++ = (char16_t) *src;
            if (dest >= destEnd) {
              goto error1;
            }
          } else {
            mData = SJIS_INDEX[*src & 0x7F];
            if (mData < 0xE000) {
              mState = 1; // two bytes
            } else if (mData < 0xF000) {
              mState = 2; // EUDC
            } else {
              *dest++ = mData; // JIS 0201
              if (dest >= destEnd) {
                goto error1;
              }
            }
          }
          break;

          case 1: // Index to table
          {
            MOZ_ASSERT(mData < 0xE000);
            uint8_t off = sbIdx[*src];

            // Error handling: in the case where the second octet is not in the
            // valid ranges 0x40-0x7E 0x80-0xFC, unconsume the invalid octet and
            // interpret it as the ASCII value. In the case where the second
            // octet is in the valid range but there is no mapping for the
            // 2-octet sequence, do not unconsume.
            if(0xFF == off) {
               src--;
               if (mErrBehavior == kOnError_Signal)
                 goto error_invalidchar;
               *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
               char16_t ch = gJapaneseMap[mData+off];
               if(ch == 0xfffd) {
                 if (mErrBehavior == kOnError_Signal)
                   goto error_invalidchar;
                 ch = SJIS_UNMAPPED;
               }
               *dest++ = ch;
            }
            mState = 0;
            if(dest >= destEnd)
              goto error1;
          }
          break;

          case 2: // EUDC
          {
            MOZ_ASSERT(0xE000 <= mData && mData < 0xF000);
            uint8_t off = sbIdx[*src];

            // Error handling as in case 1
            if(0xFF == off) {
               src--;
               if (mErrBehavior == kOnError_Signal)
                 goto error_invalidchar;

               *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
               *dest++ = mData + off;
            }
            mState = 0;
            if(dest >= destEnd)
              goto error1;
          }
          break;

       }
       src++;
   }
   *aDestLen = dest - aDest;
   return NS_OK;
error_invalidchar:
   *aDestLen = dest - aDest;
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_ERROR_ILLEGAL_INPUT;
error1:
   *aDestLen = dest - aDest;
   src++;
   if ((mState == 0) && (src == srcEnd)) {
     return NS_OK;
   }
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_OK_UDEC_MOREOUTPUT;
}

char16_t
nsShiftJISToUnicode::GetCharacterForUnMapped()
{
  return char16_t(SJIS_UNMAPPED);
}

NS_IMETHODIMP nsEUCJPToUnicodeV2::Convert(
   const char * aSrc, int32_t * aSrcLen,
     char16_t * aDest, int32_t * aDestLen)
{
   static const uint8_t sbIdx[256] =
   {
/* 0x0X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x1X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x2X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x3X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x4X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x5X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x6X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x7X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x8X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x9X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xAX */
     0xFF, 0,    1,    2,    3,    4,    5,    6,  
     7,    8 ,   9,    10,   11,   12,   13,   14,
/* 0xBX */
     15,   16,   17,   18,   19,   20,   21,   22, 
     23,   24,   25,   26,   27,   28,   29,   30, 
/* 0xCX */
     31,   32,   33,   34,   35,   36,   37,   38, 
     39,   40,   41,   42,   43,   44,   45,   46, 
/* 0xDX */
     47,   48,   49,   50,   51,   52,   53,   54, 
     55,   56,   57,   58,   59,   60,   61,   62, 
/* 0xEX */
     63,   64,   65,   66,   67,   68,   69,   70, 
     71,   72,   73,   74,   75,   76,   77,   78, 
/* 0xFX */
     79,   80,   81,   82,   83,   84,   85,   86, 
     87,   88,   89,   90,   91,   92,   93,   0xFF, 
   };

   const unsigned char* srcEnd = (unsigned char*)aSrc + *aSrcLen;
   const unsigned char* src =(unsigned char*) aSrc;
   char16_t* destEnd = aDest + *aDestLen;
   char16_t* dest = aDest;
   while((src < srcEnd))
   {
       switch(mState)
       {
          case 0:
          if(*src & 0x80  && *src != (unsigned char)0xa0)
          {
            mData = JIS0208_INDEX[*src & 0x7F];
            if(mData != 0xFFFD )
            {
               mState = 1; // two byte JIS0208
            } else {
               if( 0x8e == *src) {
                 // JIS 0201
                 mState = 2; // JIS0201
               } else if(0x8f == *src) {
                 // JIS 0212
                 mState = 3; // JIS0212
               } else {
                 // others 
                 if (mErrBehavior == kOnError_Signal)
                   goto error_invalidchar;
                 *dest++ = 0xFFFD;
                 if(dest >= destEnd)
                   goto error1;
               }
            }
          } else {
            // ASCII
            *dest++ = (char16_t) *src;
            if(dest >= destEnd)
              goto error1;
          }
          break;

          case 1: // Index to table
          {
            uint8_t off = sbIdx[*src];
            if(0xFF == off) {
              if (mErrBehavior == kOnError_Signal)
                goto error_invalidchar;
              *dest++ = 0xFFFD;
               // if the first byte is valid for EUC-JP but the second 
               // is not while being a valid US-ASCII, save it
               // instead of eating it up !
              if ( (uint8_t)*src < (uint8_t)0x7f )
                --src;
            } else {
               *dest++ = gJapaneseMap[mData+off];
            }
            mState = 0;
            if(dest >= destEnd)
              goto error1;
          }
          break;

          case 2: // JIS 0201
          {
            if((0xA1 <= *src) && (*src <= 0xDF)) {
              *dest++ = (0xFF61-0x00A1) + *src;
            } else {
              if (mErrBehavior == kOnError_Signal)
                goto error_invalidchar;
              *dest++ = 0xFFFD;             
              // if 0x8e is not followed by a valid JIS X 0201 byte
              // but by a valid US-ASCII, save it instead of eating it up.
              if ( (uint8_t)*src < (uint8_t)0x7f )
                --src;
            }
            mState = 0;
            if(dest >= destEnd)
              goto error1;
          }
          break;

          case 3: // JIS 0212
          {
            if (IN_GR_RANGE(*src))
            {
              mData = JIS0212_INDEX[*src & 0x7F];
              if(mData != 0xFFFD )
              {
                 mState = 4; 
              } else {
                 mState = 5; // error
              }
            } else {
              // First "JIS 0212" byte is not in the valid GR range: save it
              if (mErrBehavior == kOnError_Signal)
                goto error_invalidchar;
              *dest++ = 0xFFFD;
              --src;
              mState = 0;
              if(dest >= destEnd)
                goto error1;
            }
          }
          break;
          case 4:
          {
            uint8_t off = sbIdx[*src];
            if(0xFF != off) {
              *dest++ = gJapaneseMap[mData+off];
              mState = 0;
              if(dest >= destEnd)
                goto error1;
              break;
            }
            // else fall through to error handler
          }
          case 5: // two bytes undefined
          {
            if (mErrBehavior == kOnError_Signal)
              goto error_invalidchar;
            *dest++ = 0xFFFD;
            // Undefined JIS 0212 two byte sequence. If the second byte is in
            // the valid range for a two byte sequence (0xa1 - 0xfe) consume
            // both bytes. Otherwise resynchronize on the second byte.
            if (!IN_GR_RANGE(*src))
              --src;
            mState = 0;
            if(dest >= destEnd)
              goto error1;
          }
          break;
       }
       src++;
   }
   *aDestLen = dest - aDest;
   return NS_OK;
error_invalidchar:
   *aDestLen = dest - aDest;
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_ERROR_ILLEGAL_INPUT;
error1:
   *aDestLen = dest - aDest;
   src++;
   if ((mState == 0) && (src == srcEnd)) {
     return NS_OK;
   } 
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_OK_UDEC_MOREOUTPUT;
}



NS_IMETHODIMP nsISO2022JPToUnicodeV2::Convert(
   const char * aSrc, int32_t * aSrcLen,
     char16_t * aDest, int32_t * aDestLen)
{
   static const uint16_t fbIdx[128] =
   {
/* 0x8X */
     0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
     0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
/* 0x9X */
     0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
     0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
/* 0xAX */
     0xFFFD, 0,      94,     94* 2,  94* 3,  94* 4,  94* 5,  94* 6,  
     94* 7,  94* 8 , 94* 9,  94*10,  94*11,  94*12,  94*13,  94*14,
/* 0xBX */
     94*15,  94*16,  94*17,  94*18,  94*19,  94*20,  94*21,  94*22,
     94*23,  94*24,  94*25,  94*26,  94*27,  94*28,  94*29,  94*30,
/* 0xCX */
     94*31,  94*32,  94*33,  94*34,  94*35,  94*36,  94*37,  94*38,
     94*39,  94*40,  94*41,  94*42,  94*43,  94*44,  94*45,  94*46,
/* 0xDX */
     94*47,  94*48,  94*49,  94*50,  94*51,  94*52,  94*53,  94*54,
     94*55,  94*56,  94*57,  94*58,  94*59,  94*60,  94*61,  94*62,
/* 0xEX */
     94*63,  94*64,  94*65,  94*66,  94*67,  94*68,  94*69,  94*70,
     94*71,  94*72,  94*73,  94*74,  94*75,  94*76,  94*77,  94*78,
/* 0xFX */
     94*79,  94*80,  94*81,  94*82,  94*83,  94*84,  94*85,  94*86,
     94*87,  94*88,  94*89,  94*90,  94*91,  94*92,  94*93,  0xFFFD,
   };
   static const uint8_t sbIdx[256] =
   {
/* 0x0X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x1X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x2X */
     0xFF, 0,    1,    2,    3,    4,    5,    6,  
     7,    8 ,   9,    10,   11,   12,   13,   14,
/* 0x3X */
     15,   16,   17,   18,   19,   20,   21,   22, 
     23,   24,   25,   26,   27,   28,   29,   30, 
/* 0x4X */
     31,   32,   33,   34,   35,   36,   37,   38, 
     39,   40,   41,   42,   43,   44,   45,   46, 
/* 0x5X */
     47,   48,   49,   50,   51,   52,   53,   54, 
     55,   56,   57,   58,   59,   60,   61,   62, 
/* 0x6X */
     63,   64,   65,   66,   67,   68,   69,   70, 
     71,   72,   73,   74,   75,   76,   77,   78, 
/* 0x7X */
     79,   80,   81,   82,   83,   84,   85,   86, 
     87,   88,   89,   90,   91,   92,   93,   0xFF, 
/* 0x8X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0x9X */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xAX */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xBX */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xCX */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xDX */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xEX */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
/* 0xFX */
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
   };

   const unsigned char* srcEnd = (unsigned char*)aSrc + *aSrcLen;
   const unsigned char* src =(unsigned char*) aSrc;
   char16_t* destEnd = aDest + *aDestLen;
   char16_t* dest = aDest;
   while((src < srcEnd))
   {
     
       switch(mState)
       {
          case mState_ASCII:
            if(0x1b == *src)
            {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              if (CHECK_OVERRUN(dest, destEnd, 1))
                goto error1;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                goto error1;
              *dest++ = (char16_t) *src;
            }
          break;
          
          case mState_ESC:
            if( '(' == *src) {
              mState = mState_ESC_28;
            } else if ('$' == *src)  {
              mState = mState_ESC_24;
            } else if ('.' == *src)  { // for ISO-2022-JP-2
              mState = mState_ESC_2e;
            } else if ('N' == *src)  { // for ISO-2022-JP-2
              mState = mState_ESC_4e;
            } else  {
              if (CHECK_OVERRUN(dest, destEnd, 2))
                goto error1;
              *dest++ = (char16_t) 0x1b;
              if (0x80 & *src) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              } else {
                *dest++ = (char16_t) *src;
              }
              mState = mLastLegalState;
            }
          break;

          case mState_ESC_28: // ESC (
            if( 'B' == *src) {
              mState = mState_ASCII;
              if (mRunLength == 0) {
                if (CHECK_OVERRUN(dest, destEnd, 1))
                  goto error1;
                *dest++ = 0xFFFD;
              }
              mRunLength = 0;
            } else if ('J' == *src)  {
              mState = mState_JISX0201_1976Roman;
              if (mRunLength == 0 && mLastLegalState != mState_ASCII) {
                if (CHECK_OVERRUN(dest, destEnd, 1))
                  goto error1;
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = 0xFFFD;
              }
              mRunLength = 0;
            } else if ('I' == *src)  {
              mState = mState_JISX0201_1976Kana;
              mRunLength = 0;
            } else  {
              if (CHECK_OVERRUN(dest, destEnd, 3))
                goto error1;
              *dest++ = (char16_t) 0x1b;
              *dest++ = (char16_t) '(';
              if (0x80 & *src) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              } else {
                *dest++ = (char16_t) *src;
              }
              mState = mLastLegalState;
            }
          break;

          case mState_ESC_24: // ESC $
            if( '@' == *src) {
              mState = mState_JISX0208_1978;
              mRunLength = 0;
            } else if ('A' == *src)  {
              mState = mState_GB2312_1980;
              mRunLength = 0;
            } else if ('B' == *src)  {
              mState = mState_JISX0208_1983;
              mRunLength = 0;
            } else if ('(' == *src)  {
              mState = mState_ESC_24_28;
            } else  {
              if (CHECK_OVERRUN(dest, destEnd, 3))
                goto error1;
              *dest++ = (char16_t) 0x1b;
              *dest++ = (char16_t) '$';
              if (0x80 & *src) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              } else {
                *dest++ = (char16_t) *src;
              }
              mState = mLastLegalState;
            }
          break;

          case mState_ESC_24_28: // ESC $ (
            if( 'C' == *src) {
              mState = mState_KSC5601_1987;
              mRunLength = 0;
            } else if ('D' == *src) {
              mState = mState_JISX0212_1990;
              mRunLength = 0;
            } else  {
              if (CHECK_OVERRUN(dest, destEnd, 4))
                goto error1;
              *dest++ = (char16_t) 0x1b;
              *dest++ = (char16_t) '$';
              *dest++ = (char16_t) '(';
              if (0x80 & *src) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              } else {
                *dest++ = (char16_t) *src;
              }
              mState = mLastLegalState;
            }
          break;

          case mState_JISX0201_1976Roman:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              if (CHECK_OVERRUN(dest, destEnd, 1))
                goto error1;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              ++mRunLength;
            } else {
              // XXX We need to  decide how to handle \ and ~ here
              // we may need a if statement here for '\' and '~' 
              // to map them to Yen and Overbar
              if (CHECK_OVERRUN(dest, destEnd, 1))
                goto error1;
              *dest++ = (char16_t) *src;
              ++mRunLength;
            }
          break;

          case mState_JISX0201_1976Kana:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                goto error1;
              if((0x21 <= *src) && (*src <= 0x5F)) {
                *dest++ = (0xFF61-0x0021) + *src;
              } else {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              }
              ++mRunLength;
            }
          break;

          case mState_JISX0208_1978:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              mLastLegalState = mState;
              mState = mState_ERROR;
            } else {
              mData = JIS0208_INDEX[*src & 0x7F];
              if (0xFFFD == mData) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                mState = mState_ERROR;
              } else {
                mState = mState_JISX0208_1978_2ndbyte;
              }
            }
          break;

          case mState_GB2312_1980:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              mLastLegalState = mState;
              mState = mState_ERROR;
            } else {
              mData = fbIdx[*src & 0x7F];
              if (0xFFFD == mData) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                mState = mState_ERROR;
              } else {
                mState = mState_GB2312_1980_2ndbyte;
              }
            }
          break;

          case mState_JISX0208_1983:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              mLastLegalState = mState;
              mState = mState_ERROR;
            } else {
              mData = JIS0208_INDEX[*src & 0x7F];
              if (0xFFFD == mData) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                mState = mState_ERROR;
              } else {
                mState = mState_JISX0208_1983_2ndbyte;
              }
            }
          break;

          case mState_KSC5601_1987:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              mLastLegalState = mState;
              mState = mState_ERROR;
            } else {
              mData = fbIdx[*src & 0x7F];
              if (0xFFFD == mData) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                mState = mState_ERROR;
              } else {
                mState = mState_KSC5601_1987_2ndbyte;
              }
            }
          break;

          case mState_JISX0212_1990:
            if(0x1b == *src) {
              mLastLegalState = mState;
              mState = mState_ESC;
            } else if(*src & 0x80) {
              mLastLegalState = mState;
              mState = mState_ERROR;
            } else {
              mData = JIS0212_INDEX[*src & 0x7F];
              if (0xFFFD == mData) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                mState = mState_ERROR;
              } else {
                mState = mState_JISX0212_1990_2ndbyte;
              }
            }
          break;

          case mState_JISX0208_1978_2ndbyte:
          {
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            uint8_t off = sbIdx[*src];
            if(0xFF == off) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
               // XXX We need to map from JIS X 0208 1983 to 1987 
               // in the next line before pass to *dest++
              *dest++ = gJapaneseMap[mData+off];
            }
            ++mRunLength;
            mState = mState_JISX0208_1978;
          }
          break;

          case mState_GB2312_1980_2ndbyte:
          {
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            uint8_t off = sbIdx[*src];
            if(0xFF == off) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
              if (!mGB2312Decoder) {
                // creating a delegate converter (GB2312)
                mGB2312Decoder =
                  EncodingUtils::DecoderForEncoding("gb18030");
              }
              if (!mGB2312Decoder) {// failed creating a delegate converter
                goto error2;
              } else {
                unsigned char gb[2];
                char16_t uni;
                int32_t gbLen = 2, uniLen = 1;
                // ((mData/94)+0x21) is the original 1st byte.
                // *src is the present 2nd byte.
                // Put 2 bytes (one character) to gb[] with GB2312 encoding.
                gb[0] = ((mData / 94) + 0x21) | 0x80;
                gb[1] = *src | 0x80;
                // Convert GB2312 to unicode.
                mGB2312Decoder->Convert((const char *)gb, &gbLen,
                                        &uni, &uniLen);
                *dest++ = uni;
              }
            }
            ++mRunLength;
            mState = mState_GB2312_1980;
          }
          break;

          case mState_JISX0208_1983_2ndbyte:
          {
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            uint8_t off = sbIdx[*src];
            if(0xFF == off) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
              *dest++ = gJapaneseMap[mData+off];
            }
            ++mRunLength;
            mState = mState_JISX0208_1983;
          }
          break;

          case mState_KSC5601_1987_2ndbyte:
          {
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            uint8_t off = sbIdx[*src];
            if(0xFF == off) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
              if (!mEUCKRDecoder) {
                // creating a delegate converter (EUC-KR)
                mEUCKRDecoder =
                  EncodingUtils::DecoderForEncoding(NS_LITERAL_CSTRING("EUC-KR"));
              }
              if (!mEUCKRDecoder) {// failed creating a delegate converter
                goto error2;
              } else {              
                unsigned char ksc[2];
                char16_t uni;
                int32_t kscLen = 2, uniLen = 1;
                // ((mData/94)+0x21) is the original 1st byte.
                // *src is the present 2nd byte.
                // Put 2 bytes (one character) to ksc[] with EUC-KR encoding.
                ksc[0] = ((mData / 94) + 0x21) | 0x80;
                ksc[1] = *src | 0x80;
                // Convert EUC-KR to unicode.
                mEUCKRDecoder->Convert((const char *)ksc, &kscLen,
                                       &uni, &uniLen);
                *dest++ = uni;
              }
            }
            ++mRunLength;
            mState = mState_KSC5601_1987;
          }
          break;

          case mState_JISX0212_1990_2ndbyte:
          {
            uint8_t off = sbIdx[*src];
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            if(0xFF == off) {
              if (mErrBehavior == kOnError_Signal)
                goto error3;
              *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            } else {
              *dest++ = gJapaneseMap[mData+off];
            }
            ++mRunLength;
            mState = mState_JISX0212_1990;
          }
          break;

          case mState_ESC_2e: // ESC .
            // "ESC ." will designate 96 character set to G2.
            mState = mLastLegalState;
            if( 'A' == *src) {
              G2charset = G2_ISO88591;
            } else if ('F' == *src) {
              G2charset = G2_ISO88597;
            } else  {
              if (CHECK_OVERRUN(dest, destEnd, 3))
                goto error1;
              *dest++ = (char16_t) 0x1b;
              *dest++ = (char16_t) '.';
              if (0x80 & *src) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              } else {
                *dest++ = (char16_t) *src;
              }
            }
          break;

          case mState_ESC_4e: // ESC N
            // "ESC N" is the SS2 sequence, that invoke a G2 designated
            // character set.  Since SS2 is effective only for next one
            // character, mState should be returned to the last status.
            mState = mLastLegalState;
            if((0x20 <= *src) && (*src <= 0x7F)) {
              if (CHECK_OVERRUN(dest, destEnd, 1))
                goto error1;
              if (G2_ISO88591 == G2charset) {
                *dest++ = *src | 0x80;
              } else if (G2_ISO88597 == G2charset) {
                if (!mISO88597Decoder) {
                  // creating a delegate converter (ISO-8859-7)
                  mISO88597Decoder =
                    EncodingUtils::DecoderForEncoding(NS_LITERAL_CSTRING("ISO-8859-7"));
                }
                if (!mISO88597Decoder) {// failed creating a delegate converter
                  goto error2;
                } else {
                  // Put one character with ISO-8859-7 encoding.
                  unsigned char gr = *src | 0x80;
                  char16_t uni;
                  int32_t grLen = 1, uniLen = 1;
                  // Convert ISO-8859-7 to unicode.
                  mISO88597Decoder->Convert((const char *)&gr, &grLen,
                                            &uni, &uniLen);
                  *dest++ = uni;
                }
              } else {// G2charset is G2_unknown (not designated yet)
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              }
              ++mRunLength;
            } else {
              if (CHECK_OVERRUN(dest, destEnd, 3))
                goto error1;
              *dest++ = (char16_t) 0x1b;
              *dest++ = (char16_t) 'N';
              if (0x80 & *src) {
                if (mErrBehavior == kOnError_Signal)
                  goto error3;
                *dest++ = UNICODE_REPLACEMENT_CHARACTER;
              } else {
                *dest++ = (char16_t) *src;
              }
            }
          break;

          case mState_ERROR:
            mState = mLastLegalState;
            if (mErrBehavior == kOnError_Signal) {
              mRunLength = 0;
              goto error3;
            }
            if (CHECK_OVERRUN(dest, destEnd, 1))
              goto error1;
            *dest++ = UNICODE_REPLACEMENT_CHARACTER;
            ++mRunLength;
          break;

       } // switch
       src++;
   }
   *aDestLen = dest - aDest;
   return NS_OK;
error1:
   *aDestLen = dest - aDest;
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_OK_UDEC_MOREOUTPUT;
error2:
   *aDestLen = dest - aDest;
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_ERROR_UNEXPECTED;
error3:
   *aDestLen = dest - aDest;
   *aSrcLen = src - (const unsigned char*)aSrc;
   return NS_ERROR_ILLEGAL_INPUT;
}
