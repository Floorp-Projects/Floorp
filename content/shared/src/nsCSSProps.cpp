
/*
** This is a generated file, do not edit it. This file is created by
** genhash.pl
*/

#include "plstr.h"
#include "nsCSSProps.h"
#include "nsCSSKeywordIDs.h"
#include "nsStyleConsts.h"
#include "nsCSSKeywords.h"

#define TOTAL_KEYWORDS 80
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 21
#define MIN_HASH_VALUE 6
#define MAX_HASH_VALUE 261
/* maximum key range = 256, duplicates = 0 */


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
  static unsigned short asso_values[] =
    {
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262,  65, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262, 262, 262, 262,
     262, 262, 262, 262, 262, 262, 262,  45,   0,  85,
     100,  25,   5,   0,  80,   0, 262, 262,  25,  90,
       0,   0,   5, 262,   0,  65,  40,  20,   0,  80,
      10,  25,   5, 262, 262, 262, 262, 262,
    };
  static unsigned char lengthtable[] =
    {
      0,  0,  0,  0,  0,  0,  6,  0,  0,  0,  0,  6, 12,  8,
      0, 10,  0,  0,  0, 19,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0, 10,  0,  0,  0,  0,  0,  0,
      0,  0, 19,  0, 11,  0,  3,  4,  0, 11,  0,  0,  0,  0,
      0,  7, 18,  0,  0,  0, 12,  0, 14,  0, 11, 17,  0, 19,
      0,  0,  0,  0,  0,  5, 21,  7,  0,  0,  0,  0, 17, 18,
      0,  0, 16,  0,  0,  0,  5, 21, 12,  0,  4,  0,  0, 12,
      0, 19,  0, 16, 17, 13,  9, 10, 16, 17,  0,  0,  0,  6,
      7,  8, 19,  5, 11,  0,  0,  4, 10,  0,  0,  0, 14, 10,
      0,  0,  0,  0,  0, 16,  0,  0,  0,  0, 11,  0, 18,  0,
      0,  6, 12,  0,  0,  0, 16,  0,  0,  9, 10, 21,  0,  0,
      0, 10, 11, 12,  0,  0, 10,  0, 17,  8,  0,  5,  0, 12,
      0,  0, 15,  0, 12,  0,  0,  0, 16,  7,  0,  0,  0,  0,
      0, 13,  9,  0, 11,  0,  0,  0,  0,  6,  0,  0, 14,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 14,
      0, 11,  0,  0,  0,  0,  0,  0,  0,  0, 15,  0,  0,  0,
      0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     13,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0, 11,
    };
  static struct StaticNameTable wordlist[] =
    {
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"border",  9},
      {"",}, {"",}, {"",}, {"",}, 
      {"filter",  40},
      {"border-color",  14},
      {"position",  68},
      {"",}, 
      {"border-top",  24},
      {"",}, {"",}, {"",}, 
      {"border-bottom-color",  11},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"visibility",  75},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"border-bottom-style",  12},
      {"",}, 
      {"font-family",  43},
      {"",}, 
      {"top",  73},
      {"font",  42},
      {"",}, 
      {"border-left",  15},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"padding",  63},
      {"border-right-color",  20},
      {"",}, {"",}, {"",}, 
      {"border-style",  23},
      {"",}, 
      {"letter-spacing",  50},
      {"",}, 
      {"padding-top",  67},
      {"background-filter",  3},
      {"",}, 
      {"background-position",  5},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"float",  41},
      {"background-x-position",  6},
      {"opacity",  61},
      {"",}, {"",}, {"",}, {"",}, 
      {"border-left-color",  16},
      {"border-right-style",  21},
      {"",}, {"",}, 
      {"background-image",  4},
      {"",}, {"",}, {"",}, 
      {"color",  35},
      {"background-y-position",  7},
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
      {"border-left-style",  17},
      {"",}, {"",}, {"",}, 
      {"cursor",  36},
      {"z-index",  79},
      {"overflow",  62},
      {"list-style-position",  54},
      {"clear",  29},
      {"text-indent",  71},
      {"",}, {"",}, 
      {"clip",  30},
      {"text-align",  69},
      {"",}, {"",}, {"",}, 
      {"vertical-align",  74},
      {"list-style",  52},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"list-style-image",  53},
      {"",}, {"",}, {"",}, {"",}, 
      {"font-weight",  47},
      {"",}, 
      {"border-right-width",  22},
      {"",}, {"",}, 
      {"margin",  56},
      {"padding-left",  65},
      {"",}, {"",}, {"",}, 
      {"background-color",  2},
      {"",}, {"",}, 
      {"direction",  38},
      {"margin-top",  60},
      {"background-attachment",  1},
      {"",}, {"",}, {"",}, 
      {"background",  0},
      {"line-height",  51},
      {"word-spacing",  78},
      {"",}, {"",}, 
      {"clip-right",  33},
      {"",}, 
      {"border-left-width",  18},
      {"clip-top",  34},
      {"",}, 
      {"width",  77},
      {"",}, 
      {"cursor-image",  37},
      {"",}, {"",}, 
      {"list-style-type",  55},
      {"",}, 
      {"border-width",  28},
      {"",}, {"",}, {"",}, 
      {"border-top-width",  27},
      {"display",  39},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"padding-right",  66},
      {"clip-left",  32},
      {"",}, 
      {"margin-left",  58},
      {"",}, {"",}, {"",}, {"",}, 
      {"height",  48},
      {"",}, {"",}, 
      {"padding-bottom",  64},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"text-transform",  72},
      {"",}, 
      {"clip-bottom",  31},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"text-decoration",  70},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"margin-right",  59},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, 
      {"margin-bottom",  57},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, 
      {"white-space",  76},
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
  { "opacity", 61 }, 
  { "overflow", 62 }, 
  { "padding", 63 }, 
  { "padding-bottom", 64 }, 
  { "padding-left", 65 }, 
  { "padding-right", 66 }, 
  { "padding-top", 67 }, 
  { "position", 68 }, 
  { "text-align", 69 }, 
  { "text-decoration", 70 }, 
  { "text-indent", 71 }, 
  { "text-transform", 72 }, 
  { "top", 73 }, 
  { "vertical-align", 74 }, 
  { "visibility", 75 }, 
  { "white-space", 76 }, 
  { "width", 77 }, 
  { "word-spacing", 78 }, 
  { "z-index", 79 }, 
};




