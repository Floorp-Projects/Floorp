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

/*  A Bison parser, made from css.y
 by  GNU Bison version 1.25
  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse css_parse
#define yylex css_lex
#define yyerror css_error
#define yylval css_lval
#define yychar css_char
#define yydebug css_debug
#define yynerrs css_nerrs
#define	NUMBER	258
#define	STRING	259
#define	PERCENTAGE	260
#define	LENGTH	261
#define	EMS	262
#define	EXS	263
#define	IDENT	264
#define	HEXCOLOR	265
#define	URL	266
#define	RGB	267
#define	CDO	268
#define	CDC	269
#define	IMPORTANT_SYM	270
#define	IMPORT_SYM	271
#define	DOT_AFTER_IDENT	272
#define	DOT	273
#define	LINK_PSCLASS	274
#define	VISITED_PSCLASS	275
#define	ACTIVE_PSCLASS	276
#define	LEADING_LINK_PSCLASS	277
#define	LEADING_VISITED_PSCLASS	278
#define	LEADING_ACTIVE_PSCLASS	279
#define	FIRST_LINE	280
#define	FIRST_LETTER	281
#define	WILD	282
#define	BACKGROUND	283
#define	BG_COLOR	284
#define	BG_IMAGE	285
#define	BG_REPEAT	286
#define	BG_ATTACHMENT	287
#define	BG_POSITION	288
#define	FONT	289
#define	FONT_STYLE	290
#define	FONT_VARIANT	291
#define	FONT_WEIGHT	292
#define	FONT_SIZE	293
#define	FONT_NORMAL	294
#define	LINE_HEIGHT	295
#define	LIST_STYLE	296
#define	LS_TYPE	297
#define	LS_NONE	298
#define	LS_POSITION	299
#define	BORDER	300
#define	BORDER_STYLE	301
#define	BORDER_WIDTH	302
#define	FONT_SIZE_PROPERTY	303
#define	FONTDEF	304



#include <stdio.h>
#include "cssI.h"


typedef union {
  css_node binary_node;
} YYSTYPE;


                 /* Background Shorthand Property */

typedef struct backgroundShorthandRecord {
    css_node color;
    css_node image;
    css_node repeat;
    css_node attachment;
    css_node position;
    int    parse_error;
} BackgroundShorthandRecord, *BackgroundShorthand;

#define BackgroundColor      1
#define BackgroundImage      2
#define BackgroundRepeat     3
#define BackgroundAttachment 4
#define BackgroundPosition   5

static void ClearBackground(void);
static void AddBackground(int node_type, css_node node);
static css_node AssembleBackground(void);

static BackgroundShorthandRecord bg;

/* background property names */
static char * bg_image = "background-image";
static char * bg_color = "background-color";
static char * bg_repeat = "background-repeat";
static char * bg_attachment = "background-attachment";
static char * bg_position = "background-position";
/* background property initial values */
static char * css_none = "none";
static char * css_transparent = "transparent";
static char * css_repeat = "repeat";
static char * css_scroll = "scroll";
static char * css_origin = "0% 0%";


                    /* Font Shorthand Property */

typedef struct fontShorthandRecord {
    css_node style;
    css_node variant;
    css_node weight;
    css_node size;
    css_node leading;
    css_node family;
    int normal_count;
    int parse_error;
} FontShorthandRecord, *FontShorthand;

#define FontStyle    6
#define FontVariant  7
#define FontWeight   8
#define FontNormal   9
#define FontSize    10
#define FontLeading 11
#define FontFamily  12

static void ClearFont(void);
static void AddFont(int node_type, css_node node);
static css_node AssembleFont(void);

static FontShorthandRecord font;

/* font property names */
static char * line_height = "line-height";
static char * font_family = "font-family";
static char * font_style = "font-style";
static char * font_variant = "font-variant";
static char * font_weight = "font-weight";
static char * font_size = "font-size";
/* font property initial values */
static char * css_normal = "normal";


                 /* List-Style Shorthand Property */

typedef struct listStyleShorthandRecord {
    css_node marker;
    css_node image;
    css_node position;
    int none_count;
    int parse_error;
} ListStyleShorthandRecord, *ListStyleShorthand;

#define ListStyleMarker		13
#define ListStyleImage		14
#define ListStylePosition	15
#define ListStyleNone		16

static void ClearListStyle(void);
static void AddListStyle(int node_type, css_node node);
static css_node AssembleListStyle(void);

static ListStyleShorthandRecord ls;

/* list-style property names */
static char * list_style_type = "list-style-type";
static char * list_style_image = "list-style-image";
static char * list_style_position = "list-style-position";
/* list-style initial values */
static char * css_disc = "disc";
static char * css_outside = "outside";


                   /* Border Shorthand Property */

typedef struct borderShorthandRecord {
    css_node width;
    css_node style;
    css_node color;
    int parse_error;
} BorderShorthandRecord, *BorderShorthand;

#define BorderWidth	17
#define BorderStyle	18
#define BorderColor	19

static void ClearBorder(void);
static void AddBorder(int node_type, css_node node);
static css_node AssembleBorder(void);

static BorderShorthandRecord border;

/* border property names */
static char * border_width = "border-width";
static char * border_style = "border-style";
static char * border_color = "border-color";
/* border initial values */
static char * css_medium = "medium";


/* Define yyoverflow and the forward declarations for bison. */
#define yyoverflow css_overflow
static void css_overflow(const char *message, short **yyss1, int yyss1_size, 
                         YYSTYPE **yyvs1, int yyvs1_size, int *yystacksize);
static css_node NewNode(int node_id, char *ss, css_node left, css_node right);
static void LeftAppendNode(css_node head, css_node new_node);
static css_node NewDeclarationNode(int node_id, char *ss, char *prop);
static css_node NewComponentNode(css_node value, char *prop);

css_node css_tree_root;

/* pseudo-classes */
static char * css_link = "link";
static char * css_visited = "visited";
static char * css_active = "active";

#ifdef CSS_PARSE_DEBUG
#define TRACE1(str) trace1(str)
#define TRACE2(format, str) trace2(format, str)
static trace1(const char * str)
{
    printf("%s", str);
}

static trace2(const char *fmt, char *str)
{
    printf(fmt, str);
}
#else
#define TRACE1(str)
#define TRACE2(format, str)
#endif

int css_error(const char * diagnostic)
{
#ifdef CSS_PARSE_REPORT_ERRORS
    char * identifier = "CSS1 parser message:";
    (void) fprintf(stderr,
                   "%s error, text ='%s', diagnostic ='%s'\n",
                   identifier, css_text, diagnostic ? diagnostic : "");
#endif
    return 1;
}


int css_wrap(void)
{
#ifdef CSS_PARSE_DEBUG
    printf("css_wrap() was called.\n");
#endif
    return 1;
}

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		173
#define	YYFLAG		-32768
#define	YYNTBASE	59

