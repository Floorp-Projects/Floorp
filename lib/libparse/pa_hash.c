/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
** This is a generated file, do not edit it.  If you need to make changes,
** edit the file pa_hash.template and re-build pa_hash.c on a UNIX machine.
** This whole hacky thing was done by Michael Toy.
*/

#include "pa_parse.h"
#define TOTAL_KEYWORDS 103
#define MIN_WORD_LENGTH 1
#define MAX_WORD_LENGTH 12
#define MIN_HASH_VALUE 1
#define MAX_HASH_VALUE 333
/* maximum key range = 333, duplicates = 0 */

#define MYLOWER(x) TOLOWER(((x) & 0x7f))

/*************************************
 * Function: pa_tokenize_tag
 *
 * Description: This function maps the passed in string
 * 		to one of the valid tag element tokens, or to
 *		the UNKNOWN token.
 *
 * Params: Takes a \0 terminated string.
 *
 * Returns: a 32 bit token to describe this tag element.  On error,
 * 	    which means it was not passed an unknown tag element string,
 * 	    it returns the token P_UNKNOWN.
 *
 * Performance Notes:
 * Profiling on mac revealed this routine as a big (5%) time sink.
 * This function was stolen from pa_mdl.c and merged with the perfect
 * hashing code and the tag comparison code so it would be flatter (fewer
 * function calls) since those are still expensive on 68K and x86 machines.
 *************************************/

