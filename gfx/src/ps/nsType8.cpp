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
 * The Original Code is Golden Hills Computer Services code.
 *
 * The Initial Developer of the Original Code is
 * Brian Stell <bstell@ix.netcom.com>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Stell <bstell@ix.netcom.com>.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
// This code converts TrueType to Postscript Type 8 fonts. (Note 1)
//
// The main entry point is FT2SubsetToType8()
//
// Note 1: actually any FreeType2 font that support FT_Outline_Decompose
//         should work
//

#include <string.h>
#include <unistd.h>

#include "nsCidMap.h"
#include "nsType1.h"
#include "nsType8.h"
#ifndef MOZ_ENABLE_XFT
#include "nsIFreeType2.h"
#endif
#include "nsIServiceManager.h"
#include "nsISignatureVerifier.h"
#include "plbase64.h"
#include "nsCRT.h"

#define DEFAULT_CMAP_SIZE 10240

#define HEXASCII_LINE_LEN 64


static void hex_out(unsigned char *buf, PRUint32 n, FILE *f, PRUint32 *pos);

static void flatten_name(char *aToName);
#ifdef MOZ_ENABLE_XFT
static int FT2SubsetToCidKeyedType1(FT_Face aFace,
#else
static int FT2SubsetToCidKeyedType1(nsIFreeType2 *aFt2, FT_Face aFace,
#endif
                                    const PRUnichar *aCharIDs, int aLen,
                                    const char *aFontName,
                                    const char *aRegistry,
                                    const char *aEncoding, int aSupplement,
                                    int aWmode, int aLenIV, FILE *aFile);

void
AddCIDCheckCode(FILE *aFile)
{
  fprintf(aFile, "%%\n");
  fprintf(aFile, "%% Check if CID is supported\n");
  fprintf(aFile, "%%\n");
  fprintf(aFile, "/CanUseCIDFont false def\n");

  fprintf(aFile, "%% If level at least level 2 check if CID is supported\n");
  fprintf(aFile, "%% or it has been convert to (base) level 2\n");
  fprintf(aFile, "version cvi 2000 ge {\n");
  fprintf(aFile, "  /CIDInit /ProcSet resourcestatus\n");
  fprintf(aFile, "  { pop pop /CanUseCIDFont true def }\n");
  fprintf(aFile, "  { /MozConvertedToLevel2 where \n");
  fprintf(aFile, "    { pop MozConvertedToLevel2 { /CanUseCIDFont true def"
                        " } if } if\n");
  fprintf(aFile, "  }\n");
  fprintf(aFile, "  ifelse\n");
  fprintf(aFile, "} if\n");

  fprintf(aFile, "%% If it has been converted to level 1 it is very big but\n");
  fprintf(aFile, "%% guaranteed to be compatible\n");
  fprintf(aFile, "/MozConvertedToLevel1 where {\n");
  fprintf(aFile, "  pop MozConvertedToLevel1 { /CanUseCIDFont true def } if\n");
  fprintf(aFile, "} if\n");

  fprintf(aFile, "CanUseCIDFont not {\n");
  fprintf(aFile, "  %%\n");
  fprintf(aFile, "  %% Cannot print this so tell the user how to convert it\n");
  fprintf(aFile, "  %%\n");

  // IMPROVMENT: add the document title and/or user id
  fprintf(aFile, "  /Times-Roman findfont 12 scalefont setfont\n");
  fprintf(aFile, "  50 700 moveto currentpoint\n");
  fprintf(aFile, "  (The Postscript interpreter in your printer is ) show\n");
  fprintf(aFile, "  version show\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint\n");
  fprintf(aFile, "  (This printout requires at least version 2015 or "
                     "greater) show\n");

  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint\n");
  fprintf(aFile, "  (To make a Unix/Linux Gecko browser (eg: Netscape or "
                     "Mozilla) produce ) show\n");
  fprintf(aFile, "  (output that will work ) show\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint\n");
  fprintf(aFile, "  (on any level 2 interpreter change the \"Print Command\" "
                    "to use ) show\n");
  fprintf(aFile, "  (Ghostscript to convert the output) show\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint\n");
  fprintf(aFile, "  (down to basic level 2; eg: change the print command "
                    "from) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint 20 0 rmoveto\n");

  fprintf(aFile, "  (lpr [OPTIONS]) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint\n");
  fprintf(aFile, "  (to \\(all on one line\\)) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint 20 0 rmoveto\n");
  fprintf(aFile, "  (gs -q -sDEVICE=pswrite -sOutputFile=- -dNOPAUSE "
                    "-dBATCH ) show\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint 60 0 rmoveto\n");
  fprintf(aFile, "  (-dMozConvertedToLevel2=true - | lpr [OPTIONS]) show\n");
  fprintf(aFile, "  /Times-Roman findfont 8 scalefont setfont\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint\n");
  fprintf(aFile, "  (for level 1 use: ) show\n");
  fprintf(aFile, "  moveto 0 -15 rmoveto currentpoint 20 0 rmoveto\n");
  fprintf(aFile, "  (gs -q -sDEVICE=pswrite -sOutputFile=- -dNOPAUSE ) show\n");
  fprintf(aFile, "  ( -dBATCH -dLanguageLevel=1 ) show\n");
  fprintf(aFile, "  (-dMozConvertedToLevel1=true - | lpr [OPTIONS]) show\n");
  fprintf(aFile, "  /Times-Roman findfont 12 scalefont setfont\n");

  fprintf(aFile, "  moveto 0 -50 rmoveto currentpoint\n");
  fprintf(aFile, "  (If you are printing this from a file use ) show\n");
  fprintf(aFile, "  (Ghostscript to convert it to basic level 2;) show\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint\n");
  fprintf(aFile, "  (eg: change the print command from) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint 20 0 rmoveto\n");
  fprintf(aFile, "  (lpr [OPTIONS] <FILENAME>) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint\n");
  fprintf(aFile, "  (to) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint 20 0 rmoveto\n");
  fprintf(aFile, "  (gs -q -sDEVICE=pswrite -sOutputFile=- -dNOPAUSE "
                    "-dBATCH ) show\n");
  fprintf(aFile, "  moveto 0 -30 rmoveto currentpoint 40 0 rmoveto\n");
  fprintf(aFile, "  (-dMozConvertedToLevel2=true <FILENAME> | lpr [OPTIONS]"
                    ") show\n");
  fprintf(aFile, "  /Times-Roman findfont 8 scalefont setfont\n");
  fprintf(aFile, "  moveto 0 -20 rmoveto currentpoint\n");
  fprintf(aFile, "  (for level 1 use: ) show\n");
  fprintf(aFile, "  moveto 0 -15 rmoveto currentpoint 20 0 rmoveto\n");
  fprintf(aFile, "  (gs -q -sDEVICE=pswrite -sOutputFile=- -dNOPAUSE  -dBATCH "
                    ") show\n");
  fprintf(aFile, "  (-dLanguageLevel=1 -dMozConvertedToLevel1=true ) show\n");
  fprintf(aFile, "  (<FILENAME> | lpr [OPTIONS]) show\n");
  fprintf(aFile, "  /Times-Roman findfont 12 scalefont setfont\n");

  fprintf(aFile, "  showpage\n");
  fprintf(aFile, "  quit\n");
  fprintf(aFile, "} if\n");
  fprintf(aFile, "\n");
}

static char *
FT2ToType1FontName(FT_Face aFace, int aWmode)
{
  char *fontname;
  int name_len;

  name_len  = strlen(aFace->family_name);    // family name
  name_len += 1 + strlen(aFace->style_name); // '.' + style name
  name_len += 11;                            // '.' + face_index
  name_len += 2;                             // '.' + wmode
  name_len += 1;                             // string terminator

  fontname = (char *)PR_Malloc(name_len);
  if (!fontname) {
    return nsnull;
  }
  sprintf(fontname, "%s.%s.%ld.%d", aFace->family_name, aFace->style_name,
                                    aFace->face_index, aWmode?1:0);
  flatten_name(fontname);
  return fontname;
}


static char *
FontNameToType8CmapName(char *aFontName)
{
  char *cmapname;

  /* get cmap name */
  cmapname = (char *)PR_Malloc(strlen(aFontName)+1+5);
  if (!cmapname) {
    return nsnull;
  }
  sprintf((char*)cmapname, "%s_cmap", aFontName);
  return cmapname;
}

char *
FT2ToType8CidFontName(FT_Face aFace, int aWmode)
{
  int cidfontname_len;
  char *cmapname = NULL;
  char *fontname = NULL;
  char *cidfontname = NULL;

  fontname = FT2ToType1FontName(aFace, aWmode);
  if (!fontname) {
    goto done;
  }

  /* get cmap name */
  cmapname = FontNameToType8CmapName(fontname);
  if (!cmapname) {
    goto done;
  }

  /* generate the font name */
  cidfontname_len = strlen(fontname) + 2 + strlen(cmapname) + 1;
  cidfontname = (char *)PR_Malloc(cidfontname_len);
  if (!cidfontname) {
    NS_ERROR("Failed to alloc space for font name");
    goto done;
  }
  sprintf((char*)cidfontname, "%s--%s", fontname, cmapname);

done:
  if (fontname)
    PR_Free((void*)fontname);
  if (cmapname)
    PR_Free((void*)cmapname);
  return (char *)cidfontname;
}

char *
FT2SubsetToEncoding(const PRUnichar *aCharIDs, PRUint32 aNumChars)
{
  unsigned char* rawDigest;
  char *encoding = NULL;
  nsresult rv;
  PRUint32 len;
  HASHContextStr* id;

  //
  // generate a cryptographic digest of the charIDs to use as the "encoding"
  //
  nsCOMPtr<nsISignatureVerifier> verifier = 
           do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = verifier->HashBegin(nsISignatureVerifier::SHA1, &id);
    if (NS_SUCCEEDED(rv)) {
      rv = verifier->HashUpdate(id, (const char*)aCharIDs,
                                aNumChars*sizeof(PRUnichar));
      if (NS_SUCCEEDED(rv)) {
        rawDigest = (unsigned char*)PR_Malloc(
                                          nsISignatureVerifier::SHA1_LENGTH);
        if (rawDigest) {
          rv = verifier->HashEnd(id, &rawDigest, &len,
                                 nsISignatureVerifier::SHA1_LENGTH);
          if (NS_SUCCEEDED(rv)) {
            encoding = PL_Base64Encode((char*)rawDigest, len, encoding);
          }
          PR_Free(rawDigest);
          if (encoding)
            return encoding;
        }
      }
    }
  }

  // if psm is not available then just make a string based
  // on the time and a simple hash of the chars
  PRUint32 hash = nsCRT::HashCode(aCharIDs, &aNumChars);
  encoding = (char*)PR_Malloc(10+1+10+1+10+1);
  if (!encoding)
    return nsnull;

  PRTime tmp_now, now = PR_Now();
  PRTime usec_per_sec;
  PRUint32 t_sec, t_usec;
  LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
  LL_DIV(tmp_now, now, usec_per_sec);
  LL_L2I(t_sec, tmp_now);
  LL_MOD(tmp_now, now, usec_per_sec);
  LL_L2I(t_usec, tmp_now);
  sprintf(encoding, "%u.%u.%u", hash, t_sec, t_usec);
  return encoding;
}