#define YYTRANSLATE(x) ((unsigned)(x) <= 304 ? yytranslate[x] : 115)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,    57,     2,     2,     2,     2,     2,
     2,     2,    52,    54,    51,     2,    53,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,    58,    50,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,    55,     2,    56,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     2,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    46,    47,    48,    49
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     7,    10,    13,    16,    18,    20,    24,
    28,    30,    32,    34,    36,    37,    39,    41,    43,    45,
    47,    49,    51,    53,    58,    60,    64,    66,    69,    71,
    74,    77,    79,    82,    84,    87,    89,    93,    96,   101,
   105,   107,   109,   111,   113,   116,   118,   120,   122,   124,
   126,   128,   130,   132,   134,   138,   139,   144,   149,   154,
   159,   164,   169,   170,   172,   174,   176,   180,   182,   184,
   186,   188,   190,   192,   195,   197,   199,   202,   204,   206,
   208,   210,   212,   214,   216,   218,   220,   222,   224,   227,
   230,   234,   236,   239,   241,   243,   245,   247,   249,   251,
   253,   255,   257,   260,   262,   265,   267,   271,   277,   278,
   280,   282,   284,   288,   289,   292,   294,   296,   298,   300,
   302,   304,   307,   309,   311,   313,   315,   318,   320,   322,
   324,   326,   328,   331,   333,   335,   337,   339
};

static const short yyrhs[] = {    -1,
    59,    13,     0,    59,    60,     0,    59,    72,     0,    59,
    14,     0,    59,     1,     0,    61,     0,    62,     0,    16,
    63,    50,     0,    49,    63,    50,     0,     4,     0,    95,
     0,    51,     0,    52,     0,     0,    53,     0,    54,     0,
     9,     0,    48,     0,    28,     0,    34,     0,    41,     0,
    45,     0,    73,    55,    84,    56,     0,    74,     0,    73,
    54,    74,     0,    77,     0,    77,    83,     0,    75,     0,
    75,    83,     0,    76,    77,     0,    77,     0,    76,    77,
     0,    78,     0,    18,    79,     0,    80,     0,    78,    17,
    79,     0,    78,    81,     0,    78,    17,    79,    81,     0,
    18,    79,    81,     0,    82,     0,    27,     0,     9,     0,
     9,     0,    57,     9,     0,    19,     0,    20,     0,    21,
     0,    22,     0,    23,     0,    24,     0,    25,     0,    26,
     0,    85,     0,    84,    50,    85,     0,     0,    66,    58,
    88,    86,     0,    67,    58,   109,    86,     0,    68,    58,
    97,    86,     0,    69,    58,   103,    86,     0,    70,    58,
   111,    86,     0,    71,    58,   113,    86,     0,     0,    87,
     0,    15,     0,    89,     0,    88,    65,    89,     0,    94,
     0,    96,     0,    95,     0,    92,     0,    90,     0,    91,
     0,    64,    91,     0,     3,     0,    93,     0,    64,    93,
     0,     5,     0,     6,     0,     7,     0,     8,     0,     4,
     0,     9,     0,    11,     0,    10,     0,    12,     0,    98,
     0,   100,     0,    98,   100,     0,   100,    98,     0,    98,
   100,    98,     0,    99,     0,    98,    99,     0,    11,     0,
    30,     0,    96,     0,     9,     0,    29,     0,    31,     0,
    32,     0,   101,     0,    92,     0,    92,    92,     0,   102,
     0,   102,   102,     0,    33,     0,   107,   109,   105,     0,
   107,   109,    53,   110,   105,     0,     0,    54,     0,   106,
     0,    94,     0,   106,   104,    94,     0,     0,   107,   108,
     0,    35,     0,    36,     0,    37,     0,    39,     0,    93,
     0,    38,     0,    52,    93,     0,    92,     0,    90,     0,
    40,     0,   112,     0,   111,   112,     0,    42,     0,    43,
     0,    44,     0,    95,     0,   114,     0,   113,   114,     0,
    46,     0,    47,     0,    92,     0,    96,     0,     9,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   268,   271,   275,   284,   296,   300,   307,   310,   316,   322,
   328,   331,   339,   343,   350,   354,   358,   365,   372,   378,
   387,   396,   404,   412,   419,   423,   437,   441,   445,   449,
   461,   468,   472,   479,   483,   487,   491,   495,   499,   506,
   510,   517,   523,   530,   537,   544,   547,   550,   556,   559,
   562,   568,   572,   579,   583,   595,   599,   605,   611,   618,
   625,   628,   634,   635,   639,   646,   650,   665,   666,   667,
   668,   669,   673,   674,   682,   688,   689,   696,   699,   702,
   705,   711,   714,   721,   727,   730,   736,   737,   738,   739,
   740,   744,   745,   749,   753,   757,   761,   765,   769,   773,
   780,   790,   793,   802,   805,   817,   823,   830,   842,   846,
   853,   862,   866,   881,   884,   890,   895,   900,   905,   911,
   912,   915,   922,   923,   924,   934,   935,   939,   944,   948,
   953,   961,   962,   966,   971,   976,   981,   986
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","NUMBER",
"STRING","PERCENTAGE","LENGTH","EMS","EXS","IDENT","HEXCOLOR","URL","RGB","CDO",
"CDC","IMPORTANT_SYM","IMPORT_SYM","DOT_AFTER_IDENT","DOT","LINK_PSCLASS","VISITED_PSCLASS",
"ACTIVE_PSCLASS","LEADING_LINK_PSCLASS","LEADING_VISITED_PSCLASS","LEADING_ACTIVE_PSCLASS",
"FIRST_LINE","FIRST_LETTER","WILD","BACKGROUND","BG_COLOR","BG_IMAGE","BG_REPEAT",
"BG_ATTACHMENT","BG_POSITION","FONT","FONT_STYLE","FONT_VARIANT","FONT_WEIGHT",
"FONT_SIZE","FONT_NORMAL","LINE_HEIGHT","LIST_STYLE","LS_TYPE","LS_NONE","LS_POSITION",
"BORDER","BORDER_STYLE","BORDER_WIDTH","FONT_SIZE_PROPERTY","FONTDEF","';'",
"'-'","'+'","'/'","','","'{'","'}'","'#'","':'","stylesheet","atrule","import",
"fontdef","import_value","unary_operator","operator","property","font_size_property",
"background_property","font_property","list_style_property","border_property",
"ruleset","selector_list","selector","contextual_selector","contextual_selector_list",
"simple_selector","element_name","class","id","pseudo_class","leading_pseudo_class",
"pseudo_element","declaration_list","declaration","optional_priority","prio",
"expr","term","numeric_const","unsigned_numeric_const","numeric_unit","unsigned_numeric_unit",
"unsigned_symbol","url","color_code","background_values_list","background_values",
"background_value","background_position_value","background_position_expr","background_position_keyword",
"font_values_list","font_family_operator","font_family_value","font_family_expr",
"font_optional_values_list","font_optional_value","font_size_value","line_height_value",
"list_style_values_list","list_style_value","border_values_list","border_value", NULL
};
#endif

