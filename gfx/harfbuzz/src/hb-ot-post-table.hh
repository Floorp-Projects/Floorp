/*
 * Copyright © 2016  Google, Inc.
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

#ifndef HB_OT_POST_TABLE_HH
#define HB_OT_POST_TABLE_HH

#include "hb-open-type-private.hh"

#define NUM_FORMAT1_NAMES 258

static const char* const format1_names[NUM_FORMAT1_NAMES] =
{
  ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl",
  "numbersign", "dollar", "percent", "ampersand", "quotesingle", "parenleft",
  "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
  "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
  "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at",
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
  "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft",
  "backslash", "bracketright", "asciicircum", "underscore", "grave", "a", "b",
  "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q",
  "r", "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar",
  "braceright", "asciitilde", "Adieresis", "Aring", "Ccedilla", "Eacute",
  "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave", "acircumflex",
  "adieresis", "atilde", "aring", "ccedilla", "eacute", "egrave",
  "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis",
  "ntilde", "oacute", "ograve", "ocircumflex", "odieresis", "otilde", "uacute",
  "ugrave", "ucircumflex", "udieresis", "dagger", "degree", "cent", "sterling",
  "section", "bullet", "paragraph", "germandbls", "registered", "copyright",
  "trademark", "acute", "dieresis", "notequal", "AE", "Oslash", "infinity",
  "plusminus", "lessequal", "greaterequal", "yen", "mu", "partialdiff",
  "summation", "product", "pi", "integral", "ordfeminine", "ordmasculine",
  "Omega", "ae", "oslash", "questiondown", "exclamdown", "logicalnot",
  "radical", "florin", "approxequal", "Delta", "guillemotleft",
  "guillemotright", "ellipsis", "nonbreakingspace", "Agrave", "Atilde",
  "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright",
  "quoteleft", "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis",
  "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl",
  "daggerdbl", "periodcentered", "quotesinglbase", "quotedblbase",
  "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis", "Egrave",
  "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex",
  "apple", "Ograve", "Uacute", "Ucircumflex", "Ugrave", "dotlessi",
  "circumflex", "tilde", "macron", "breve", "dotaccent", "ring", "cedilla",
  "hungarumlaut", "ogonek", "caron", "Lslash", "lslash", "Scaron", "scaron",
  "Zcaron", "zcaron", "brokenbar", "Eth", "eth", "Yacute", "yacute", "Thorn",
  "thorn", "minus", "multiply", "onesuperior", "twosuperior", "threesuperior",
  "onehalf", "onequarter", "threequarters", "franc", "Gbreve", "gbreve",
  "Idotaccent", "Scedilla", "scedilla", "Cacute", "cacute", "Ccaron", "ccaron",
  "dcroat",
};

namespace OT {


/*
 * post -- PostScript
 */

#define HB_OT_TAG_post HB_TAG('p','o','s','t')


struct postV2Tail
{
  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (numberOfGlyphs.sanitize (c) &&
		  c->check_array (glyphNameIndex, sizeof (USHORT), numberOfGlyphs));
  }

  USHORT	numberOfGlyphs;		/* Number of glyphs (this should be the
					 * same as numGlyphs in 'maxp' table). */
  USHORT	glyphNameIndex[VAR];	/* This is not an offset, but is the
					 * ordinal number of the glyph in 'post'
					 * string tables. */
  BYTE		namesX[VAR];		/* Glyph names with length bytes [variable]
					 * (a Pascal string). */

  DEFINE_SIZE_ARRAY2 (2, glyphNameIndex, namesX);
};