// Keyword id tables for variant/enum parsing
static PRInt32 kBackgroundAttachmentKTable[] = {
  KEYWORD_FIXED, NS_STYLE_BG_ATTACHMENT_FIXED,
  KEYWORD_SCROLL, NS_STYLE_BG_ATTACHMENT_SCROLL,
  -1,-1
};

static PRInt32 kBackgroundColorKTable[] = {
  KEYWORD_TRANSPARENT, NS_STYLE_BG_COLOR_TRANSPARENT,
  -1,-1
};

static PRInt32 kBackgroundRepeatKTable[] = {
  KEYWORD_NO_REPEAT, NS_STYLE_BG_REPEAT_OFF,
  KEYWORD_REPEAT, NS_STYLE_BG_REPEAT_XY,
  KEYWORD_REPEAT_X, NS_STYLE_BG_REPEAT_X,
  KEYWORD_REPEAT_Y, NS_STYLE_BG_REPEAT_Y,
  -1,-1
};

static PRInt32 kBorderStyleKTable[] = {
  KEYWORD_NONE,   NS_STYLE_BORDER_STYLE_NONE,
  KEYWORD_DOTTED, NS_STYLE_BORDER_STYLE_DOTTED,
  KEYWORD_DASHED, NS_STYLE_BORDER_STYLE_DASHED,
  KEYWORD_SOLID,  NS_STYLE_BORDER_STYLE_SOLID,
  KEYWORD_DOUBLE, NS_STYLE_BORDER_STYLE_DOUBLE,
  KEYWORD_GROOVE, NS_STYLE_BORDER_STYLE_GROOVE,
  KEYWORD_RIDGE,  NS_STYLE_BORDER_STYLE_RIDGE,
  KEYWORD_INSET,  NS_STYLE_BORDER_STYLE_INSET,
  KEYWORD_OUTSET, NS_STYLE_BORDER_STYLE_OUTSET,
  -1,-1
};