static const short yyr1[] = {     0,
    59,    59,    59,    59,    59,    59,    60,    60,    61,    62,
    63,    63,    64,    64,    65,    65,    65,    66,    67,    68,
    69,    70,    71,    72,    73,    73,    74,    74,    74,    74,
    75,    76,    76,    77,    77,    77,    77,    77,    77,    77,
    77,    77,    78,    79,    80,    81,    81,    81,    82,    82,
    82,    83,    83,    84,    84,    85,    85,    85,    85,    85,
    85,    85,    86,    86,    87,    88,    88,    89,    89,    89,
    89,    89,    90,    90,    91,    92,    92,    93,    93,    93,
    93,    94,    94,    95,    96,    96,    97,    97,    97,    97,
    97,    98,    98,    99,    99,    99,    99,    99,    99,    99,
   100,   101,   101,   101,   101,   102,   103,   103,   104,   104,
   105,   106,   106,   107,   107,   108,   108,   108,   108,   109,
   109,   109,   110,   110,   110,   111,   111,   112,   112,   112,
   112,   113,   113,   114,   114,   114,   114,   114
};

static const short yyr2[] = {     0,
     0,     2,     2,     2,     2,     2,     1,     1,     3,     3,
     1,     1,     1,     1,     0,     1,     1,     1,     1,     1,
     1,     1,     1,     4,     1,     3,     1,     2,     1,     2,
     2,     1,     2,     1,     2,     1,     3,     2,     4,     3,
     1,     1,     1,     1,     2,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     3,     0,     4,     4,     4,     4,
     4,     4,     0,     1,     1,     1,     3,     1,     1,     1,
     1,     1,     1,     2,     1,     1,     2,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     2,     2,
     3,     1,     2,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     2,     1,     2,     1,     3,     5,     0,     1,
     1,     1,     3,     0,     2,     1,     1,     1,     1,     1,
     1,     2,     1,     1,     1,     1,     2,     1,     1,     1,
     1,     1,     2,     1,     1,     1,     1,     1
};

static const short yydefact[] = {     1,
     0,     6,    43,     2,     5,     0,     0,    49,    50,    51,
    42,     0,     0,     3,     7,     8,     4,     0,    25,    29,
     0,    32,    34,    36,    41,    11,    84,     0,    12,    44,
    35,     0,    45,     0,    56,    52,    53,    30,    33,    28,
     0,    46,    47,    48,    38,     9,    40,    10,    26,    18,
    20,    21,    22,    23,    19,     0,     0,     0,     0,     0,
     0,     0,    54,    37,     0,     0,     0,   114,     0,     0,
    56,    24,    39,    75,    82,    78,    79,    80,    81,    83,
    85,    86,    13,    14,     0,    15,    66,    72,    73,    71,
    76,    68,    70,    69,   121,     0,   120,    63,    97,    94,
    98,    95,    99,   100,   106,     0,   102,    96,    63,    87,
    92,    88,   101,   104,    63,     0,   128,   129,   130,   131,
    63,   126,   138,   134,   135,   136,   137,    63,   132,    55,
    74,    77,    65,    16,    17,     0,    57,    64,   122,    58,
   103,    59,    93,    89,    90,   105,    60,   116,   117,   118,
   119,   115,     0,    61,   127,    62,   133,    67,    91,     0,
   112,   107,   111,   125,   124,   123,     0,   110,     0,   108,
   113,     0,     0
};

static const short yydefgoto[] = {     1,
    14,    15,    16,    28,   106,   136,    56,    57,    58,    59,
    60,    61,    17,    18,    19,    20,    21,    22,    23,    31,
    24,    45,    25,    38,    62,    63,   137,   138,    86,    87,
    88,    89,    90,    91,    92,    29,   108,   109,   110,   111,
   112,   113,   114,   115,   169,   162,   163,   116,   152,    98,
   167,   121,   122,   128,   129
};

static const short yypact[] = {-32768,
    10,-32768,-32768,-32768,-32768,    11,     0,-32768,-32768,-32768,
-32768,    11,    18,-32768,-32768,-32768,-32768,   -37,-32768,    38,
    78,   -12,   150,-32768,-32768,-32768,-32768,   -14,-32768,-32768,
   155,   -10,-32768,    78,   170,-32768,-32768,-32768,   180,-32768,
     0,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,    -2,    14,    27,    30,    31,
    35,   -25,-32768,   155,    72,   121,   101,-32768,   103,   131,
   170,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,   223,   134,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   231,-32768,    53,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   231,    40,-32768,    53,   101,
-32768,   213,-32768,    36,    53,   158,-32768,-32768,-32768,-32768,
   157,-32768,-32768,-32768,-32768,-32768,-32768,   110,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,    72,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,   213,   213,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,    12,-32768,-32768,-32768,-32768,-32768,   213,    46,
-32768,-32768,     3,-32768,-32768,-32768,    26,-32768,    26,-32768,
-32768,    99,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,-32768,    91,   -65,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,    87,-32768,-32768,   127,-32768,   109,
-32768,   -26,-32768,   120,-32768,    80,    93,-32768,-32768,    19,
    -6,    95,   -66,   -46,    -9,   -63,   -62,-32768,   -83,   -55,
    71,-32768,    75,-32768,-32768,    25,-32768,-32768,-32768,    69,
-32768,-32768,    65,-32768,    79
};


#define	YYLAST		245


static const short yytable[] = {    85,
   107,    93,    94,   126,    47,   120,  -109,   127,    30,   172,
     2,  -109,    36,    37,    26,    75,    34,    35,     3,    97,
    80,    27,     4,     5,    71,     6,    33,     7,   145,    75,
    72,     8,     9,    10,    80,    46,    11,    73,   132,    48,
   141,   -27,   -27,   107,    76,    77,    78,    79,    74,   139,
    76,    77,    78,    79,   143,    65,   168,   120,    12,   132,
   159,   126,    36,    37,   160,   127,    13,   133,   105,    97,
    85,    66,    93,    94,    74,    75,    76,    77,    78,    79,
    80,    81,    27,    82,    67,   164,     3,    68,    69,   143,
    83,    84,    70,   166,    85,     7,    83,    84,   173,     8,
     9,    10,    32,   143,    11,    76,    77,    78,    79,    99,
    81,   100,    82,    27,    76,    77,    78,    79,   123,    81,
    49,    82,    83,    84,   133,    76,    77,    78,    79,   101,
   102,   103,   104,   105,    13,    76,    77,    78,    79,   123,
    81,    40,    82,   161,   117,   118,   119,    39,   133,    64,
   130,    83,    84,   165,   158,   124,   125,   161,    95,   171,
    83,    84,    76,    77,    78,    79,    41,    27,    42,    43,
    44,   133,    96,    42,    43,    44,   124,   125,    50,   131,
   144,    83,    84,   -63,   153,   155,   134,   135,   146,   -63,
   140,   170,   148,   149,   150,    95,   151,    51,   117,   118,
   119,   142,     0,    52,   -31,   -31,   157,   147,     0,    96,
    53,     0,     0,   154,    54,     0,     0,    55,     0,     0,
   156,    99,    81,   100,    82,    74,     0,    76,    77,    78,
    79,     0,     0,   -31,   -31,    76,    77,    78,    79,     0,
     0,   101,   102,   103,   104
};