struct post
{
  static const hb_tag_t tableTag = HB_OT_TAG_post;

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this)))
      return_trace (false);
    if (version.to_int () == 0x00020000)
    {
      const postV2Tail &v2 = StructAfter<postV2Tail> (*this);
      return_trace (v2.sanitize (c));
    }
    return_trace (true);
  }

  inline bool get_glyph_name (hb_codepoint_t glyph,
			      char *buffer, unsigned int buffer_length,
			      unsigned int blob_len) const
  {
    if (version.to_int () == 0x00010000)
    {
      if (glyph >= NUM_FORMAT1_NAMES)
	return false;

      if (!buffer_length)
	return true;
      strncpy (buffer, format1_names[glyph], buffer_length);
      buffer[buffer_length - 1] = '\0';
      return true;
    }

    if (version.to_int () == 0x00020000)
    {
      const postV2Tail &v2 = StructAfter<postV2Tail> (*this);

      if (glyph >= v2.numberOfGlyphs)
	return false;

      if (!buffer_length)
	return true;

      unsigned int index = v2.glyphNameIndex[glyph];
      if (index < NUM_FORMAT1_NAMES)
      {
	if (!buffer_length)
	  return true;
	strncpy (buffer, format1_names[index], buffer_length);
	buffer[buffer_length - 1] = '\0';
	return true;
      }
      index -= NUM_FORMAT1_NAMES;

      unsigned int offset = min_size + v2.min_size + 2 * v2.numberOfGlyphs;
      unsigned char *data = (unsigned char *) this + offset;
      unsigned char *end = (unsigned char *) this + blob_len;
      for (unsigned int i = 0; data < end; i++)
      {
	unsigned int name_length = data[0];
	data++;
	if (i == index)
	{
	  if (unlikely (!name_length))
	    return false;

	  unsigned int remaining = end - data;
	  name_length = MIN (name_length, buffer_length - 1);
	  name_length = MIN (name_length, remaining);
	  memcpy (buffer, data, name_length);
	  buffer[name_length] = '\0';
	  return true;
	}
	data += name_length;
      }

      return false;
    }

    return false;
  }

  inline bool get_glyph_from_name (const char *name, int len,
				   hb_codepoint_t *glyph,
				   unsigned int blob_len) const
  {
    if (len < 0)
      len = strlen (name);

    if (version.to_int () == 0x00010000)
    {
      for (int i = 0; i < NUM_FORMAT1_NAMES; i++)
      {
	if (strncmp (name, format1_names[i], len) == 0 && format1_names[i][len] == '\0')
	{
	  *glyph = i;
	  return true;
	}
      }
      return false;
    }

    if (version.to_int () == 0x00020000)
    {
      const postV2Tail &v2 = StructAfter<postV2Tail> (*this);
      unsigned int offset = min_size + v2.min_size + 2 * v2.numberOfGlyphs;
      char* data = (char*) this + offset;


      /* XXX The following code is wrong. */
      return false;
      for (hb_codepoint_t gid = 0; gid < v2.numberOfGlyphs; gid++)
      {
	unsigned int index = v2.glyphNameIndex[gid];
	if (index < NUM_FORMAT1_NAMES)
	{
	  if (strncmp (name, format1_names[index], len) == 0 && format1_names[index][len] == '\0')
	  {
	    *glyph = gid;
	    return true;
	  }
	  continue;
	}
	index -= NUM_FORMAT1_NAMES;

	for (unsigned int i = 0; data < (char*) this + blob_len; i++)
	{
	  unsigned int name_length = data[0];
	  unsigned int remaining = (char*) this + blob_len - data - 1;
	  name_length = MIN (name_length, remaining);
	  if (name_length == (unsigned int) len && strncmp (name, data + 1, len) == 0)
	  {
	    *glyph = gid;
	    return true;
	  }
	  data += name_length + 1;
	}
	return false;
      }

      return false;
    }

    return false;
  }

  public:
  FixedVersion<>version;		/* 0x00010000 for version 1.0
					 * 0x00020000 for version 2.0
					 * 0x00025000 for version 2.5 (deprecated)
					 * 0x00030000 for version 3.0 */
  Fixed		italicAngle;		/* Italic angle in counter-clockwise degrees
					 * from the vertical. Zero for upright text,
					 * negative for text that leans to the right
					 * (forward). */
  FWORD		underlinePosition;	/* This is the suggested distance of the top
					 * of the underline from the baseline
					 * (negative values indicate below baseline).
					 * The PostScript definition of this FontInfo
					 * dictionary key (the y coordinate of the
					 * center of the stroke) is not used for
					 * historical reasons. The value of the
					 * PostScript key may be calculated by
					 * subtracting half the underlineThickness
					 * from the value of this field. */
  FWORD		underlineThickness;	/* Suggested values for the underline
					   thickness. */
  ULONG		isFixedPitch;		/* Set to 0 if the font is proportionally
					 * spaced, non-zero if the font is not
					 * proportionally spaced (i.e. monospaced). */
  ULONG		minMemType42;		/* Minimum memory usage when an OpenType font
					 * is downloaded. */
  ULONG		maxMemType42;		/* Maximum memory usage when an OpenType font
					 * is downloaded. */
  ULONG		minMemType1;		/* Minimum memory usage when an OpenType font
					 * is downloaded as a Type 1 font. */
  ULONG		maxMemType1;		/* Maximum memory usage when an OpenType font
					 * is downloaded as a Type 1 font. */
/*postV2Tail	v2[VAR];*/
  DEFINE_SIZE_STATIC (32);
};

} /* namespace OT */


#endif /* HB_OT_POST_TABLE_HH */
