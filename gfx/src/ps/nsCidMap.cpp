/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab:
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
 * The Original Code is Golden Hills Computer Services code.
 *
 * The Initial Developer of the Original Code is
 * Brian Stell <bstell@ix.netcom.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brian Stell <bstell@ix.netcom.com>.
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

//
// This file has routines to build Postscript CID maps
//
// For general information on Postscript see:
//   Adobe Solutions Network: Technical Notes - Fonts
//   http://partners.adobe.com/asn/developer/technotes/fonts.html
//
// For information on CID maps see:
//
//   CID-Keyed Font Technology Overview
//   http://partners.adobe.com/asn/developer/pdfs/tn/5092.CID_Overview.pdf
//
//   Adobe CMap and CID Font Files Specification
//   http://partners.adobe.com/asn/developer/pdfs/tn/5014.CMap_CIDFont_Spec.pdf
//
//   Building CMap Files for CID-Keyed Fonts
//   http://partners.adobe.com/asn/developer/pdfs/tn/5099.CMapFiles.pdf
//
//

#include "nsCidMap.h"

CodeSpaceRangeElement UCS2_CodeSpaceRange[2] = {
  { 2, 0x0000, 0xD7FF },
  { 2, 0xE000, 0xFFFF },
};

int len_UCS2_CodeSpaceRange = 
             sizeof(UCS2_CodeSpaceRange)/sizeof(UCS2_CodeSpaceRange[0]);

void
WriteCidCharMap(const PRUnichar *aCharIDs, PRUint32 *aCIDs,
                int aLen, FILE *aFile)
{
  int i, blk_len;

  while (aLen) {
    /* determine the # of lines in this block */
    if (aLen >= 100)
      blk_len = 100;
    else
      blk_len = aLen;

    /* output the block */
    fprintf(aFile, "%d begincidchar\n", blk_len);
    for (i=0; i<blk_len; i++)
      fprintf(aFile, "<%04X> %d\n", aCharIDs[i], aCIDs[i]);
    fprintf(aFile, "endcidchar\n\n");

    /* setup for next block */
    aLen -= blk_len;
    aCharIDs += blk_len;
    aCIDs += blk_len;
  }
}

void
WriteCidRangeMapUnicode(FILE *aFile)
{
  int i;

  fprintf(aFile, "100 begincidrange\n");
  for (i=0; i<100; i++)
    fprintf(aFile, "<%04X> <%04X> %d\n", i*256, ((i+1)*256)-1, i*256);
  fprintf(aFile, "endcidrange\n\n");

  fprintf(aFile, "100 begincidrange\n");
  for (i=100; i<200; i++)
    fprintf(aFile, "<%04X> <%04X> %d\n", i*256, ((i+1)*256)-1, i*256);
  fprintf(aFile, "endcidrange\n\n");

  fprintf(aFile, "56 begincidrange\n");
  for (i=200; i<256; i++)
    fprintf(aFile, "<%04X> <%04X> %d\n", i*256, ((i+1)*256)-1, i*256);
  fprintf(aFile, "endcidrange\n\n");

}

void
WriteCmapHeader(const char *aName, const char *aRegistry, 
                const char *aEncoding, int aSupplement,
                int aType, int aWmode, FILE *aFile)
{
  fprintf(aFile, "%%%%DocumentNeededResources: procset CIDInit\n");
  fprintf(aFile, "%%%%IncludeResource: procset CIDInit\n");
  fprintf(aFile, "%%%%BeginResource: CMap %s\n", aName);
  fprintf(aFile, "%%%%Title: (%s %s %s %d)\n", aName, aRegistry, aEncoding,aSupplement);
  fprintf(aFile, "%%%%Version : 1\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "/CIDInit /ProcSet findresource begin\n");
  // IMPROVMENT: to add support for Postlevel 1 interpreters
  // IMPROVMENT: add code to test if CIDInit is defined 
  // IMPROVMENT: if not defined then add code here to define it
  fprintf(aFile, "\n");
  fprintf(aFile, "12 dict begin\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "begincmap\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "/CIDSystemInfo 3 dict dup begin\n");
  fprintf(aFile, "  /Registry (%s) def\n", aRegistry);
  fprintf(aFile, "  /Ordering (%s) def\n", aEncoding);
  fprintf(aFile, "  /Supplement %d def\n", aSupplement);
  fprintf(aFile, "end def\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "/CMapName /%s def\n", aName);
  fprintf(aFile, "\n");
  fprintf(aFile, "/CMapVersion 1 def\n");
  fprintf(aFile, "/CMapType %d def\n", aType);
  fprintf(aFile, "\n");
  fprintf(aFile, "/WMode %d def\n", aWmode);
  fprintf(aFile, "\n");
}

void
WriteCmapFooter(FILE *aFile)
{
  fprintf(aFile, "endcmap\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "CMapName currentdict /CMap defineresource pop\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "end\n");
  fprintf(aFile, "end\n");
  fprintf(aFile, "%%%%EndResource\n");
  fprintf(aFile, "\n");
}

PRBool
WriteCodeSpaceRangeMap(CodeSpaceRangeElement *aElements, int aLen, FILE *aFile)
{
  int i, blk_len;

  while (aLen) {
    /* determine the # of lines in this block */
    if (aLen >= 100)
      blk_len = 100;
    else
      blk_len = aLen;

    /* output the block */
    fprintf(aFile, "%d begincodespacerange\n", blk_len);
    for (i=0; i<blk_len; i++, aElements++) {
      if (aElements->num_bytes == 1)
        fprintf(aFile, "<%02X>   <%02X>\n", aElements->start, aElements->end);
      else if (aElements->num_bytes == 2)
        fprintf(aFile, "<%04X> <%04X>\n", aElements->start, aElements->end);
      else {
        fprintf(aFile, "codespacerange: invalid num_bytes (%d)\nexiting...\n", 
                aElements->num_bytes);
        return PR_FALSE;
      }
    }
    fprintf(aFile, "endcodespacerange\n\n");

    /* setup for next block */
    aLen -= blk_len;
  }
  return PR_TRUE;
}

void
WriteCodeSpaceRangeMapUCS2(FILE *aFile)
{
  WriteCodeSpaceRangeMap(UCS2_CodeSpaceRange, len_UCS2_CodeSpaceRange, aFile);
}