static const short yycheck[] = {    65,
    67,    65,    65,    70,    31,    69,     4,    70,     9,     0,
     1,     9,    25,    26,     4,     4,    54,    55,     9,    66,
     9,    11,    13,    14,    50,    16,     9,    18,   112,     4,
    56,    22,    23,    24,     9,    50,    27,    64,    85,    50,
   107,    54,    55,   110,     5,     6,     7,     8,     3,    96,
     5,     6,     7,     8,   110,    58,    54,   121,    49,   106,
   144,   128,    25,    26,    53,   128,    57,    15,    33,   116,
   136,    58,   136,   136,     3,     4,     5,     6,     7,     8,
     9,    10,    11,    12,    58,    40,     9,    58,    58,   145,
    51,    52,    58,   160,   160,    18,    51,    52,     0,    22,
    23,    24,    12,   159,    27,     5,     6,     7,     8,     9,
    10,    11,    12,    11,     5,     6,     7,     8,     9,    10,
    34,    12,    51,    52,    15,     5,     6,     7,     8,    29,
    30,    31,    32,    33,    57,     5,     6,     7,     8,     9,
    10,    22,    12,   153,    42,    43,    44,    21,    15,    41,
    71,    51,    52,   160,   136,    46,    47,   167,    38,   169,
    51,    52,     5,     6,     7,     8,    17,    11,    19,    20,
    21,    15,    52,    19,    20,    21,    46,    47,     9,    85,
   110,    51,    52,    50,   116,   121,    53,    54,   114,    56,
    98,   167,    35,    36,    37,    38,    39,    28,    42,    43,
    44,   109,    -1,    34,    25,    26,   128,   115,    -1,    52,
    41,    -1,    -1,   121,    45,    -1,    -1,    48,    -1,    -1,
   128,     9,    10,    11,    12,     3,    -1,     5,     6,     7,
     8,    -1,    -1,    54,    55,     5,     6,     7,     8,    -1,
    -1,    29,    30,    31,    32
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */


/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

#ifndef alloca
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi)
#include <alloca.h>
#else /* not sparc */
#if defined (MSDOS) && !defined (__TURBOC__)
#include <malloc.h>
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
#include <malloc.h>
 #pragma alloca
#else /* not MSDOS, __TURBOC__, or _AIX */
#ifdef __hpux
#ifdef __cplusplus
extern "C" {
void *alloca (unsigned int);
};
#else /* not __cplusplus */
void *alloca ();
#endif /* not __cplusplus */
#endif /* __hpux */
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc.  */
#endif /* not GNU C.  */
#endif /* alloca not defined.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	return(0)
#define YYABORT 	return(1)
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
int yyparse (void);
#endif

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, int count)
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif



/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
      yyss = (short *) alloca (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1, size * sizeof (*yyssp));
      yyvs = (YYSTYPE *) alloca (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1, size * sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) alloca (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1, size * sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
{
        yyval.binary_node = NULL;
        ;
    break;}
case 2:
{
        TRACE1("-cdo\n");
        yyval.binary_node = yyvsp[-1].binary_node;
        ;
    break;}
case 3:
{
        yyval.binary_node = yyvsp[-1].binary_node;
        if (yyval.binary_node == NULL) {
            css_tree_root = yyvsp[0].binary_node;
            yyval.binary_node = css_tree_root;
        }
        else
            LeftAppendNode(yyval.binary_node, yyvsp[0].binary_node);
        ;
    break;}
case 4:
{
        css_node tmp;
        tmp = NewNode(NODE_RULESET_LIST, NULL, NULL, yyvsp[0].binary_node);
        
        yyval.binary_node = yyvsp[-1].binary_node;
        if (yyval.binary_node == NULL) {
            css_tree_root = tmp;
            yyval.binary_node = css_tree_root;
        }
        else
            LeftAppendNode(yyval.binary_node, tmp);
        ;
    break;}
case 5:
{
        TRACE1("-cdc\n");
        yyval.binary_node = yyvsp[-1].binary_node;
        ;
    break;}
case 6:
{
        yyerrok;
        yyclearin;
        ;
    break;}
case 7:
{
        yyval.binary_node = NewNode(NODE_IMPORT_LIST, NULL, NULL, yyvsp[0].binary_node);
        ;
    break;}
case 8:
{
        yyval.binary_node = NewNode(NODE_FONTDEF_LIST, NULL, NULL, yyvsp[0].binary_node);
        ;
    break;}
case 9:
{ 
     yyval.binary_node = yyvsp[-1].binary_node;
   ;
    break;}
case 10:
{ 
        yyval.binary_node = yyvsp[-1].binary_node;
        ;
    break;}
case 11:
{
    yyval.binary_node = NewNode(NODE_IMPORT_STRING, css_text, NULL, NULL);
   ;
    break;}
case 12:
{
     TRACE1("-import url\n");
     yyvsp[0].binary_node->node_id = NODE_IMPORT_URL;
     yyval.binary_node = yyvsp[0].binary_node;
   ;
    break;}
case 13:
{ 
     TRACE1("-unary_op '-'\n");
     yyval.binary_node = NewNode(NODE_UNARY_OP, css_text, NULL, NULL);
   ;
    break;}
case 14:
{
     TRACE1("-unary_op '+'\n");
     yyval.binary_node = NewNode(NODE_UNARY_OP, css_text, NULL, NULL);
   ;
    break;}
case 15:
{ 
     TRACE1("-empty operator\n");
     yyval.binary_node = NewNode(NODE_EMPTY_OP, NULL, NULL, NULL);
   ;
    break;}
case 16:
{ 
     TRACE1("-operator : '/'\n"); 
     yyval.binary_node = NewNode(NODE_EXPR_OP, css_text, NULL, NULL);
   ;
    break;}
case 17:
{ 
     TRACE1("-operator : ','\n"); 
     yyval.binary_node = NewNode(NODE_EXPR_OP, css_text, NULL, NULL);
   ;
    break;}
case 18:
{
     TRACE2("-property : IDENT '%s'\n", css_text); 
     yyval.binary_node = NewNode(NODE_PROPERTY, css_text, NULL, NULL);
   ;
    break;}
case 19:
{
     yyval.binary_node = NewNode(NODE_PROPERTY, css_text, NULL, NULL);
   ;
    break;}
case 20:
{
     TRACE2("-property : BACKGROUND '%s'\n", css_text);
     ClearBackground();
     yyval.binary_node = NULL;
     /* This shorthand property will generate five component properties. */
   ;
    break;}
case 21:
{
     TRACE2("-property : FONT '%s'\n", css_text);
     ClearFont();
     yyval.binary_node = NULL;
     /* This shorthand property will generate six component properties. */
   ;
    break;}
case 22:
{
     ClearListStyle();
     yyval.binary_node = NULL;
     /* This shorthand property will generate three component properties. */
   ;
    break;}
case 23:
{
     ClearBorder();
     yyval.binary_node = NULL;
     /* This shorthand property will generate 2 or 3 component properties. */
   ;
    break;}
case 24:
{ 
     TRACE1("-selector_list { declaration_list }\n"); 
     yyval.binary_node = NewNode(NODE_RULESET, NULL, yyvsp[-3].binary_node, yyvsp[-1].binary_node);
   ;
    break;}
case 25:
{ 
     TRACE1("-selector\n"); 
     yyval.binary_node = NewNode(NODE_SELECTOR_LIST, NULL, NULL, yyvsp[0].binary_node);
   ;
    break;}
case 26:
{ 
     css_node tmp;
     TRACE1("-selector_list , selector\n"); 
     tmp = NewNode(NODE_SELECTOR_LIST, NULL, NULL, yyvsp[0].binary_node);
     
     yyval.binary_node = yyvsp[-2].binary_node;
     if( yyval.binary_node == NULL )
       yyval.binary_node = tmp;
     else
       LeftAppendNode( yyval.binary_node, tmp );
   ;
    break;}
case 27:
{
    TRACE1("-simple_selector\n");
    yyval.binary_node = NewNode(NODE_SELECTOR, NULL, yyvsp[0].binary_node, NULL);
;
    break;}
case 28:
{
    TRACE1("-simple_selector : pseudo_element\n");
    yyval.binary_node = NewNode(NODE_SELECTOR, NULL, yyvsp[-1].binary_node, yyvsp[0].binary_node);
;
    break;}
case 29:
{
    TRACE1("-contextual_selector\n");
    yyval.binary_node = NewNode(NODE_SELECTOR, NULL, yyvsp[0].binary_node, NULL);
;
    break;}
case 30:
{
    TRACE1("-contextual_selector : pseudo_element\n");
     yyval.binary_node = NewNode(NODE_SELECTOR, NULL, yyvsp[-1].binary_node, yyvsp[0].binary_node);
;
    break;}
case 31:
{
    TRACE1("-contextual_selector_list simple_selector\n");
    yyval.binary_node = NewNode(NODE_SELECTOR_CONTEXTUAL, NULL, yyvsp[-1].binary_node, yyvsp[0].binary_node);
;
    break;}
case 32:
{
     TRACE1("-simple_selector\n");
     yyval.binary_node = NewNode(NODE_SELECTOR_CONTEXTUAL, NULL, NULL, yyvsp[0].binary_node);
   ;
    break;}
case 33:
{
     TRACE1("-contextual_selector_list simple_selector\n");
     yyval.binary_node = NewNode(NODE_SELECTOR_CONTEXTUAL, NULL, yyvsp[-1].binary_node, yyvsp[0].binary_node);
   ;
    break;}
case 34:
{ 
     TRACE1("-element_name\n");
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_NAME_ONLY, NULL, yyvsp[0].binary_node, NULL);
   ;
    break;}