static PRInt32 kBorderWidthKTable[] = {
  KEYWORD_THIN, NS_STYLE_BORDER_WIDTH_THIN,
  KEYWORD_MEDIUM, NS_STYLE_BORDER_WIDTH_MEDIUM,
  KEYWORD_THICK, NS_STYLE_BORDER_WIDTH_THICK,
  -1,-1
};

static PRInt32 kClearKTable[] = {
  KEYWORD_NONE, NS_STYLE_CLEAR_NONE,
  KEYWORD_LEFT, NS_STYLE_CLEAR_LEFT,
  KEYWORD_RIGHT, NS_STYLE_CLEAR_RIGHT,
  KEYWORD_BOTH, NS_STYLE_CLEAR_LEFT_AND_RIGHT,
  -1,-1
};

static PRInt32 kCursorKTable[] = {
  KEYWORD_IBEAM, NS_STYLE_CURSOR_IBEAM,
  KEYWORD_ARROW, NS_STYLE_CURSOR_DEFAULT,
  KEYWORD_HAND, NS_STYLE_CURSOR_HAND,
  -1,-1
};

static PRInt32 kDirectionKTable[] = {
  KEYWORD_LTR,      NS_STYLE_DIRECTION_LTR,
  KEYWORD_RTL,      NS_STYLE_DIRECTION_RTL,
  KEYWORD_INHERIT,  NS_STYLE_DIRECTION_INHERIT,
  -1,-1
};

static PRInt32 kDisplayKTable[] = {
  KEYWORD_NONE,               NS_STYLE_DISPLAY_NONE,
  KEYWORD_BLOCK,              NS_STYLE_DISPLAY_BLOCK,
  KEYWORD_INLINE,             NS_STYLE_DISPLAY_INLINE,
  KEYWORD_LIST_ITEM,          NS_STYLE_DISPLAY_LIST_ITEM,
  KEYWORD_MARKER,             NS_STYLE_DISPLAY_MARKER,
  KEYWORD_RUN_IN,             NS_STYLE_DISPLAY_RUN_IN,
  KEYWORD_COMPACT,            NS_STYLE_DISPLAY_COMPACT,
  KEYWORD_TABLE,              NS_STYLE_DISPLAY_TABLE,
  KEYWORD_INLINE_TABLE,       NS_STYLE_DISPLAY_INLINE_TABLE,
  KEYWORD_TABLE_ROW_GROUP,    NS_STYLE_DISPLAY_TABLE_ROW_GROUP,
  KEYWORD_TABLE_COLUMN,       NS_STYLE_DISPLAY_TABLE_COLUMN,
  KEYWORD_TABLE_COLUMN_GROUP, NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP,
  KEYWORD_TABLE_HEADER_GROUP, NS_STYLE_DISPLAY_TABLE_HEADER_GROUP,
  KEYWORD_TABLE_FOOTER_GROUP, NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP,
  KEYWORD_TABLE_ROW,          NS_STYLE_DISPLAY_TABLE_ROW,
  KEYWORD_TABLE_CELL,         NS_STYLE_DISPLAY_TABLE_CELL,
  KEYWORD_TABLE_CAPTION,      NS_STYLE_DISPLAY_TABLE_CAPTION,
  -1,-1
};

static PRInt32 kFloatKTable[] = {
  KEYWORD_NONE,  NS_STYLE_FLOAT_NONE,
  KEYWORD_LEFT,  NS_STYLE_FLOAT_LEFT,
  KEYWORD_RIGHT, NS_STYLE_FLOAT_RIGHT,
  -1,-1
};

static PRInt32 kFontSizeKTable[] = {
  KEYWORD_XX_SMALL, NS_STYLE_FONT_SIZE_XXSMALL,
  KEYWORD_X_SMALL, NS_STYLE_FONT_SIZE_XSMALL,
  KEYWORD_SMALL, NS_STYLE_FONT_SIZE_SMALL,
  KEYWORD_MEDIUM, NS_STYLE_FONT_SIZE_MEDIUM,
  KEYWORD_LARGE, NS_STYLE_FONT_SIZE_LARGE,
  KEYWORD_X_LARGE, NS_STYLE_FONT_SIZE_XLARGE,
  KEYWORD_XX_LARGE, NS_STYLE_FONT_SIZE_XXLARGE,
  KEYWORD_LARGER, NS_STYLE_FONT_SIZE_LARGER,
  KEYWORD_SMALLER, NS_STYLE_FONT_SIZE_SMALLER,
  -1,-1
};

