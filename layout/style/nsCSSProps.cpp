
/*
** This is a generated file, do not edit it. This file is created by
** genhash.pl
*/

#include "plstr.h"
#include "nsCSSProps.h"
#define TOTAL_KEYWORDS 79
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 21
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 238
/* maximum key range = 233, duplicates = 0 */


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
PRInt32 nsCSSProps::LookupName(const char* str)
{
  static unsigned char asso_values[] =
    {
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239,  35, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239, 239, 239, 239,
     239, 239, 239, 239, 239, 239, 239,  45,   0,  85,
      80,  25,   5,   0,  80,   0, 239, 239,  25,  90,
       0,   0,   5, 239,   0,  65,  40,  35,   0,  80,
      10,  15,   5, 239, 239, 239, 239, 239,
    };
  static unsigned char lengthtable[] =
    {
      0,  0,  0,  0,  0,  0,  6,  0,  0,  0,  0,  6, 12,  8,
      0, 10,  0,  0,  0, 19,  0,  0,  0,  0,  0, 10,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0, 11,  0,  0,  0,  0,  0,
      0,  0, 19,  0,  0,  0,  3,  4,  0, 11, 17,  0,  0,  0,
      0,  7, 18,  0,  0,  0, 12,  0, 14,  0, 11, 17,  0, 19,
      0,  0,  0,  0,  0,  5, 21, 17,  0,  0,  0, 21,  7, 18,
      0,  0, 16,  0,  0,  0,  5,  0, 12,  0,  4,  0,  0, 12,
      0, 19,  0, 16, 17, 13,  9, 10, 16,  0,  0,  0,  0,  0,
      0,  8, 19,  5, 11,  0,  0,  4, 10,  0,  0,  0, 14, 10,
      6,  0,  0,  9,  0, 16, 17,  0,  0, 10, 11,  0, 18,  0,
      0,  6, 12,  0,  0,  0, 16,  7,  0,  0, 10, 21,  0,  0,
      0,  0, 11, 12,  0,  0, 10,  0,  0,  8,  0,  5,  0,  0,
      0,  0, 15,  0, 12,  0,  0,  0, 16,  0,  0,  0,  0,  0,
     12, 13,  9,  0, 11,  0,  0,  0,  0,  6,  0,  0, 14,  0,
      0,  0,  0,  0, 15,  0,  0,  0,  0,  0,  0,  0,  0, 14,
      0, 11,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0, 12,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,
     13,
    };
  static struct StaticNameTable wordlist[] =
    {
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"border",  9},
      {"",}, {"",}, {"",}, {"",}, 
      {"filter",  40},
      {"border-color",  14},
      {"position",  67},
      {"",}, 
      {"border-top",  24},
      {"",}, {"",}, {"",}, 
      {"border-bottom-color",  11},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"visibility",  74},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"font-family",  43},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"border-bottom-style",  12},
      {"",}, {"",}, {"",}, 
      {"top",  72},
      {"font",  42},
      {"",}, 
      {"border-left",  15},
      {"border-left-color",  16},
      {"",}, {"",}, {"",}, {"",}, 
      {"padding",  62},
      {"border-right-color",  20},
      {"",}, {"",}, {"",}, 
      {"border-style",  23},
      {"",}, 
      {"letter-spacing",  50},
      {"",}, 
      {"padding-top",  66},
      {"background-filter",  3},
      {"",}, 
      {"background-position",  5},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"float",  41},
      {"background-x-position",  6},
      {"border-left-style",  17},
      {"",}, {"",}, {"",}, 
      {"background-y-position",  7},
      {"z-index",  78},
      {"border-right-style",  21},
      {"",}, {"",}, 
      {"background-image",  4},
      {"",}, {"",}, {"",}, 
      {"color",  35},
      {"",}, 
      {"border-right",  19},
      {"",}, 
      {"left",  49},
      {"",}, {"",}, 
      {"font-variant",  46},
      {"",}, 
      {"border-bottom-width",  13},
      {"",}, 
      {"border-top-color",  25},
      {"background-repeat",  8},
      {"border-bottom",  10},
      {"font-size",  44},
      {"font-style",  45},
      {"border-top-style",  26},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"overflow",  61},
      {"list-style-position",  54},
      {"clear",  29},
      {"text-indent",  70},
      {"",}, {"",}, 
      {"clip",  30},
      {"text-align",  68},
      {"",}, {"",}, {"",}, 
      {"vertical-align",  73},
      {"list-style",  52},
      {"cursor",  36},
      {"",}, {"",}, 
      {"direction",  38},
      {"",}, 
      {"list-style-image",  53},
      {"border-left-width",  18},
      {"",}, {"",}, 
      {"background",  0},
      {"font-weight",  47},
      {"",}, 
      {"border-right-width",  22},
      {"",}, {"",}, 
      {"margin",  56},
      {"padding-left",  64},
      {"",}, {"",}, {"",}, 
      {"background-color",  2},
      {"display",  39},
      {"",}, {"",}, 
      {"margin-top",  60},
      {"background-attachment",  1},
      {"",}, {"",}, {"",}, {"",}, 
      {"line-height",  51},
      {"word-spacing",  77},
      {"",}, {"",}, 
      {"clip-right",  33},
      {"",}, {"",}, 
      {"clip-top",  34},
      {"",}, 
      {"width",  76},
      {"",}, {"",}, {"",}, {"",}, 
      {"list-style-type",  55},
      {"",}, 
      {"border-width",  28},
      {"",}, {"",}, {"",}, 
      {"border-top-width",  27},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"cursor-image",  37},
      {"padding-right",  65},
      {"clip-left",  32},
      {"",}, 
      {"margin-left",  58},
      {"",}, {"",}, {"",}, {"",}, 
      {"height",  48},
      {"",}, {"",}, 
      {"padding-bottom",  63},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"text-decoration",  69},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"text-transform",  71},
      {"",}, 
      {"clip-bottom",  31},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"margin-right",  59},
      {"",}, {"",}, {"",}, 
      {"white-space",  75},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"margin-bottom",  57},
    };

  if (str != NULL) {
    int len = PL_strlen(str);
    if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH) {
  register int hval = len;

  switch (hval)
    {
      default:
      case 12:
        hval += asso_values[MYLOWER(str[11])];
      case 11:
      case 10:
      case 9:
      case 8:
      case 7:
      case 6:
        hval += asso_values[MYLOWER(str[5])];
      case 5:
      case 4:
      case 3:
      case 2:
        hval += asso_values[MYLOWER(str[1])];
      case 1:
        hval += asso_values[MYLOWER(str[0])];
        break;
    }
  hval += asso_values[MYLOWER(str[len - 1])];
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

const nsCSSProps::NameTableEntry nsCSSProps::kNameTable[] = {
  { "background", 0 }, 
  { "background-attachment", 1 }, 
  { "background-color", 2 }, 
  { "background-filter", 3 }, 
  { "background-image", 4 }, 
  { "background-position", 5 }, 
  { "background-x-position", 6 }, 
  { "background-y-position", 7 }, 
  { "background-repeat", 8 }, 
  { "border", 9 }, 
  { "border-bottom", 10 }, 
  { "border-bottom-color", 11 }, 
  { "border-bottom-style", 12 }, 
  { "border-bottom-width", 13 }, 
  { "border-color", 14 }, 
  { "border-left", 15 }, 
  { "border-left-color", 16 }, 
  { "border-left-style", 17 }, 
  { "border-left-width", 18 }, 
  { "border-right", 19 }, 
  { "border-right-color", 20 }, 
  { "border-right-style", 21 }, 
  { "border-right-width", 22 }, 
  { "border-style", 23 }, 
  { "border-top", 24 }, 
  { "border-top-color", 25 }, 
  { "border-top-style", 26 }, 
  { "border-top-width", 27 }, 
  { "border-width", 28 }, 
  { "clear", 29 }, 
  { "clip", 30 }, 
  { "clip-bottom", 31 }, 
  { "clip-left", 32 }, 
  { "clip-right", 33 }, 
  { "clip-top", 34 }, 
  { "color", 35 }, 
  { "cursor", 36 }, 
  { "cursor-image", 37 }, 
  { "direction", 38 }, 
  { "display", 39 }, 
  { "filter", 40 }, 
  { "float", 41 }, 
  { "font", 42 }, 
  { "font-family", 43 }, 
  { "font-size", 44 }, 
  { "font-style", 45 }, 
  { "font-variant", 46 }, 
  { "font-weight", 47 }, 
  { "height", 48 }, 
  { "left", 49 }, 
  { "letter-spacing", 50 }, 
  { "line-height", 51 }, 
  { "list-style", 52 }, 
  { "list-style-image", 53 }, 
  { "list-style-position", 54 }, 
  { "list-style-type", 55 }, 
  { "margin", 56 }, 
  { "margin-bottom", 57 }, 
  { "margin-left", 58 }, 
  { "margin-right", 59 }, 
  { "margin-top", 60 }, 
  { "overflow", 61 }, 
  { "padding", 62 }, 
  { "padding-bottom", 63 }, 
  { "padding-left", 64 }, 
  { "padding-right", 65 }, 
  { "padding-top", 66 }, 
  { "position", 67 }, 
  { "text-align", 68 }, 
  { "text-decoration", 69 }, 
  { "text-indent", 70 }, 
  { "text-transform", 71 }, 
  { "top", 72 }, 
  { "vertical-align", 73 }, 
  { "visibility", 74 }, 
  { "white-space", 75 }, 
  { "width", 76 }, 
  { "word-spacing", 77 }, 
  { "z-index", 78 }, 
};