case 35:
{
     TRACE1("-dot class\n");
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_DOT_AND_CLASS, NULL, yyvsp[0].binary_node, NULL);
   ;
    break;}
case 36:
{
     TRACE1("-id\n");
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_ID_SELECTOR, NULL, yyvsp[0].binary_node, NULL);
   ;
    break;}
case 37:
{
     TRACE1("-element dot class\n");
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_NAME_AND_CLASS, NULL, yyvsp[-2].binary_node, yyvsp[0].binary_node);
   ;
    break;}
case 38:
{
     TRACE1("-element pseudo_class\n");
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_NAME_PSEUDO_CLASS, NULL, yyvsp[-1].binary_node, yyvsp[0].binary_node);
   ;
    break;}
case 39:
{
     TRACE2("-element_name '%s' dot class pseudo_class\n", yyvsp[-3].binary_node->string);
     yyvsp[-3].binary_node->node_id = NODE_SIMPLE_SELECTOR_NAME_CLASS_PSEUDO_CLASS;
     yyvsp[-3].binary_node->left = yyvsp[-1].binary_node;
     yyvsp[-3].binary_node->right = yyvsp[0].binary_node;
     yyval.binary_node = yyvsp[-3].binary_node;
   ;
    break;}
case 40:
{
     TRACE1("-dot class pseudo_class\n");
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_NAME_CLASS_PSEUDO_CLASS, "A", yyvsp[-1].binary_node, yyvsp[0].binary_node);
   ;
    break;}
case 41:
{
     css_node tmp;
     TRACE1("solitary pseudo_class\n");
     /* See CSS1 spec of 17 December 1996 section 2.1 Anchor pseudo-classes */
     tmp = NewNode(NODE_ELEMENT_NAME, "A", NULL, NULL);
     yyval.binary_node = NewNode(NODE_SIMPLE_SELECTOR_NAME_PSEUDO_CLASS, NULL, tmp, yyvsp[0].binary_node);
   ;
    break;}
case 42:
{
     yyval.binary_node = NewNode(NODE_WILD, NULL, NULL, NULL);
   ;
    break;}
case 43:
{ 
     TRACE2("-IDENT '%s' to element_name\n", css_text); 
     yyval.binary_node = NewNode(NODE_ELEMENT_NAME, css_text, NULL, NULL);
   ;
    break;}
case 44:
{
        TRACE2("-IDENT '%s' to class\n", css_text);
        yyval.binary_node = NewNode(NODE_CLASS, css_text, NULL, NULL);
      ;
    break;}
case 45:
{
        TRACE2("-IDENT '%s' to id\n", css_text);
        yyval.binary_node = NewNode(NODE_ID_SELECTOR, css_text, NULL, NULL);
      ;
    break;}
case 46:
{
     yyval.binary_node = NewNode(NODE_LINK_PSCLASS, css_link, NULL, NULL);
   ;
    break;}
case 47:
{
     yyval.binary_node = NewNode(NODE_VISITED_PSCLASS, css_visited, NULL, NULL);
   ;
    break;}
case 48:
{
     yyval.binary_node = NewNode(NODE_ACTIVE_PSCLASS, css_active, NULL, NULL);
   ;
    break;}
case 49:
{
     yyval.binary_node = NewNode(NODE_LINK_PSCLASS, css_link, NULL, NULL);
   ;
    break;}
case 50:
{
     yyval.binary_node = NewNode(NODE_VISITED_PSCLASS, css_visited, NULL, NULL);
   ;
    break;}
case 51:
{
     yyval.binary_node = NewNode(NODE_ACTIVE_PSCLASS, css_active, NULL, NULL);
   ;
    break;}
case 52:
{ 
     TRACE2("-IDENT '%s' to pseudo_element\n", css_text);
     yyval.binary_node = NewNode(NODE_PSEUDO_ELEMENT, "first-Line", NULL, NULL);
   ;
    break;}
case 53:
{ 
     TRACE2("-IDENT '%s' to pseudo_element\n", css_text);
     yyval.binary_node = NewNode(NODE_PSEUDO_ELEMENT, "first-Letter", NULL, NULL);
   ;
    break;}
case 54:
{
     TRACE1("-declaration\n");
     yyval.binary_node = yyvsp[0].binary_node;
   ;
    break;}
case 55:
{
     /* to keep the order, append the new node to the end. */
     TRACE1("-declaration_list ';' declaration\n");
     yyval.binary_node = yyvsp[-2].binary_node;
     if (NULL == yyval.binary_node)
       yyval.binary_node = yyvsp[0].binary_node;
     else
       LeftAppendNode(yyval.binary_node, yyvsp[0].binary_node);
   ;
    break;}