static PRInt32 kFontStyleKTable[] = {
  KEYWORD_NORMAL, NS_STYLE_FONT_STYLE_NORMAL,
  KEYWORD_ITALIC, NS_STYLE_FONT_STYLE_ITALIC,
  KEYWORD_OBLIQUE, NS_STYLE_FONT_STYLE_OBLIQUE,
  -1,-1
};

static PRInt32 kFontVariantKTable[] = {
  KEYWORD_NORMAL, NS_STYLE_FONT_VARIANT_NORMAL,
  KEYWORD_SMALL_CAPS, NS_STYLE_FONT_VARIANT_SMALL_CAPS,
  -1,-1
};

static PRInt32 kListStyleImageKTable[] = {
  KEYWORD_NONE, NS_STYLE_LIST_STYLE_IMAGE_NONE,
  -1,-1
};

static PRInt32 kListStylePositionKTable[] = {
  KEYWORD_INSIDE, NS_STYLE_LIST_STYLE_POSITION_INSIDE,
  KEYWORD_OUTSIDE, NS_STYLE_LIST_STYLE_POSITION_OUTSIDE,
  -1,-1
};

static PRInt32 kListStyleKTable[] = {
  KEYWORD_NONE, NS_STYLE_LIST_STYLE_NONE,
  KEYWORD_DISC, NS_STYLE_LIST_STYLE_DISC,
  KEYWORD_CIRCLE, NS_STYLE_LIST_STYLE_CIRCLE,
  KEYWORD_SQUARE, NS_STYLE_LIST_STYLE_SQUARE,
  KEYWORD_DECIMAL, NS_STYLE_LIST_STYLE_DECIMAL,
  KEYWORD_LOWER_ROMAN, NS_STYLE_LIST_STYLE_LOWER_ROMAN,
  KEYWORD_UPPER_ROMAN, NS_STYLE_LIST_STYLE_UPPER_ROMAN,
  KEYWORD_LOWER_ALPHA, NS_STYLE_LIST_STYLE_LOWER_ALPHA,
  KEYWORD_UPPER_ALPHA, NS_STYLE_LIST_STYLE_UPPER_ALPHA,
  -1,-1
};

static PRInt32 kMarginSizeKTable[] = {
  KEYWORD_AUTO, NS_STYLE_MARGIN_SIZE_AUTO,
  -1,-1
};

static PRInt32 kOverflowKTable[] = {
  KEYWORD_VISIBLE, NS_STYLE_OVERFLOW_VISIBLE,
  KEYWORD_HIDDEN, NS_STYLE_OVERFLOW_HIDDEN,
  KEYWORD_SCROLL, NS_STYLE_OVERFLOW_SCROLL,
  KEYWORD_AUTO, NS_STYLE_OVERFLOW_AUTO,
  -1,-1
};

static PRInt32 kPositionKTable[] = {
  KEYWORD_RELATIVE, NS_STYLE_POSITION_RELATIVE,
  KEYWORD_ABSOLUTE, NS_STYLE_POSITION_ABSOLUTE,
  KEYWORD_FIXED, NS_STYLE_POSITION_FIXED,
  -1,-1
};

static PRInt32 kTextAlignKTable[] = {
  KEYWORD_LEFT, NS_STYLE_TEXT_ALIGN_LEFT,
  KEYWORD_RIGHT, NS_STYLE_TEXT_ALIGN_RIGHT,
  KEYWORD_CENTER, NS_STYLE_TEXT_ALIGN_CENTER,
  KEYWORD_JUSTIFY, NS_STYLE_TEXT_ALIGN_JUSTIFY,
  -1,-1
};