PRBool
FT2SubsetToType8(FT_Face aFace, const PRUnichar *aCharIDs, PRUint32 aNumChars,
                 int aWmode, FILE *aFile)
{
  PRUint32 i;
  char *fontname = NULL;
  char *cmapname = NULL;
  char *cidfontname = NULL;
  char *registry;
  char *encoding = NULL;
  int supplement, lenIV;
  PRUint32 CIDs_buf[5000];
  PRUint32 *CIDs = CIDs_buf;
  int cmap_type = 0;
  PRBool status = PR_FALSE;

#ifndef MOZ_ENABLE_XFT
  nsresult rv;
  nsCOMPtr<nsIFreeType2> ft2 = do_GetService(NS_FREETYPE2_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_ERROR("Failed to get nsIFreeType2 service");
    goto done;
  }
#endif

  if ((aNumChars+1) > sizeof(CIDs_buf)/sizeof(CIDs_buf[0]))
    CIDs = (PRUint32 *)PR_Malloc((aNumChars+1)*sizeof(CIDs_buf[0]));
  if (!CIDs) {
    NS_ERROR("Failed to alloc space for CIDs");
    goto done;
  }

  fontname = FT2ToType1FontName(aFace, aWmode);
  if (!fontname)
    goto done;
  cmapname = FontNameToType8CmapName(fontname);
  if (!cmapname)
    goto done;
  cidfontname = FT2ToType8CidFontName(aFace, aWmode);
  if (!cidfontname)
    goto done;

  registry = "mozilla_printout";
  encoding = FT2SubsetToEncoding(aCharIDs, aNumChars);
  if (!encoding)
    goto done;

  supplement = 0;
  lenIV = 0;

  //
  // Build the Char to CID map
  //
  // reserve one space for the notdef glyph
  // then map the the chars sequentially to glyphs
  //
  for (i=0; i<aNumChars; i++) {
    CIDs[i] = i+1;
  }
  /* output the CMap */
  WriteCmapHeader(cmapname, registry, encoding, supplement, cmap_type, aWmode,
                  aFile);
  WriteCodeSpaceRangeMapUCS2(aFile);
  WriteCidCharMap(aCharIDs, CIDs, aNumChars, aFile);
  WriteCmapFooter(aFile);

  /* output the Type 8 CID font */
#ifdef MOZ_ENABLE_XFT
  FT2SubsetToCidKeyedType1(aFace, aCharIDs, aNumChars, fontname,
#else
  FT2SubsetToCidKeyedType1(ft2, aFace, aCharIDs, aNumChars, fontname,
#endif
                           registry, encoding, supplement, aWmode, lenIV,
                           aFile);
  fprintf(aFile, "\n");

  /* compose the cmap and font */
  fprintf(aFile, "/%s\n", cidfontname);
  fprintf(aFile, "  /%s /CMap findresource\n", cmapname);
  fprintf(aFile, "  [/%s /CIDFont findresource]\n", fontname);
  fprintf(aFile, "  composefont pop\n");
  fprintf(aFile, "\n");
  status = PR_TRUE;

done:
  PR_FREEIF(fontname);
  PR_FREEIF(cmapname);
  PR_FREEIF(encoding);
  PR_FREEIF(cidfontname);
  if (CIDs != CIDs_buf)
    PR_Free(CIDs);
  return status;
}

static PRBool
#ifdef MOZ_ENABLE_XFT
FT2SubsetToCidKeyedType1(FT_Face aFace,
#else
FT2SubsetToCidKeyedType1(nsIFreeType2 *aFt2, FT_Face aFace,
#endif
                         const PRUnichar *aCharIDs,
                         int aLen, const char *aFontName,
                         const char *aRegistry, const char *aEncoding,
                         int aSupplement, int aWmode, int aLenIV, FILE *aFile)
{
  int i, charstrings_len, data_offset;
  unsigned int charstring_len;
  PRUint32 glyphID, data_len, data_len2, max_charstring, line_pos;
  unsigned int cmapinfo_buf[DEFAULT_CMAP_SIZE+1];
  unsigned int *cmapinfo = cmapinfo_buf;
  unsigned char charstring_buf[1024];
  unsigned char *charstring = charstring_buf;
  int fd_len, gd_len, cidmap_len, cmap_array_len, num_charstrings;
  FT_UShort upm = aFace->units_per_EM;

  if (aLen+2 > DEFAULT_CMAP_SIZE) {
    cmapinfo = (unsigned int*)PR_Calloc(aLen+2, sizeof(unsigned int));
    if (!cmapinfo)
      return PR_FALSE;
  }

  fprintf(aFile, "%%%%DocumentNeededResources: procset CIDInit\n");
  fprintf(aFile, "%%%%IncludeResource: procset CIDInit\n");
  fprintf(aFile, "%%%%BeginResource: CIDFont %s\n", aFontName);
  fprintf(aFile, "%%%%Title: (%s %s %s %d)\n", aFontName, aRegistry,
                 aEncoding, aSupplement);
  fprintf(aFile, "%%%%Version: 1\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "/CIDInit /ProcSet findresource begin\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "20 dict begin\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "/CIDFontName /%s def\n", aFontName);
  fprintf(aFile, "/CIDFontVersion 1 def\n");
  fprintf(aFile, "/CIDFontType 0 def\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "/CIDSystemInfo 3 dict dup begin\n");
  fprintf(aFile, "  /Registry (%s) def \n", aRegistry);
  fprintf(aFile, "  /Ordering (%s) def \n", aEncoding);
  fprintf(aFile, "  /Supplement 0 def \n");
  fprintf(aFile, "end def\n");
  fprintf(aFile, "\n");

  fprintf(aFile, "/FontBBox [%d %d %d %d] def\n", 
                 toCS(upm, aFace->bbox.xMin),
                 toCS(upm, aFace->bbox.yMin),
                 toCS(upm, aFace->bbox.xMax),
                 toCS(upm, aFace->bbox.yMax));

  fprintf(aFile, "\n");

  /* measure the notdef glyph length */
#ifdef MOZ_ENABLE_XFT
  cmapinfo[0] = FT2GlyphToType1CharString(aFace, 0, aWmode, aLenIV, NULL);
#else
  cmapinfo[0] = FT2GlyphToType1CharString(aFt2, aFace, 0, aWmode, aLenIV, NULL);
#endif
  num_charstrings = 1;
  charstrings_len = cmapinfo[0];

  /* get charstring lengths */
  max_charstring = cmapinfo[0];
  for (i=0; i<aLen; i++) {
#ifdef MOZ_ENABLE_XFT
    glyphID = FT_Get_Char_Index(aFace, aCharIDs[i]);
    cmapinfo[i+1] = FT2GlyphToType1CharString(aFace, glyphID, aWmode,
                                              aLenIV, NULL);
#else
    aFt2->GetCharIndex(aFace, aCharIDs[i], &glyphID);
    cmapinfo[i+1] = FT2GlyphToType1CharString(aFt2, aFace, glyphID, aWmode,
                                              aLenIV, NULL);
#endif
    charstrings_len += cmapinfo[i+1];
    if (cmapinfo[i+1])
      num_charstrings++;
    if (cmapinfo[i+1] > max_charstring)
      max_charstring = cmapinfo[i+1];
  }
  cmapinfo[i+1] = 0;

  if (max_charstring > sizeof(charstring_buf))
    charstring = (unsigned char *)PR_Malloc(max_charstring);
  if (!charstring) {
    NS_ERROR("Failed to alloc bytes for charstring");
    return PR_FALSE;
  }

  line_pos = 0;
  fd_len = 0;
  gd_len = 3;
  cmap_array_len = 1 + aLen + 1;
  cidmap_len = (fd_len+gd_len) * cmap_array_len;
  fprintf(aFile, "/CIDMapOffset %d def\n", 0);
  fprintf(aFile, "/FDBytes %d def\n", fd_len);
  fprintf(aFile, "/GDBytes %d def\n", gd_len);
  fprintf(aFile, "/CIDCount %d def\n", cmap_array_len-1);
  fprintf(aFile, "\n");

  fprintf(aFile, "/FDArray 1 array\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "dup 0\n");
  fprintf(aFile, "  %%ADOBeginFontDict\n");
  fprintf(aFile, "  14 dict begin\n");
  fprintf(aFile, "  \n");
  fprintf(aFile, "  /FontName /%s-Proportional def\n", aFontName);
  fprintf(aFile, "  /FontType 1 def\n");
  fprintf(aFile, "  /FontMatrix [ 0.001 0 0 0.001 0 0 ] def\n");
  fprintf(aFile, "  /PaintType 0 def\n");
  fprintf(aFile, "  \n");
  fprintf(aFile, "  %%ADOBeginPrivateDict\n");
  fprintf(aFile, "  /Private 25 dict dup begin\n");
  fprintf(aFile, "    /lenIV %d def\n", aLenIV);
  fprintf(aFile, "    /SubrCount 0 def\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "  end def\n");
  fprintf(aFile, "  %%ADOEndPrivateDict\n");
  fprintf(aFile, "currentdict end\n");
  fprintf(aFile, "%%ADOEndFontDict\n");
  fprintf(aFile, "put\n");
  fprintf(aFile, "\n");
  fprintf(aFile, "def\n");
  fprintf(aFile, "\n");

  data_len = cidmap_len + charstrings_len;

  //
  // output the BeginData tag
  //
  // binary data is 1/2 the size of hexascii data but printers
  // get confused by it so we don't use it
  //
  data_len2  = 27; // "(Hex) %10d StartData\n"
  data_len2 += (data_len+1)*2; // the data (+pad byte) in hexascii
  // new-lines at the end of the hexascii lines
  data_len2 += ((data_len+1)*2)/HEXASCII_LINE_LEN;
  data_len2 += 2; // EOD + new-line

  fprintf(aFile, "%%%%BeginData: %d Binary Bytes\n", data_len2);
  fprintf(aFile, "(Hex) %10d StartData\n", data_len);

  //
  // output the CIDMap
  //
  data_offset = cidmap_len;
  for (i=0; i<cmap_array_len; i++) {
    unsigned char buf[4];
    int l = 0;
    if (fd_len == 1)
      buf[l++] = 0;
    else if (fd_len != 0){
      NS_ASSERTION(fd_len==0||fd_len==1, "only support fd_len == 0 or 1");
      return PR_FALSE;
    }

    if (gd_len == 3) {
      buf[l++] = (data_offset >> 16) & 0xFF;
      buf[l++] = (data_offset >> 8) & 0xFF;
      buf[l++] = data_offset & 0xFF;
    }
    else {
      NS_ASSERTION(gd_len==3, "only support gd_len == 3");
      return PR_FALSE;
    }
    hex_out(buf, fd_len + gd_len, aFile, &line_pos);
    data_offset += cmapinfo[i];
  }

  //
  // output the charStrings
  //
  // output the notdef glyph
#ifdef MOZ_ENABLE_XFT
  charstring_len = FT2GlyphToType1CharString(aFace, 0, aWmode, aLenIV,
                                             charstring);
#else
  charstring_len = FT2GlyphToType1CharString(aFt2, aFace, 0, aWmode, aLenIV,
                                             charstring);
#endif
  hex_out(charstring, charstring_len, aFile, &line_pos);

  /* output the charstrings for the glyphs */
  for (i=0; i<aLen; i++) {
#ifdef MOZ_ENABLE_XFT
    glyphID = FT_Get_Char_Index(aFace, aCharIDs[i]);
    charstring_len = FT2GlyphToType1CharString(aFace, glyphID, aWmode,
                                               aLenIV, charstring);
#else
    aFt2->GetCharIndex(aFace, aCharIDs[i], &glyphID);
    charstring_len = FT2GlyphToType1CharString(aFt2, aFace, glyphID, aWmode,
                                               aLenIV, charstring);
#endif
    NS_ASSERTION(charstring_len==cmapinfo[i+1], "glyph data changed");
    hex_out(charstring, charstring_len, aFile, &line_pos);
  }
  // add one byte padding so the interperter can always do 2 byte reads
  charstring[0] = 0;
  hex_out(charstring, 1, aFile, &line_pos);

  // Output the EOD
  fprintf(aFile, ">\n"); // EOD
  /* StartData does an implicit "end end" */
  fprintf(aFile, "%%%%EndData\n");
  fprintf(aFile, "%%%%EndResource\n");

  if (cmapinfo != cmapinfo_buf) {
    PR_Free(cmapinfo);
  }

  if (charstring != charstring_buf) {
    PR_Free(charstring);
  }
  return PR_TRUE;
}

static void
flatten_name(char *aString)
{
  for (; *aString; aString++) {
    if (*aString == ' ')
      *aString = '_';
    else if (*aString == '(')
      *aString = '_';
    else if (*aString == ')')
      *aString = '_';
  }
}

static void
hex_out(unsigned char *aBuf, PRUint32 aLen, FILE *f, PRUint32 *line_pos)
{
  PRUint32 i, pos = *line_pos;
  for (i=0; i<aLen; i++) {
    fprintf(f, "%02X", aBuf[i]);
    pos += 2;
    if (pos >= HEXASCII_LINE_LEN) {
      fprintf(f, "\n");
      pos = 0;
    }
  }
  *line_pos = pos;
}

