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
 */

#include <stdio.h>
#include <ctype.h> /* isspace */

#include "nsULE.h"
#include "nsString.h"

#include "pango-types.h"
#include "pango-glyph.h"
#include "pango-modules.h"
#include "pango-utils.h"

/*
 * Start of nsULE Public Functions
 */
nsULE::nsULE() {
  NS_INIT_ISUPPORTS(); // For Reference Counting
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
   guint           engine_type_id = 0, render_type_id = 0;
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

// Analysis needs to have valid direction and font charset
NS_IMETHODIMP 
nsULE::GetPresentationForm(const PRUnichar  *aString,
                           PRUint32         aLength,
                           PangoAnalysis    *aAnalysis,
                           PangoGlyphString *aGlyphs)
{
  PangoEngineShape *aShaper = aAnalysis->shape_engine;
  PRSize           inLen = 0;
  char             *utf8Str = NULL;

  if (aShaper != NULL) {
    
    nsAutoString strBuf(aString);

    // Convert Unicode string to UTF8 and store in buffer
    utf8Str = strBuf.ToNewUTF8String();
    inLen = strlen(utf8Str);

    aShaper->script_shape(aAnalysis->fontCharset, utf8Str, inLen, 
                          aAnalysis, aGlyphs);
    nsMemory::Free(utf8Str);
  }
  else {
    /* No Shaper - Copy Input to output */
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsULE::GetPresentationForm(const PRUnichar *aString,
                           PRUint32        aLength,                           
                           const char      *fontCharset,
                           char            *aGlyphs,
                           PRSize          *aOutLength)
{
  PangoEngineShape *aShaper = GetShaper(aString, aLength, (const char*)NULL);
  PangoAnalysis    aAnalysis;
  PangoGlyphString *tmpGlyphs = pango_glyph_string_new();
  PRSize           inLen = 0;
  int              aSize = 0;
  char             *utf8Str = NULL;

  aAnalysis.shape_engine = aShaper;
  aAnalysis.aDir = PANGO_DIRECTION_LTR;
  aAnalysis.fontCharset = (char*)fontCharset;

  if (aShaper != NULL) {
    
    nsAutoString strBuf;
    strBuf.Assign(aString, aLength);

    // Convert Unicode string to UTF8 and store in buffer
    utf8Str = NULL;
    utf8Str = strBuf.ToNewUTF8String();
    inLen = strlen(utf8Str);

    aShaper->script_shape(fontCharset, utf8Str, inLen, 
                          &aAnalysis, tmpGlyphs);
    nsMemory::Free(utf8Str);
    if (tmpGlyphs->num_glyphs > 0) {
      // Note : Does NOT handle 2 byte fonts
      aSize = tmpGlyphs->num_glyphs;
      // if (*aOutLength < aSize)
      //    trouble
      for (int i = 0; i < aSize; i++)       
        aGlyphs[i] = tmpGlyphs->glyphs[i].glyph & 0xFF;
    }
    *aOutLength = (PRSize)aSize;
  }
  else {
    /* No Shaper - Copy Input to output */
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsULE::GetPresentationForm(const PRUnichar *aString,
                           PRUint32        aLength,                           
                           const char      *fontCharset,
                           PRUint32        *aGlyphs,
                           PRSize          *aOutLength)
{
  PangoEngineShape *aShaper = GetShaper(aString, aLength, (const char*)NULL);
  PangoAnalysis    aAnalysis;
  PangoGlyphString *tmpGlyphs = pango_glyph_string_new();
  PRSize           inLen = 0;
  int              aSize = 0;
  char             *utf8Str = NULL;

  aAnalysis.shape_engine = aShaper;
  aAnalysis.aDir = PANGO_DIRECTION_LTR;
  aAnalysis.fontCharset = (char*)fontCharset;

  if (aShaper != NULL) {
    
    nsAutoString strBuf;
    strBuf.Assign(aString, aLength);

    // Convert Unicode string to UTF8 and store in buffer
    utf8Str = NULL;
    utf8Str = strBuf.ToNewUTF8String();
    inLen = strlen(utf8Str);

    aShaper->script_shape(fontCharset, utf8Str, inLen,
                          &aAnalysis, tmpGlyphs);
    nsMemory::Free(utf8Str);
    if (tmpGlyphs->num_glyphs > 0) {
      aSize = tmpGlyphs->num_glyphs;
      // if (*aOutLength < aSize)
      //    trouble
      for (int i = 0; i < aSize; i++) 
        aGlyphs[i] = tmpGlyphs->glyphs[i].glyph;
    }
    *aOutLength = (PRSize)aSize;
  }
  else {
    // Should not reach here are checks are set
    // in upper layers
    /* Alternately, if no Shaper - Copy Input to output */
  }
  return NS_OK;
}
