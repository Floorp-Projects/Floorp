/*
 * Copyright © 2011,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_OS2_TABLE_HH
#define HB_OT_OS2_TABLE_HH

#include "hb-open-type-private.hh"


namespace OT {

/*
 * OS/2 and Windows Metrics
 * http://www.microsoft.com/typography/otspec/os2.htm
 */

#define HB_OT_TAG_os2 HB_TAG('O','S','/','2')

struct os2
{
  static const hb_tag_t tableTag = HB_OT_TAG_os2;

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  UINT16	version;

  /* Version 0 */
  INT16		xAvgCharWidth;
  UINT16	usWeightClass;
  UINT16	usWidthClass;
  UINT16	fsType;
  INT16		ySubscriptXSize;
  INT16		ySubscriptYSize;
  INT16		ySubscriptXOffset;
  INT16		ySubscriptYOffset;
  INT16		ySuperscriptXSize;
  INT16		ySuperscriptYSize;
  INT16		ySuperscriptXOffset;
  INT16		ySuperscriptYOffset;
  INT16		yStrikeoutSize;
  INT16		yStrikeoutPosition;
  INT16		sFamilyClass;
  UINT8		panose[10];
  UINT32		ulUnicodeRange[4];
  Tag		achVendID;
  UINT16	fsSelection;
  UINT16	usFirstCharIndex;
  UINT16	usLastCharIndex;
  INT16		sTypoAscender;
  INT16		sTypoDescender;
  INT16		sTypoLineGap;
  UINT16	usWinAscent;
  UINT16	usWinDescent;

  /* Version 1 */
  //UINT32 ulCodePageRange1;
  //UINT32 ulCodePageRange2;

  /* Version 2 */
  //INT16 sxHeight;
  //INT16 sCapHeight;
  //UINT16  usDefaultChar;
  //UINT16  usBreakChar;
  //UINT16  usMaxContext;

  /* Version 5 */
  //UINT16  usLowerOpticalPointSize;
  //UINT16  usUpperOpticalPointSize;

  public:
  DEFINE_SIZE_STATIC (78);
};

} /* namespace OT */


#endif /* HB_OT_OS2_TABLE_HH */