case 56:
{ 
     TRACE1("-empty declaration\n");
     yyval.binary_node = NewNode(NODE_DECLARATION_LIST, NULL, NULL, NULL);
   ;
    break;}
case 57:
{
     css_node dcl;
     TRACE1("-property : expr\n");
     dcl = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, yyvsp[-3].binary_node, yyvsp[-1].binary_node);
     yyval.binary_node = NewNode(NODE_DECLARATION_LIST, NULL, NULL, dcl);
   ;
    break;}
case 58:
{
     css_node expr, dcl;
     expr = NewNode(NODE_EXPR, NULL, NULL, yyvsp[-1].binary_node);
     dcl = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, yyvsp[-3].binary_node, expr);
     yyval.binary_node = NewNode(NODE_DECLARATION_LIST, NULL, NULL, dcl);
    ;
    break;}
case 59:
{
     /* The priority notation is ignored by the translator
      * so we conveniently ignore it here, too.
      */
     TRACE1("-background property : background_values_list\n");
     yyval.binary_node = AssembleBackground();
   ;
    break;}
case 60:
{
     TRACE1("-font property : font_values_list prio\n");
     /* The priority notation is ignored by the translator
      * so we conveniently ignore it here, too.
      */
     yyval.binary_node = AssembleFont();
   ;
    break;}
case 61:
{
     yyval.binary_node = AssembleListStyle();
   ;
    break;}
case 62:
{
     yyval.binary_node = AssembleBorder();
    ;
    break;}
case 63:
{ /* nothing */ ;
    break;}
case 65:
{                       /* !important */
     TRACE1("-IMPORTANT_SYM\n");
     yyval.binary_node = NULL;
   ;
    break;}
case 66:
{
     TRACE1("-term\n");
     yyval.binary_node = NewNode(NODE_EXPR, NULL, NULL, yyvsp[0].binary_node);
   ;
    break;}
case 67:
{
     TRACE1("-expr op term\n");

     yyval.binary_node = yyvsp[-2].binary_node;
     /* put the new term at the end */
     yyvsp[-1].binary_node->right = yyvsp[0].binary_node;

     if (yyval.binary_node == NULL)
       yyval.binary_node = yyvsp[-1].binary_node;
     else
       LeftAppendNode( yyval.binary_node, yyvsp[-1].binary_node ); 
   ;
    break;}
case 74:
{
     TRACE1("-unary_operator signed_const to term\n"); 
     yyval.binary_node = yyvsp[-1].binary_node;
     yyval.binary_node->left = yyvsp[0].binary_node;
   ;
    break;}
case 75:
{
     yyval.binary_node = NewNode(NODE_NUMBER, css_text, NULL, NULL);
   ;
    break;}
case 77:
{
     yyval.binary_node = yyvsp[-1].binary_node;
     yyval.binary_node->left = yyvsp[0].binary_node;
   ;
    break;}
case 78:
{
     yyval.binary_node = NewNode(NODE_PERCENTAGE, css_text, NULL, NULL);
   ;
    break;}
case 79:
{
     yyval.binary_node = NewNode(NODE_LENGTH, css_text, NULL, NULL);
   ;
    break;}
case 80:
{
     yyval.binary_node = NewNode(NODE_EMS, css_text, NULL, NULL);
   ;
    break;}
case 81:
{
     yyval.binary_node = NewNode(NODE_EMS, css_text, NULL, NULL);
   ;
    break;}
case 82:
{
     yyval.binary_node = NewNode(NODE_STRING, css_text, NULL, NULL);
   ;
    break;}
case 83:
{ 
     TRACE2("-IDENT '%s' to unsigned_symbol\n", css_text);
     yyval.binary_node = NewNode(NODE_IDENT, css_text, NULL, NULL);
   ;
    break;}
case 84:
{
     yyval.binary_node = NewNode(NODE_URL, css_text, NULL, NULL);
   ;
    break;}
case 85:
{
     yyval.binary_node = NewNode(NODE_HEXCOLOR, css_text, NULL, NULL);
   ;
    break;}
case 86:
{
     yyval.binary_node = NewNode(NODE_RGB, css_text, NULL, NULL);
   ;
    break;}
case 94:
{
        yyval.binary_node = NewDeclarationNode(NODE_URL, css_text, bg_image);
        AddBackground(BackgroundImage, yyval.binary_node);
      ;
    break;}
case 95:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, bg_image);
        AddBackground(BackgroundImage, yyval.binary_node);
      ;
    break;}
case 96:
{
        yyval.binary_node = NewComponentNode(yyvsp[0].binary_node, bg_color);
        AddBackground(BackgroundColor, yyval.binary_node);
      ;
    break;}
case 97:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, bg_color);
        AddBackground(BackgroundColor, yyval.binary_node);
      ;
    break;}
case 98:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, bg_color);
        AddBackground(BackgroundColor, yyval.binary_node);
      ;
    break;}
case 99:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, "background-repeat");
        AddBackground(BackgroundRepeat, yyval.binary_node);
      ;
    break;}
case 100:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, "background-attachment");
        AddBackground(BackgroundAttachment, yyval.binary_node);
      ;
    break;}
case 101:
{
        css_node property, declaration;
        property = NewNode(NODE_PROPERTY, bg_position, NULL, NULL);
        declaration = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL,
                              property, yyvsp[0].binary_node);
        AddBackground(BackgroundPosition, declaration);
      ;
    break;}
case 102:
{
        yyval.binary_node = NewNode(NODE_EXPR, NULL, NULL, yyvsp[0].binary_node);
      ;
    break;}
case 103:
{
        css_node operator;
        yyval.binary_node = NewNode(NODE_EXPR, NULL, NULL, yyvsp[-1].binary_node);
        operator = NewNode(NODE_EMPTY_OP, NULL, NULL, yyvsp[0].binary_node);
        if (yyval.binary_node == NULL)
            yyval.binary_node = operator;
        else
            LeftAppendNode(yyval.binary_node, operator);
      ;
    break;}
case 104:
{
        yyval.binary_node = NewNode(NODE_EXPR, NULL, NULL, yyvsp[0].binary_node);
      ;
    break;}
case 105:
{
        css_node operator;
        yyval.binary_node = yyvsp[-1].binary_node;
        operator = NewNode(NODE_EMPTY_OP, NULL, NULL, yyvsp[0].binary_node);
        if (yyval.binary_node == NULL)
            yyval.binary_node = operator;
        else
            LeftAppendNode(yyval.binary_node, operator);
      ;
    break;}
case 106:
{
        yyval.binary_node = NewNode(NODE_IDENT, css_text, NULL, NULL);
      ;
    break;}
