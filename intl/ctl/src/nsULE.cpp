/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * XPCTL : nsULE.cpp
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by SUN are Copyright (C) 2000 SUN
 * Microsystems, Inc. All Rights Reserved.
 *
 * This module 'XPCTL Interface' is based on Pango (www.pango.org) 
 * by Red Hat Software. Portions created by Redhat are Copyright (C) 
 * 1999 Red Hat Software.
 * 
 * Contributor(s):
 *   Prabhat Hegde (prabhat.hegde@sun.com)
 */

#include "nsULE.h"
#include "nsString.h"

#include "pango-types.h"
#include "pango-glyph.h"
#include "pango-modules.h"
#include "pango-utils.h"

/* 
 * To Do: (Last Updated by prabhat - 08/24/02)
 * A> Extend shapers to support ASCII + Script
 * B> Eliminate Separate Script
 * C> Look at improving the speed of cursor handling logic
 *
*/

#define CLEAN_RUN \
  aPtr = aRun.head; \
  for (int ct=0; (ct < aRun.numRuns); ct++) { \
    aTmpPtr = aPtr; \
    aPtr = aPtr->next; \
    delete(aTmpPtr); \
  }

/*
 * Start of nsULE Public Functions
 */
nsULE::nsULE() {
}

nsULE::~nsULE() {
  // No data to cleanup.
}

NS_IMPL_ISUPPORTS1(nsULE, nsILE);

/* Caller needs to ensure that GetEngine is called with valid state */
PangoEngineShape* 
nsULE::GetShaper(const PRUnichar *inBuf,
                 PRUint32        aLength,
                 const char      *lang)
{
  PangoEngineShape *aEngine = NULL;
  PangoMap         *aMap = NULL;
  guint            engine_type_id = 0, render_type_id = 0;
  PRUnichar        wc = inBuf[0];

  if ((inBuf == (PRUnichar*)NULL) || (aLength <= 0)) {
    aEngine = (PangoEngineShape*)NULL;
  }
  else {

    if (engine_type_id == 0) {
      engine_type_id = g_quark_from_static_string(PANGO_ENGINE_TYPE_SHAPE);
      render_type_id = g_quark_from_static_string(PANGO_RENDER_TYPE_X);
    }

    // Do not care about lang for now
    aMap = pango_find_map("en_US", engine_type_id, render_type_id);  
    aEngine = (PangoEngineShape*)pango_map_get_engine(aMap, (PRUint32)wc);
  }
  return aEngine;
}

PRInt32
nsULE::ScriptsByRun(const PRUnichar *aSrcBuf, 
                      PRInt32         aSrcLen, 
                      textRunList     *aRunList)
{
  int              ct = 0, start = 0;
  PRBool           sameCtlRun = PR_FALSE;
  struct textRun   *tmpChunk;
  PangoEngineShape *curEngine = NULL, *prevEngine = NULL;
  PangoMap         *aMap = NULL;
  guint            engine_type_id = 0, render_type_id = 0;

  engine_type_id = g_quark_from_static_string(PANGO_ENGINE_TYPE_SHAPE);
  render_type_id = g_quark_from_static_string(PANGO_RENDER_TYPE_X);
  aMap = pango_find_map("en_US", engine_type_id, render_type_id);

  for (ct = 0; ct < aSrcLen;) {
    tmpChunk = new textRun;

    if (aRunList->numRuns == 0)
      aRunList->head = tmpChunk;
    else
      aRunList->cur->next = tmpChunk;
    aRunList->cur = tmpChunk;
    aRunList->numRuns++;
    
    tmpChunk->start = &aSrcBuf[ct];
    start = ct;
    curEngine = (PangoEngineShape*)
      pango_map_get_engine(aMap, (PRUint32)aSrcBuf[ct]);
    sameCtlRun = (curEngine != NULL);
    prevEngine = curEngine;

    if (sameCtlRun) {
      while (sameCtlRun && ct < aSrcLen) {
        curEngine = (PangoEngineShape*)
          pango_map_get_engine(aMap, (PRUint32)aSrcBuf[ct]);
        sameCtlRun = ((curEngine != NULL) && (curEngine == prevEngine));
        if (sameCtlRun)
          ct++;
      }
      tmpChunk->isOther = PR_FALSE;
    }
    else {
      while (!sameCtlRun && ct < aSrcLen) {
        curEngine = (PangoEngineShape*)
          pango_map_get_engine(aMap, (PRUint32)aSrcBuf[ct]);
        sameCtlRun = (curEngine != NULL);       
        if (!sameCtlRun)
          ct++;
      }
      tmpChunk->isOther = PR_TRUE;
    }
    
    tmpChunk->length = ct - start;
  }
  return (PRInt32)aRunList->numRuns;
}

