/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCharSetProber.h"

//This filter applies to all scripts which do not use English characters
bool nsCharSetProber::FilterWithoutEnglishLetters(const char* aBuf, uint32_t aLen, char** newBuf, uint32_t& newLen)
{
  char *newptr;
  char *prevPtr, *curPtr;

  bool meetMSB = false;
  newptr = *newBuf = (char*)malloc(aLen);
  if (!newptr)
    return false;

  for (curPtr = prevPtr = (char*)aBuf; curPtr < aBuf+aLen; curPtr++)
  {
    if (*curPtr & 0x80)
    {
      meetMSB = true;
    }
    else if (*curPtr < 'A' || (*curPtr > 'Z' && *curPtr < 'a') || *curPtr > 'z')
    {
      //current char is a symbol, most likely a punctuation. we treat it as segment delimiter
      if (meetMSB && curPtr > prevPtr)
      //this segment contains more than single symbol, and it has upper ASCII, we need to keep it
      {
        while (prevPtr < curPtr) *newptr++ = *prevPtr++;
        prevPtr++;
        *newptr++ = ' ';
        meetMSB = false;
      }
      else //ignore current segment. (either because it is just a symbol or just an English word)
        prevPtr = curPtr+1;
    }
  }
  if (meetMSB && curPtr > prevPtr)
    while (prevPtr < curPtr) *newptr++ = *prevPtr++;

  newLen = newptr - *newBuf;

  return true;
}

//This filter applies to all scripts which contain both English characters and upper ASCII characters.
bool nsCharSetProber::FilterWithEnglishLetters(const char* aBuf, uint32_t aLen, char** newBuf, uint32_t& newLen)
{
  //do filtering to reduce load to probers
  char *newptr;
  char *prevPtr, *curPtr;
  bool isInTag = false;

  newptr = *newBuf = (char*)malloc(aLen);
  if (!newptr)
    return false;

  for (curPtr = prevPtr = (char*)aBuf; curPtr < aBuf+aLen; curPtr++)
  {
    if (*curPtr == '>')
      isInTag = false;
    else if (*curPtr == '<')
      isInTag = true;

    if (!(*curPtr & 0x80) &&
        (*curPtr < 'A' || (*curPtr > 'Z' && *curPtr < 'a') || *curPtr > 'z') )
    {
      if (curPtr > prevPtr && !isInTag) // Current segment contains more than just a symbol
                                        // and it is not inside a tag, keep it.
      {
        while (prevPtr < curPtr) *newptr++ = *prevPtr++;
        prevPtr++;
        *newptr++ = ' ';
      }
      else
        prevPtr = curPtr+1;
    }
  }

  // If the current segment contains more than just a symbol
  // and it is not inside a tag then keep it.
  if (!isInTag)
    while (prevPtr < curPtr)
      *newptr++ = *prevPtr++;

  newLen = newptr - *newBuf;

  return true;
}