static PRInt32 kTextTransformKTable[] = {
  KEYWORD_NONE, NS_STYLE_TEXT_TRANSFORM_NONE,
  KEYWORD_CAPITALIZE, NS_STYLE_TEXT_TRANSFORM_CAPITALIZE,
  KEYWORD_LOWERCASE, NS_STYLE_TEXT_TRANSFORM_LOWERCASE,
  KEYWORD_UPPERCASE, NS_STYLE_TEXT_TRANSFORM_UPPERCASE,
  -1,-1
};

static PRInt32 kVerticalAlignKTable[] = {
  KEYWORD_BASELINE, NS_STYLE_VERTICAL_ALIGN_BASELINE,
  KEYWORD_SUB, NS_STYLE_VERTICAL_ALIGN_SUB,
  KEYWORD_SUPER, NS_STYLE_VERTICAL_ALIGN_SUPER,
  KEYWORD_TOP, NS_STYLE_VERTICAL_ALIGN_TOP,
  KEYWORD_TEXT_TOP, NS_STYLE_VERTICAL_ALIGN_TEXT_TOP,
  KEYWORD_MIDDLE, NS_STYLE_VERTICAL_ALIGN_MIDDLE,
  KEYWORD_BOTTOM, NS_STYLE_VERTICAL_ALIGN_BOTTOM,
  KEYWORD_TEXT_BOTTOM, NS_STYLE_VERTICAL_ALIGN_TEXT_BOTTOM,
  -1,-1
};

static PRInt32 kVisibilityKTable[] = {
  KEYWORD_VISIBLE, NS_STYLE_VISIBILITY_VISIBLE,
  KEYWORD_HIDDEN, NS_STYLE_VISIBILITY_HIDDEN,
  -1,-1
};

static PRInt32 kWhitespaceKTable[] = {
  KEYWORD_NORMAL, NS_STYLE_WHITESPACE_NORMAL,
  KEYWORD_PRE, NS_STYLE_WHITESPACE_PRE,
  KEYWORD_NOWRAP, NS_STYLE_WHITESPACE_NOWRAP,
  -1,-1
};

static const char* kBorderTopNames[] = {
  "border-top-width",
  "border-top-style",
  "border-top-color",
};
static const char* kBorderRightNames[] = {
  "border-right-width",
  "border-right-style",
  "border-right-color",
};
static const char* kBorderBottomNames[] = {
  "border-bottom-width",
  "border-bottom-style",
  "border-bottom-color",
};
static const char* kBorderLeftNames[] = {
  "border-left-width",
  "border-left-style",
  "border-left-color",
};

static const char* SearchKeywordTable(PRInt32 aID, PRInt32 aTable[])
{
  PRInt32 i = 1;
  if (aID >= 0)
  {
    for (;;) {
      if (aTable[i] < 0) {
        break;
      }
      if (aID == aTable[i]) {
        return nsCSSKeywords::kNameTable[aTable[i-1]].name;
      }
      i += 2;
    }
  }
  return nsnull;
}



