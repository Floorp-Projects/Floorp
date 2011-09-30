/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
    
    char* Init(PRUint32 flen) 
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
    PRUint32    LineLength() { return mLength; }    

    bool        IsEOL(char c) {return c == '\n' || c == '\r';}

    char*       mBase;
  private:
    char*       mCur;
    PRUint32    mLength;
    char*       mNext;
    char*       mLimit;
};

#endif