intn
pa_tokenize_tag(char *str)
{
  static unsigned short asso_values[] =
    {
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334,  40,
      10,  15,  20,  25,  30, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 334, 334, 334,
     334, 334, 334, 334, 334, 334, 334, 127,  10,  10,
      55,   0,  15,  20,  67,  12,  25,  50,  45,  35,
     115,  65,   5,   0,  75,   0,   0,  62,   0,   0,
      15,  10, 334, 334, 334, 334, 334, 334,
    };
  static unsigned char lengthtable[] =
    {
      0,  1,  2,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,
      0,  5,  0,  5,  0,  0,  0,  1,  0,  0,  0,  1,  4,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  7,  0,  0,  0,  0,  6,  0,  0,  0,  5,
      0,  2,  0,  0,  0,  0,  0,  3,  0,  3,  0,  0,  0,  0,
      3,  2,  2,  0,  0,  3,  0,  0,  0,  0,  0,  6,  0,  3,
      7,  3,  4,  0,  6,  2,  3,  6,  0,  0,  0,  0, 11,  0,
      0,  2,  0,  6,  0,  6,  4,  5,  6,  0,  0,  2,  0,  0,
      2,  0,  0,  0,  0,  0,  0,  2,  0,  0,  5,  0,  0,  1,
      0,  0,  0,  2, 10,  0,  0,  8,  4, 10,  2,  5,  0,  0,
      0,  4,  5,  6,  4,  8,  0,  2,  0,  2,  8,  4,  2,  0,
      2,  0,  6,  2,  0,  0,  0,  0,  2,  3,  0,  0,  4,  2,
      0,  0,  0,  4,  0,  3,  4,  3,  0,  0,  0,  5,  0,  6,
      0, 11,  0,  0,  9, 12,  8,  0,  0,  6,  0,  0,  4,  8,
      0,  0,  0,  4,  0,  0,  0,  8,  0,  0,  6,  0,  0,  0,
      0,  7,  5,  6,  0,  0,  4,  0,  0,  2,  3,  0,  5,  0,
      0,  8,  4,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  7,  0,  7,  0,  0,  5,  0,  9,  0,  4,
      0,  4,  0,  1,  0,  0,  0,  0,  0,  0,  5,  0,  7,  6,
      0,  0,  0,  4,  0,  4,  0,  0,  0,  0,  0,  0,  0,  0,
      3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  7,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  4,
    };
  static struct pa_TagTable wordlist[] =
    {
      {"",}, 
      {"s",  P_STRIKE},
      {"tt",  P_FIXED},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"p",  P_PARAGRAPH},
      {"",}, {"",}, {"",}, 
      {"style",  P_STYLE},
      {"",}, 
      {"title",  P_TITLE},
      {"",}, {"",}, {"",}, 
      {"b",  P_BOLD},
      {"",}, {"",}, {"",}, 
      {"i",  P_ITALIC},
      {"cite",  P_CITATION},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"isindex",  P_INDEX},
      {"",}, {"",}, {"",}, {"",}, 
      {"select",  P_SELECT},
      {"",}, {"",}, {"",}, 
      {"spell",  P_SPELL},
      {"",}, 
      {"dt",  P_DESC_TITLE},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"xmp",  P_PLAIN_PIECE},
      {"",}, 
      {"big",  P_BIG},
      {"",}, {"",}, {"",}, {"",}, 
      {"div",  P_DIVISION},
      {"li",  P_LIST_ITEM},
      {"em",  P_EMPHASIZED},
      {"",}, {"",}, 
      {"sup",  P_SUPER},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"strike",  P_STRIKEOUT},
      {"",}, 
      {"pre",  P_PREFORMAT},
      {"listing",  P_LISTING_TEXT},
      {"sub",  P_SUB},
      {"hype",  P_HYPE},
      {"",}, 
      {"subdoc",  P_SUBDOC},
      {"h2",  P_HEADER_2},
      {"img",  P_IMAGE},
      {"script",  P_SCRIPT},
      {"",}, {"",}, {"",}, {"",}, 
      {"certificate",  P_CERTIFICATE},
      {"",}, {"",}, 
      {"h3",  P_HEADER_3},
      {"",}, 
      {"strong",  P_STRONG},
      {"",}, 
      {"mquote",  P_MQUOTE},
      {"cell",  P_CELL},
      {"embed",  P_EMBED},
      {"object",  P_OBJECT},
      {"",}, {"",}, 
      {"h4",  P_HEADER_4},
      {"",}, {"",}, 
      {"td",  P_TABLE_DATA},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"h5",  P_HEADER_5},
      {"",}, {"",}, 
      {"blink",  P_BLINK},
      {"",}, {"",}, 
      {"u",  P_UNDERLINE},
      {"",}, {"",}, {"",}, 
      {"h6",  P_HEADER_6},
      {"blockquote",  P_BLOCKQUOTE},
      {"",}, {"",}, 
      {"colormap",  P_COLORMAP},
      {"code",  P_CODE},
      {"nscp_close",  P_NSCP_CLOSE},
      {"th",  P_TABLE_HEADER},
      {"input",  P_INPUT},
      {"",}, {"",}, {"",}, 
      {"base",  P_BASE},
      {"table",  P_TABLE},
      {"applet",  P_JAVA_APPLET},
      {"body",  P_BODY},
      {"basefont",  P_BASEFONT},
      {"",}, 
      {"dl",  P_DESC_LIST},
      {"",}, 
      {"h1",  P_HEADER_1},
      {"textarea",  P_TEXTAREA},
      {"html",  P_HTML},
      {"tr",  P_TABLE_ROW},
      {"",}, 
      {"ul",  P_UNUM_LIST},
      {"",}, 
      {"server",  P_SERVER},
      {"ol",  P_NUM_LIST},
      {"",}, {"",}, {"",}, {"",}, 
      {"br",  P_LINEBREAK},
      {"wbr",  P_WORDBREAK},
      {"",}, {"",}, 
      {"meta",  P_META},
      {"dd",  P_DESC_TEXT},
      {"",}, {"",}, {"",}, 
      {"samp",  P_SAMPLE},
      {"",}, 
      {"kbd",  P_KEYBOARD},
      {"nsdt",  P_NSDT},
      {"map",  P_MAP},
      {"",}, {"",}, {"",}, 
      {"image",  P_NEW_IMAGE},
      {"",}, 
      {"keygen",  P_KEYGEN},
      {"",}, 
      {"inlineinput",  P_INLINEINPUT},
      {"",}, {"",}, 
      {"plaintext",  P_PLAIN_TEXT},
      {"nscp_reblock",  P_NSCP_REBLOCK},
      {"noscript",  P_NOSCRIPT},
      {"",}, {"",}, 
      {"option",  P_OPTION},
      {"",}, {"",}, 
      {"form",  P_FORM},
      {"multicol",  P_MULTICOLUMN},
      {"",}, {"",}, {"",}, 
      {"font",  P_FONT},
      {"",}, {"",}, {"",}, 
      {"noframes",  P_NOGRIDS},
      {"",}, {"",}, 
      {"center",  P_CENTER},
      {"",}, {"",}, {"",}, {"",}, 
      {"charles",  P_INLINEINPUTDOTTED},
      {"small",  P_SMALL},
      {"spacer",  P_SPACER},
      {"",}, {"",}, 
      {"menu",  P_MENU},
      {"",}, {"",}, 
      {"hr",  P_HRULE},
      {"dir",  P_DIRECTORY},
      {"",}, 
      {"frame",  P_GRID_CELL},
      {"",}, {"",}, 
      {"frameset",  P_GRID},
      {"link",  P_LINK},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"noembed",  P_NOEMBED},
      {"",}, 
      {"address",  P_ADDRESS},
      {"",}, {"",}, 
      {"param",  P_PARAM},
      {"",}, 
      {"nscp_open",  P_NSCP_OPEN},
      {"",}, 
      {"span",  P_SPAN},
      {"",}, 
      {"head",  P_HEAD},
      {"",}, 
      {"a",  P_ANCHOR},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"layer",  P_LAYER},
      {"",}, 
      {"caption",  P_CAPTION},
      {"ilayer",  P_ILAYER},
      {"",}, {"",}, {"",}, 
      {"nobr",  P_NOBREAK},
      {"",}, 
      {"jean",  P_INLINEINPUTTHICK},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"var",  P_VARIABLE},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"nolayer",  P_NOLAYER},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"area",  P_AREA},
    };

  if (str != NULL)
  {
    int len = strlen(str);
    if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
  register int hval = len;

  switch (hval)
    {
      default:
      case 3:
        hval += asso_values[MYLOWER(str[2])];
      case 2:
        hval += asso_values[MYLOWER(str[1])];
      case 1:
        hval += asso_values[MYLOWER(str[0])];
        break;
    }
  hval += asso_values[MYLOWER(str[len - 1])];
      if (hval <= MAX_HASH_VALUE && hval >= MIN_HASH_VALUE)
      {
	if (len == lengthtable[hval])
	{
	  register char *tag = wordlist[hval].name;
	  /*
	  ** The following code was stolen from pa_TagEqual,
	  ** again to make this function flatter.
	  */

	  /*
	  ** While not at the end of the string, if they ever differ
	  ** they are not equal.  We know "tag" is already lower case.
	  */
	  while ((*tag != '\0')&&(*str != '\0'))
	  {
	    if (*tag != (char) TOLOWER(*str))
	      return(P_UNKNOWN);
	    tag++;
	    str++;
	  }
	  /*
	  ** One of the strings has ended, if they are both ended, then they
	  ** are equal, otherwise not.
	  */
	  if ((*tag == '\0')&&(*str == '\0'))
	    return wordlist[hval].id;
	}
      }
    }
  }
  return(P_UNKNOWN);
}
