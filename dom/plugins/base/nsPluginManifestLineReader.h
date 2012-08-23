/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPluginManifestLineReader_h_
#define nsPluginManifestLineReader_h_

#include "nspr.h"
#include "nsDebug.h"

#ifdef XP_WIN
#define PLUGIN_REGISTRY_FIELD_DELIMITER '|'
#else
#define PLUGIN_REGISTRY_FIELD_DELIMITER ':'
#endif

#define PLUGIN_REGISTRY_END_OF_LINE_MARKER '$'

class nsPluginManifestLineReader
{
  public:
    nsPluginManifestLineReader() {mBase = mCur = mNext = mLimit = 0;} 
    ~nsPluginManifestLineReader() { if (mBase) delete[] mBase; mBase=0;}
    
    char* Init(uint32_t flen) 
    {
      mBase = mCur = mNext = new char[flen + 1];
      if (mBase) {
        mLimit = mBase + flen;
        *mLimit = 0;
      }
      mLength = 0;
      return mBase;
    }
    
    bool NextLine()
    {
      if (mNext >= mLimit)
        return false;
      
      mCur = mNext;
      mLength = 0;
      
      char *lastDelimiter = 0;
      while(mNext < mLimit) {
        if (IsEOL(*mNext)) {
          if (lastDelimiter) {
            if (lastDelimiter && *(mNext - 1) != PLUGIN_REGISTRY_END_OF_LINE_MARKER)
              return false;
            *lastDelimiter = '\0';
          } else {
            *mNext = '\0';
          }

          for (++mNext; mNext < mLimit; ++mNext) {
            if (!IsEOL(*mNext))
              break;
          }
          return true;
        }
        if (*mNext == PLUGIN_REGISTRY_FIELD_DELIMITER)
          lastDelimiter = mNext;
        ++mNext;
        ++mLength;
      }
      return false;        
    }

    int ParseLine(char** chunks, int maxChunks)
    {
      NS_ASSERTION(mCur && maxChunks && chunks, "bad call to ParseLine");
      int found = 0;
      chunks[found++] = mCur;
      
      if (found < maxChunks) {
        for (char* cur = mCur; *cur; cur++) {
          if (*cur == PLUGIN_REGISTRY_FIELD_DELIMITER) {
            *cur = 0;
            chunks[found++] = cur + 1;
            if (found == maxChunks)
              break;
          }
        }
      }
      return found;
    }

    char*       LinePtr() { return mCur; }
    uint32_t    LineLength() { return mLength; }    

    bool        IsEOL(char c) {return c == '\n' || c == '\r';}

    char*       mBase;
  private:
    char*       mCur;
    uint32_t    mLength;
    char*       mNext;
    char*       mLimit;
};

#endif