case 107:
{
        css_node tmp;
        tmp = NewComponentNode(yyvsp[-1].binary_node, font_size);
        AddFont(FontSize, tmp);
        AddFont(FontFamily, yyvsp[0].binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 108:
{
        css_node tmp;
        tmp = NewComponentNode(yyvsp[-3].binary_node, font_size);
        AddFont(FontSize, tmp);
        tmp = NewComponentNode(yyvsp[-1].binary_node, line_height);
        AddFont(FontLeading, tmp);
        AddFont(FontFamily, yyvsp[0].binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 109:
{ 
     TRACE1("-empty operator\n");
     yyval.binary_node = NewNode(NODE_EMPTY_OP, NULL, NULL, NULL);
   ;
    break;}
case 110:
{ 
     TRACE1("-operator : ','\n"); 
     yyval.binary_node = NewNode(NODE_EXPR_OP, css_text, NULL, NULL);
   ;
    break;}
case 111:
{
     css_node property;
     TRACE1("-font_family_expr to font_family_value\n");
     property = NewNode(NODE_PROPERTY, font_family, NULL, NULL);
     yyval.binary_node = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, property, yyvsp[0].binary_node);
    ;
    break;}
case 112:
{
     TRACE1("-unsigned_symbol\n");
     yyval.binary_node = NewNode(NODE_EXPR, NULL, NULL, yyvsp[0].binary_node);
   ;
    break;}
case 113:
{
     TRACE1("-font_family_value font_family_op unsigned_symbol\n");

     yyval.binary_node = yyvsp[-2].binary_node;
     /* put the new term at the end */
     yyvsp[-1].binary_node->right = yyvsp[0].binary_node;

     if (yyval.binary_node == NULL)
       yyval.binary_node = yyvsp[-1].binary_node;
     else
       LeftAppendNode( yyval.binary_node, yyvsp[-1].binary_node ); 
   ;
    break;}
case 114:
{ /* empty */
        yyval.binary_node = NULL;
      ;
    break;}
case 115:
{
        yyval.binary_node = NULL;
      ;
    break;}
case 116:
{
	TRACE2("-FONT_STYLE '%s' to font_optional_value\n", css_text);
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, font_style);
        AddFont(FontStyle, yyval.binary_node);
      ;
    break;}
case 117:
{
	TRACE2("-FONT_VARIANT '%s' to font_optional_value\n", css_text);
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, font_variant);
        AddFont(FontVariant, yyval.binary_node);
      ;
    break;}
case 118:
{
	TRACE2("-FONT_WEIGHT '%s' to font_optional_value\n", css_text);
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, font_weight);
        AddFont(FontWeight, yyval.binary_node);
      ;
    break;}
case 119:
{
        AddFont(FontNormal, NULL);
      ;
    break;}
case 121:
{
        yyval.binary_node = NewNode(NODE_IDENT, css_text, NULL, NULL);
      ;
    break;}
case 122:
{
        /* just drop the '+' on the floor */
        yyval.binary_node = yyvsp[0].binary_node;
      ;
    break;}
case 125:
{
        /* The only valid identifier is the word "normal".
         * There's an idea that a normal leading value is font-specific,
         * so the identifier is passed upwards.
         */
	yyval.binary_node = NewNode(NODE_IDENT, css_text, NULL, NULL);
      ;
    break;}
case 128:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, list_style_type);
        AddListStyle(ListStyleMarker, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 129:
{
        AddListStyle(ListStyleNone, NULL);
        yyval.binary_node = NULL;
      ;
    break;}
case 130:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, list_style_position);
        AddListStyle(ListStylePosition, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 131:
{
        yyval.binary_node = NewComponentNode(yyvsp[0].binary_node, list_style_image);
        AddListStyle(ListStyleImage, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 134:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, border_style);
        AddBorder(BorderStyle, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 135:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, border_width);
        AddBorder(BorderWidth, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 136:
{
        yyval.binary_node = NewComponentNode(yyvsp[0].binary_node, border_width);
        AddBorder(BorderWidth, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 137:
{
        yyval.binary_node = NewComponentNode(yyvsp[0].binary_node, border_color);
        AddBorder(BorderColor, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
case 138:
{
        yyval.binary_node = NewDeclarationNode(NODE_IDENT, css_text, border_color);
        AddBorder(BorderColor, yyval.binary_node);
        yyval.binary_node = NULL;
      ;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */


  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;
}


#include "xp_mem.h"
#include "xpassert.h"

/* Memory allocated here will be freed by css_FreeNode in csstojs.c */
static css_node NewNode(int node_id, char *ss, css_node left, css_node right)
{
    register css_node pp;

    if ((pp = XP_NEW(css_nodeRecord)) == NULL)
        return NULL;

    pp->node_id = node_id;
    pp->string = NULL;
    if (ss) {
        if ((pp->string = (char *) XP_ALLOC(strlen(ss) + 1)) != NULL)
            (void) strcpy(pp->string, ss);
    }
    pp->left   = left;
    pp->right  = right;
    return pp;
}


/* Append new_node to the leftmost leaf of head */
static void LeftAppendNode(css_node head, css_node new_node)
{
    if (head == NULL)
        return;
    
    while (head->left != NULL)
        head = head->left;
    head->left = new_node;         
}


static css_node NewDeclarationNode(int node_id, char *ss, char *prop)
{
    css_node value, expression, property;
    value = NewNode(node_id, ss, NULL, NULL);
    expression = NewNode(NODE_EXPR, NULL, NULL, value);
    property = NewNode(NODE_PROPERTY, prop, NULL, NULL);
    return NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, property, expression);
}


static css_node NewComponentNode(css_node value, char *prop)
{
    css_node expression, property;
    expression = NewNode(NODE_EXPR, NULL, NULL, value);
    property = NewNode(NODE_PROPERTY, prop, NULL, NULL);
    return NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, property, expression);
}


static void ClearFont(void)
{
    font.style = font.variant = font.weight = NULL;
    font.size = font.leading = font.family = NULL;
    font.normal_count = font.parse_error = 0;
}


static void AddFont(int node_type, css_node node)
{
    if (FontStyle == node_type && (! font.style))
        font.style = node;
    else if (FontVariant == node_type && (! font.variant))
        font.variant = node;
    else if (FontWeight == node_type && (!  font.weight))
        font.weight = node;
    else if (FontNormal == node_type)
        font.normal_count++;
    else if (FontSize == node_type && (! font.size))
        font.size = node;
    else if (FontLeading == node_type && (! font.leading))
        font.leading = node;
    else if (FontFamily == node_type && (! font.family))
        font.family = node;
    else {
        font.parse_error++;
        if (node) css_FreeNode(node);
    }
}


static css_node AssembleFont(void)
{
    css_node head, element;
    int count;

    count = 0;
    if (font.style)       count++;
    if (font.variant)     count++;
    if (font.weight)      count++;
    if (font.normal_count > (3 - count))
        font.parse_error++;

    if (font.parse_error) {
        if (font.style)    css_FreeNode(font.style);
        if (font.variant)  css_FreeNode(font.variant);
        if (font.weight)   css_FreeNode(font.weight);
        if (font.size)     css_FreeNode(font.size);
        if (font.leading)  css_FreeNode(font.leading);
        if (font.family)   css_FreeNode(font.family);
        ClearFont();
        return NULL;
    }

    if (! font.style)
        font.style = NewDeclarationNode(NODE_IDENT, css_normal, font_style);
    if (! font.variant)
        font.variant = NewDeclarationNode(NODE_IDENT, css_normal, font_variant);
    if (! font.weight)
        font.weight = NewDeclarationNode(NODE_IDENT, css_normal, font_weight);
    if (! font.leading)
        font.leading = NewDeclarationNode(NODE_IDENT, css_normal, line_height);

    head = NewNode(NODE_DECLARATION_LIST, NULL, NULL, font.style);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, font.variant);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, font.weight);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, font.size);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, font.leading);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, font.family);
    LeftAppendNode(head, element);

    ClearFont();
    return head;
}