const char* nsCSSProps::LookupProperty(PRInt32 aProp, PRInt32 aIndex)
{
  switch (aProp)  {
  
  case PROP_BACKGROUND:
    break;
  
  case PROP_BACKGROUND_ATTACHMENT:
    return SearchKeywordTable(aIndex,kBackgroundAttachmentKTable);

  case PROP_BACKGROUND_COLOR:
    return SearchKeywordTable(aIndex,kBackgroundColorKTable);
  
  case PROP_BACKGROUND_FILTER:
    break;
    
  case PROP_BACKGROUND_IMAGE:
    break;

  case PROP_BACKGROUND_POSITION:
    break;

  case PROP_BACKGROUND_REPEAT:
    return SearchKeywordTable(aIndex,kBackgroundRepeatKTable);

  case PROP_BORDER:
  case PROP_BORDER_COLOR:
  case PROP_BORDER_STYLE:
  case PROP_BORDER_BOTTOM:
  case PROP_BORDER_LEFT:
  case PROP_BORDER_RIGHT:
  case PROP_BORDER_TOP:
  case PROP_BORDER_BOTTOM_COLOR:
  case PROP_BORDER_LEFT_COLOR:
  case PROP_BORDER_RIGHT_COLOR:
  case PROP_BORDER_TOP_COLOR:
    break;

  case PROP_BORDER_BOTTOM_STYLE:
  case PROP_BORDER_LEFT_STYLE:
  case PROP_BORDER_RIGHT_STYLE:
  case PROP_BORDER_TOP_STYLE:
    return SearchKeywordTable(aIndex,kBorderStyleKTable);
  break;

  case PROP_BORDER_BOTTOM_WIDTH:
  case PROP_BORDER_LEFT_WIDTH:
  case PROP_BORDER_RIGHT_WIDTH:
  case PROP_BORDER_TOP_WIDTH:
    return SearchKeywordTable(aIndex,kBorderWidthKTable);
    break;
  
  case PROP_BORDER_WIDTH:
  case PROP_CLEAR:
    return SearchKeywordTable(aIndex,kClearKTable);  
  break;
    
  case PROP_CLIP:
  case PROP_COLOR:
    break;

  case PROP_CURSOR:
    return SearchKeywordTable(aIndex,kCursorKTable);  
    break;

  case PROP_DIRECTION:
    return SearchKeywordTable(aIndex,kDirectionKTable);  
  
  case PROP_DISPLAY:
    return SearchKeywordTable(aIndex,kDisplayKTable);  

  case PROP_FILTER:
    break;
  
  case PROP_FLOAT:
    return SearchKeywordTable(aIndex,kFloatKTable);  

  case PROP_FONT:
  case PROP_FONT_FAMILY:
    break;

  case PROP_FONT_SIZE:
      return SearchKeywordTable(aIndex,kFontSizeKTable);  
    break;

  case PROP_FONT_STYLE:
    return SearchKeywordTable(aIndex,kFontStyleKTable);  

  case PROP_FONT_VARIANT:
    return SearchKeywordTable(aIndex,kFontVariantKTable);  
  
  
  case PROP_FONT_WEIGHT:
  case PROP_HEIGHT:
  case PROP_LEFT:
  case PROP_LINE_HEIGHT:
  case PROP_LIST_STYLE:
    break;

  case PROP_LIST_STYLE_IMAGE:
    return SearchKeywordTable(aIndex, kListStyleImageKTable);  
  
  case PROP_LIST_STYLE_POSITION:
    return SearchKeywordTable(aIndex, kListStylePositionKTable);  
  
  
  case PROP_LIST_STYLE_TYPE:
    return SearchKeywordTable(aIndex, kListStyleKTable);

  case PROP_MARGIN:
  case PROP_MARGIN_BOTTOM:
  case PROP_MARGIN_LEFT:
  case PROP_MARGIN_RIGHT:
  case PROP_MARGIN_TOP:
    return SearchKeywordTable(aIndex, kMarginSizeKTable);
  break;

  case PROP_PADDING:
  case PROP_PADDING_BOTTOM:
  case PROP_PADDING_LEFT:
  case PROP_PADDING_RIGHT:
  case PROP_PADDING_TOP:
    break;
  
  case PROP_OPACITY:
    break;
  
  case PROP_OVERFLOW:
    return SearchKeywordTable(aIndex, kOverflowKTable);
  
  case PROP_POSITION:
    return SearchKeywordTable(aIndex, kPositionKTable);
    break;
  
  case PROP_TEXT_ALIGN:
    return SearchKeywordTable(aIndex, kTextAlignKTable);
  
  case PROP_TEXT_DECORATION:
  case PROP_TEXT_INDENT:
    break;
  
  case PROP_TEXT_TRANSFORM:
    return SearchKeywordTable(aIndex, kTextTransformKTable);
  
  case PROP_TOP:
    break;

  case PROP_VERTICAL_ALIGN:
    return SearchKeywordTable(aIndex, kVerticalAlignKTable);
  break;
  
  case PROP_VISIBILITY:
    return SearchKeywordTable(aIndex, kVisibilityKTable);
    
  case PROP_WHITE_SPACE:
    return SearchKeywordTable(aIndex, kWhitespaceKTable);

  case PROP_WIDTH:
  case PROP_LETTER_SPACING:
  case PROP_WORD_SPACING:
  case PROP_Z_INDEX:
    break;
  }
  return nsnull;
}
