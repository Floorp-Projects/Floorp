@!
@! The contents of this file are subject to the Netscape Public
@! License Version 1.1 (the "License"); you may not use this file
@! except in compliance with the License. You may obtain a copy of
@! the License at http://www.mozilla.org/NPL/
@!
@! Software distributed under the License is distributed on an "AS
@! IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
@! implied. See the License for the specific language governing
@! rights and limitations under the License.
@!
@! The Original Code is mozilla.org code.
@!
@! The Initial Developer of the Original Code is Netscape
@! Communications Corporation.  Portions created by Netscape are
@! Copyright (C) 1998 Netscape Communications Corporation. All
@! Rights Reserved.
@!
@! Contributor(s): 

@! This file is used to generate static hash table lookups.
@! A perl script merges this file  with the output of gperf to produce
@! the hash functions.  Lines starting with @! are comments.  Lines which do
@! not being with @! are copied straight to the output file. "@begin NAME
@! /REGEX1/ /REGEX2/" means to  skip lines in the input until REGEX1 is
@! matched, and then begin  saving output under name NAME, and stop when
@! REGEX2 is matched. "@include NAME" inserts the data saved as "NAME".
@! "@SUB NAME SUBREGEX" performs a substitution on the data saved in NAME.
@!
@! The following goop extracts the parts we need from the generated output
@! of gperf.  We later merge that goop with custom code to generate
@! the tag lookup function.
@!
@begin MACROS /#define/ /^$/
@begin HASH_TABLE /static unsigned [a-z]+ (asso_values|hash_table)/ /};/
@begin HASH_FUNC /register int hval = len;/ /return hval/
@sub HASH_FUNC /return hval \+/hval +=/
@sub HASH_FUNC /str\[/MYLOWER(str[/
@sub HASH_FUNC /]]/])]/
@begin LENGTH_TABLE /static unsigned char lengthtable/ /};/
@begin TAG_TABLE /static struct StaticNameTable *wordlist/ /};/
@!
/*
** This is a generated file, do not edit it. This file is created by
** genhash.pl
*/

#include "plstr.h"
@classincl
@include MACROS

struct StaticNameTable {
  char* tag;
  PRInt32 id;
};

static const unsigned char kLowerLookup[256] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,
    97,98,99,100,101,102,103,104,105,106,107,108,109,
    110,111,112,113,114,115,116,117,118,119,120,121,122,

   91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

  128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
  144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
  160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
  176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
  192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
  208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
  224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
  240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
};

#define MYLOWER(x) kLowerLookup[((x) & 0x7f)]

/**
 * Map a name to an ID or -1
 */
@classfunc
{
@include HASH_TABLE
@include LENGTH_TABLE
@include TAG_TABLE

  if (str != NULL) {
    int len = PL_strlen(str);
    if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
@include HASH_FUNC
@! "hval" now contains hash value
      if (hval <= MAX_HASH_VALUE && hval >= MIN_HASH_VALUE) {
        if (len == lengthtable[hval]) {
          register const char *tag = wordlist[hval].tag;

          /*
          ** While not at the end of the string, if they ever differ
          ** they are not equal.  We know "tag" is already lower case.
          */
          while ((*tag != '\0')&&(*str != '\0')) {
            if (*tag != (char) MYLOWER(*str)) {
              return -1;
            }
            tag++;
            str++;
          }

          /*
          ** One of the strings has ended, if they are both ended, then they
          ** are equal, otherwise not.
          */
          if ((*tag == '\0')&&(*str == '\0')) {
            return wordlist[hval].id;
          }
        }
      }
    }
  }
  return -1;
}