static void ClearBackground(void)
{
    bg.color = bg.image = bg.repeat = NULL;
    bg.attachment = bg.position = NULL;
    bg.parse_error = 0;
}


static void AddBackground(int node_type, css_node node)
{
    if (BackgroundColor == node_type && (! bg.color))
        bg.color = node;
    else if (BackgroundImage == node_type && (! bg.image))
        bg.image = node;
    else if (BackgroundRepeat == node_type && (!  bg.repeat))
        bg.repeat = node;
    else if (BackgroundAttachment == node_type && (! bg.attachment))
        bg.attachment = node;
    else if (BackgroundPosition == node_type && (! bg.position))
        bg.position = node;
    else {
        bg.parse_error++;
        css_FreeNode(node);
    }
}


static css_node AssembleBackground(void)
{
    css_node head, element;

    if (bg.parse_error) {
        if (bg.color)
            css_FreeNode(bg.color);
        if (bg.image)
            css_FreeNode(bg.image);
        if (bg.repeat)
            css_FreeNode(bg.repeat);
        if (bg.attachment)
            css_FreeNode(bg.attachment);
        if (bg.position)
            css_FreeNode(bg.position);
        ClearBackground();
        return NULL;
    }

    if (! bg.color)
        bg.color = NewDeclarationNode(NODE_IDENT, css_transparent, bg_color);
    if (! bg.image)
        bg.image = NewDeclarationNode(NODE_IDENT, css_none, bg_image);
    if (! bg.repeat)
        bg.repeat = NewDeclarationNode(NODE_IDENT, css_repeat, bg_repeat);
    if (! bg.attachment)
        bg.attachment = NewDeclarationNode(NODE_IDENT, css_scroll, bg_attachment);
    if (! bg.position)
        bg.position = NewDeclarationNode(NODE_PERCENTAGE, css_origin,
                                         bg_position);

    head = NewNode(NODE_DECLARATION_LIST, NULL, NULL, bg.color);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, bg.image);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, bg.repeat);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, bg.attachment);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, bg.position);
    LeftAppendNode(head, element);

    ClearBackground();
    return head;
}                                      
        

static void ClearListStyle(void)
{
    ls.marker = ls.image = ls.position = NULL;
    ls.none_count = ls.parse_error = 0;
}

static void AddListStyle(int node_type, css_node node)
{
    if (ListStyleMarker == node_type && (! ls.marker))
        ls.marker = node;
    else if (ListStyleImage == node_type && (! ls.image))
        ls.image = node;
    else if (ListStylePosition == node_type && (! ls.position))
        ls.position = node;
    else if (ListStyleNone == node_type)
        ls.none_count++;
    else {
        ls.parse_error++;
        if (node) css_FreeNode(node);
    }
}

static css_node AssembleListStyle(void)
{
    css_node head, element;
    int count;
    char * marker_value;

    count = 0;
    if (ls.marker)	count++;
    if (ls.image)	count++;
    if (ls.none_count > (2 - count))
        ls.parse_error++;

    if (ls.parse_error) {
        if (ls.marker)	css_FreeNode(ls.marker);
        if (ls.image)	css_FreeNode(ls.image);
        if (ls.position)	css_FreeNode(ls.position);
        ClearListStyle();
        return NULL;
    }

    if (! ls.marker) {
        /* list-style: none
         * could mean marker or image.  It should set the marker;
         * the image will default to none.
         */
        marker_value = ls.none_count ? css_none : css_disc;
        ls.marker = NewDeclarationNode(NODE_IDENT, marker_value,
                                       list_style_type);
    }
    if (! ls.image)
        ls.image = NewDeclarationNode(NODE_IDENT, css_none, list_style_image);
    if (! ls.position)
        ls.position = NewDeclarationNode(NODE_IDENT, css_outside,
                                         list_style_position);

    head = NewNode(NODE_DECLARATION_LIST, NULL, NULL, ls.marker);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, ls.image);
    LeftAppendNode(head, element);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, ls.position);
    LeftAppendNode(head, element);

    ClearListStyle();
    return head;
}


static void ClearBorder(void)
{
    border.width = border.style = border.color = (css_node) NULL;
    border.parse_error = 0;
}

static void AddBorder(int node_type, css_node node)
{
    if (BorderWidth == node_type && (! border.width))
        border.width = node;
    else if (BorderStyle == node_type && (! border.style))
        border.style = node;
    else if (BorderColor == node_type && (! border.color))
        border.color = node;
    else {
        border.parse_error++;
        if (node) css_FreeNode(node);
    }
}

static css_node AssembleBorder(void)
{
    css_node head, element;

    if (border.parse_error) {
        if (border.width) css_FreeNode(border.width);
        if (border.style) css_FreeNode(border.style);
        if (border.color) css_FreeNode(border.color);
        ClearBorder();
        return (css_node) NULL;
    }

    if (! border.width)
        border.width = NewDeclarationNode(NODE_IDENT, css_medium, border_width);
    if (! border.style)
        border.style = NewDeclarationNode(NODE_IDENT, css_none, border_style);

    head = NewNode(NODE_DECLARATION_LIST, NULL, NULL, border.width);
    element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, border.style);
    LeftAppendNode(head, element);
    if (border.color) {
        element = NewNode(NODE_DECLARATION_LIST, NULL, NULL, border.color);
        LeftAppendNode(head, element);
    }

    ClearBorder();
    return head;
}

/* css_overflow is only for parsers generated by bison. */
static void css_overflow(const char *message, short **yyss1, int yyss1_size, 
                         YYSTYPE **yyvs1, int yyvs1_size, int *yystacksize)
{
    short * yyss2;
    YYSTYPE * yyvs2;
    int new_size;


    if (*yystacksize >= YYMAXDEPTH)
        return;

    new_size = *yystacksize * 2;
    if (new_size > YYMAXDEPTH)
        new_size = YYMAXDEPTH;

    if (*yystacksize == YYINITDEPTH) {
        /* First time allocating from the heap. */
        yyss2 = (short *) XP_ALLOC(new_size * sizeof(short));
        if (yyss2)
            (void) memcpy((void *)yyss2, (void *) *yyss1, yyss1_size);
        yyvs2 = XP_ALLOC(new_size * sizeof(YYSTYPE));
        if (yyvs2)
            (void) memcpy((void *)yyvs2, (void *) *yyvs1, yyvs1_size);
    } else {
        yyss2 = (short *) XP_REALLOC(*yyss1, new_size * sizeof(short));
        yyvs2 = XP_REALLOC(*yyvs1, new_size * sizeof(YYSTYPE));
    }

    if (yyss2 && yyvs2) {
        *yyss1 = yyss2;
        *yyvs1 = yyvs2;
        *yystacksize = new_size;
    }

    /* Any failure to allocate will be noticed by the caller. */
}