// Default font encoding by code-range
// At the moment pangoLite only supports 2 shapers/scripts
const char*
nsULE::GetDefaultFont(const PRUnichar aString)
{
  if ((aString >= 0x0e01) && (aString <= 0x0e5b))
    return "tis620-2";
  else if ((aString >= 0x0901) && (aString <= 0x0970))
    return "sun.unicode.india-0";
  else
    return "iso8859-1";
           
}

// Analysis needs to have valid direction
PRInt32
nsULE::GetCtlData(const PRUnichar  *aString,
                  PRUint32         aLength,
                  PangoGlyphString *aGlyphs,
                  const char       *fontCharset)
{
  PangoEngineShape *aShaper = GetShaper(aString, aLength, (const char*)NULL);
  PangoAnalysis    aAnalysis;  

  aAnalysis.shape_engine = aShaper;
  aAnalysis.aDir = PANGO_DIRECTION_LTR;
  
  // Maybe find a better way to handle font encodings
  if (fontCharset == NULL)
    aAnalysis.fontCharset = strdup(GetDefaultFont(aString[0]));
  else
    aAnalysis.fontCharset = strdup(fontCharset);

  if (aShaper != NULL) {
    aShaper->script_shape(aAnalysis.fontCharset, aString, aLength,
                          &aAnalysis, aGlyphs);
    nsMemory::Free(aAnalysis.fontCharset);
  }
  else {
    /* No Shaper - Copy Input to output */
    return 0;
  }
  return aGlyphs->num_glyphs;
}

NS_IMETHODIMP 
nsULE::GetPresentationForm(const PRUnichar *aString,
                           PRUint32        aLength,
                           const char      *fontCharset,
                           char            *aGlyphs,
                           PRSize          *aOutLength)
{
  PangoGlyphString *tmpGlyphs = pango_glyph_string_new();

  GetCtlData(aString, aLength, tmpGlyphs, fontCharset);
    
  if (tmpGlyphs->num_glyphs > 0) {
    guint i = 0, glyphCt = 0;
    for (i = 0; i < tmpGlyphs->num_glyphs; i++, glyphCt++) {
      if (tmpGlyphs->glyphs[i].glyph > 0xFF) {
        aGlyphs[glyphCt]=(unsigned char)((tmpGlyphs->glyphs[i].glyph & 0xFF00) >> 8);
        glyphCt++;
      }
      aGlyphs[glyphCt]=(unsigned char)(tmpGlyphs->glyphs[i].glyph & 0x00FF);
    }
 
    *aOutLength = (PRSize)glyphCt;
  }
  else {
    /* No Shaper - Copy Input to output */
  }
  return NS_OK;
}

// This routine returns the string index of the next cluster
// corresponding to the cluster at string index 'aIndex'
// Note : Index returned is the end-offset
// Cursor position iterates between 0 and (position - 1)
NS_IMETHODIMP
nsULE::NextCluster(const PRUnichar *aString,
                   PRUint32        aLength,
                   const PRInt32   aIndex,
                   PRInt32         *nextOffset)
{
  textRunList      aRun;
  textRun          *aPtr, *aTmpPtr;
  PRInt32          aStrCt=0;
  PRBool           isBoundary=PR_FALSE;
  PangoGlyphString *aGlyphData=pango_glyph_string_new();
 
  if (aIndex >= aLength-1) {
    *nextOffset = aLength; // End
    return NS_OK;
  }

  aRun.numRuns = 0;
  ScriptsByRun(aString, aLength, &aRun);

  aPtr = aRun.head;
  for (int i=0; (i < aRun.numRuns); i++) {
    PRInt32 runLen=0;

    runLen = aPtr->length;    
    
    if ((aStrCt+runLen) < aIndex) /* Skip Run and continue */
      aStrCt += runLen;

    else if ((aStrCt+runLen) == aIndex) {
      isBoundary = PR_TRUE;/* Script Boundary - Skip a cell in next iteration */
      aStrCt += runLen;
    }

    else {
      if (aPtr->isOther) {
        *nextOffset = aIndex+1;
        CLEAN_RUN
        return NS_OK;
      }
      else {  /* CTL Cell Movement */
        PRInt32 j, startCt, beg, end, numCur;
        
        startCt=aStrCt;
        GetCtlData(aPtr->start, runLen, aGlyphData);
        
        numCur=beg=0;
        for (j=0; j<aGlyphData->num_glyphs; j++) {
          while ((!aGlyphData->glyphs[j].attr.is_cluster_start) &&
                 j<aGlyphData->num_glyphs)
            j++;
          if (j>=aGlyphData->num_glyphs)
            end=runLen;
          else
            end=aGlyphData->log_clusters[j];
          numCur += end-beg;
          if (startCt+numCur > aIndex)
            break;
          else
            beg=end;
        }

        // Found Cluster - Start of Next == End Of Current
        *nextOffset = startCt+numCur;
        CLEAN_RUN
        return NS_OK;
      }
    }
    aPtr = aPtr->next;
  }
  /* UNUSED */
  CLEAN_RUN
}

// This routine returns the end-offset of the previous block
// corresponding to string index 'aIndex'
NS_IMETHODIMP 
nsULE::PrevCluster(const PRUnichar *aString,
                   PRUint32        aLength,
                   const PRInt32   aIndex,
                   PRInt32         *prevOffset)
{
  textRunList      aRun;
  textRun          *aPtr, *aTmpPtr;
  PRInt32          aStrCt=0, startCt=0, glyphct=0;
  PangoGlyphString *aGlyphData=pango_glyph_string_new();
 
  if (aIndex<=1) {
    *prevOffset=0; // End
    return NS_OK;
  }

  aRun.numRuns=0;
  ScriptsByRun(aString, aLength, &aRun);

  // Get the index of current cluster  
  aPtr=aRun.head;
  for (int i=0; i<aRun.numRuns; i++) {
    PRInt32 runLen=aPtr->length;
    
    if ((aStrCt+runLen) < aIndex) /* Skip Run */
      aStrCt += runLen;
    
    else if ((aStrCt+runLen) == aIndex) {
      if (aPtr->isOther) {
        *prevOffset=aIndex-1;
        CLEAN_RUN
        return NS_OK;
      }
      else { /* Move back a cluster */
        startCt=aStrCt;
        GetCtlData(aPtr->start, runLen, aGlyphData); 

        glyphct=aGlyphData->num_glyphs-1;
        while (glyphct > 0) {
          if (aGlyphData->glyphs[glyphct].attr.is_cluster_start) {
            *prevOffset=startCt+aGlyphData->log_clusters[glyphct];
            CLEAN_RUN
            return NS_OK;
          }
          --glyphct;
        }
        *prevOffset=startCt;
        CLEAN_RUN
        return NS_OK;
      }
    }
    else {
      if (aPtr->isOther) {
        *prevOffset=aIndex-1;
        CLEAN_RUN
        return NS_OK;
      }
      else {
        PRInt32 j,beg,end,numPrev,numCur;

        startCt=aStrCt;
        GetCtlData(aPtr->start, runLen, aGlyphData);
        
        numPrev=numCur=beg=0;
        for (j=1; j<aGlyphData->num_glyphs; j++) {
          while ((!aGlyphData->glyphs[j].attr.is_cluster_start) &&
                 j<aGlyphData->num_glyphs)
            j++;
          if (j>=aGlyphData->num_glyphs)
            end=runLen;
          else
            end=aGlyphData->log_clusters[j];
          numCur += end-beg;
          if (numCur+startCt >= aIndex)
            break;
          else {
            beg=end;
            numPrev=numCur;
          }
        }
        *prevOffset=startCt+numPrev;
        CLEAN_RUN
        return NS_OK;
      }
    }
    aPtr=aPtr->next;
  }
  /* UNUSED */
  CLEAN_RUN
}

// This routine returns the end-offset of the previous block
// corresponding to string index 'aIndex'
NS_IMETHODIMP 
nsULE::GetRangeOfCluster(const PRUnichar *aString,
                         PRUint32        aLength,
                         const PRInt32   aIndex,
                         PRInt32         *aStart,
                         PRInt32         *aEnd)
{
  textRunList      aRun;
  textRun          *aPtr, *aTmpPtr;
  PRInt32          aStrCt=0, startCt=0,j;
  PangoGlyphString *aGlyphData=pango_glyph_string_new();

  *aStart = *aEnd = 0;
  aRun.numRuns=0;
  ScriptsByRun(aString, aLength, &aRun);

  // Get the index of current cluster  
  aPtr=aRun.head;
  for (int i=0; i<aRun.numRuns; i++) {
    PRInt32 runLen=aPtr->length;
    
    if ((aStrCt+runLen) < aIndex) /* Skip Run */
      aStrCt += runLen;        
    else {
      if (aPtr->isOther) {
        *aStart = *aEnd = aIndex;
        CLEAN_RUN
        return NS_OK;
      }
      else {
        PRInt32 beg,end,numCur;

        startCt=aStrCt;
        GetCtlData(aPtr->start, runLen, aGlyphData);
        
        numCur=beg=0;
        for (j=1; j<aGlyphData->num_glyphs; j++) {
          while ((!aGlyphData->glyphs[j].attr.is_cluster_start) &&
                 j<aGlyphData->num_glyphs)
            j++;
          if (j>=aGlyphData->num_glyphs)
            end=runLen;
          else
            end=aGlyphData->log_clusters[j];
         
          numCur += end-beg;
          if (numCur+startCt >= aIndex)
            break;
          else
            beg=end;
        }

        *aEnd = startCt+numCur;
        if (beg == 0)
          *aStart = beg;
        else {
          if ((end-beg) == 1) /* n=n Condition */
            *aStart = *aEnd;
          else
            *aStart = startCt+beg+1; /* Maintain Mozilla Convention */
        }
        CLEAN_RUN
        return NS_OK;
      }
    }
    aPtr=aPtr->next;
  }
  CLEAN_RUN
    /* UNUSED */
}
