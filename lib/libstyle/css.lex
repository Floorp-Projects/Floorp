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
%{

/* For flex.  Requires support for 8-bit characters and case independence. */

#include "cssI.h"
#include "csstab.h"

#undef YY_INPUT
#define YY_INPUT(buf, result, max_size)  css_GetBuf(buf, &result, max_size)

#if !defined(__cplusplus) && !(__STDC__) && !defined(__TURBOC__)
/* Stuff from stdlib.h -- needed by win16 */
#define YY_MALLOC_DECL \
extern void free (void *); \
extern void *malloc (size_t size); \
extern void *realloc (void *, size_t);
#endif

static int input(void);

/* We never take input off of the command line. */
#define YY_NEVER_INTERACTIVE 1

#ifdef CSS_PARSE_DEBUG
#define RETURN(x) {printf("lex returns %d for '%s'\n",x,css_text);return(x);}
#else
#define RETURN(x) return(x)
#endif

%}

unicode         \\[0-9a-f]{1,4}
latin1          [¡-ÿ]
escape          {unicode}|\\[ -~¡-ÿ]
stringchar      {escape}|{latin1}|[ !#$%&(-~]
nmstrt          [a-z]|{latin1}|{escape}
nmchar          [-a-z0-9]|{latin1}|{escape}
ident           {nmstrt}{nmchar}*
name            {nmchar}+
d               [0-9]
notnm           [^-a-z0-9\\]|{latin1}
w               [ \t\n]*
num             {d}+|{d}*\.{d}+
string          \"({stringchar}|\')*\"|\'({stringchar}|\")*\'
hex             [0-9a-f]
hex3            [0-9a-f]{3}
hex6            [0-9a-f]{6}

%x css_comment
%x css_ignore
%x css_property
%x css_value
%x css_bg
%x css_font
%x css_line_height
%x css_list_style
%x css_border
%x css_font_size

%s css_after_ident

%%
%{


/* CSS1 17 December 1996, with the following changes:
 * 1.  Expand whitespace definition in the rules section.
 * 2.  Change default rule action to be silent.
 * 3.  Require class names to be an IDENT.
 * 4.  Require id names to be an IDENT.
 * 5.  Return '.' as a separate lexical token;
 *     add DOT and DOT_AFTER_IDENT; remove CLASS and CLASS_AFTER_IDENT
 * 6.  Return '#' as a separate lexical token;
 *     remove HASH and HASH_AFTER_IDENT.
 * 7.  Gut the use of AFTER_IDENT tokens for pseudoclasses and pseudoelements.
 * 8.  Provide a definition for HEXCOLOR token.
 *     move { and } out into state triggers
 * 9.  Gave up on preserving any heritage and made use of exclusive states.
 */

/* These state variables can be on the stack because their state is not
 * useful across a token return.  They should always be set to 0 before
 * starting a new input file.
 */
short int css_prior_state = 0;
short int css_nest_count = 0;

%}
\<\!\-\-                        {BEGIN(0); RETURN(CDO);}
\-\-\>                          {BEGIN(0); RETURN(CDC);}

<css_after_ident>":"link        {BEGIN(0);RETURN(LINK_PSCLASS);}
<css_after_ident>":"visited     {BEGIN(0);RETURN(VISITED_PSCLASS);}
<css_after_ident>":"active      {BEGIN(0);RETURN(ACTIVE_PSCLASS);}
<css_after_ident>"."            {BEGIN(0);RETURN(DOT_AFTER_IDENT);}
{ident}                         {BEGIN(css_after_ident); RETURN(IDENT);}

":"link                         {BEGIN(0);RETURN(LEADING_LINK_PSCLASS);}
":"visited                      {BEGIN(0);RETURN(LEADING_VISITED_PSCLASS);}
":"active                       {BEGIN(0);RETURN(LEADING_ACTIVE_PSCLASS);}
":"first-line                   {BEGIN(0);RETURN(FIRST_LINE);}
":"first-letter                 {BEGIN(0);RETURN(FIRST_LETTER);}
"."                             {BEGIN(0);RETURN(DOT);}
[,#;]                           {BEGIN(0);return (*css_text);}
[ \t\f\v\r\n]+                  {BEGIN(0);}

@import                         {BEGIN(0); RETURN(IMPORT_SYM);}
@fontdef                        {BEGIN(0); RETURN(FONTDEF);}
url\({w}{string}{w}\)                             |
url\({w}([^ \n\'\")]|\\\ |\\\'|\\\"|\\\))+{w}\)   {RETURN(URL);}
{string}                        {RETURN(STRING);}

@{ident}             {BEGIN(css_ignore); css_nest_count=0;}
<css_ignore>{string}     |
<css_ignore>[^;{}]*      { /* unrecognized @rules are ignored. */}
<css_ignore>";"          {if (0 == css_nest_count) BEGIN(0);}
<css_ignore>"{"          {css_nest_count++;}
<css_ignore>"}"          {if (0 >= --css_nest_count) BEGIN(0);}

<*>"/*"              {if (css_comment != YY_START) {
                         /* Comments cannot be nested. */
                         css_prior_state = YY_START;}
                     BEGIN(css_comment);}
<css_comment>[^*]*       |
<css_comment>"*"+[^*/]*  {/* Comments function as whitespace. */}
<css_comment>"*"+"/"     {BEGIN(css_prior_state);}

<css_property,css_value,css_bg,css_font,css_line_height,css_list_style,css_border,css_font_size>[ \t\f\v\r\n]+ {/* whitespace can separate tokens */}
<css_property,css_value,css_bg,css_font,css_line_height,css_list_style,css_border,css_font_size>"}"   {BEGIN(0); return (*css_text);}

<css_value,css_bg,css_font,css_line_height,css_list_style,css_border,css_font_size>[;]     {BEGIN(css_property); return (*css_text);}
<css_value,css_bg,css_font,css_line_height,css_list_style,css_border,css_font_size>"!"{w}important    {RETURN(IMPORTANT_SYM);}

"{"                           {BEGIN(css_property); return (*css_text);}
<css_property>[:]                 {BEGIN(css_value); return (*css_text);}
<css_property>[-/+;,#]            {return (*css_text);}
<css_property>"background"        {BEGIN(css_bg); RETURN(BACKGROUND);}
<css_property>font                {BEGIN(css_font); RETURN(FONT);}
<css_property>list-style          {BEGIN(css_list_style); RETURN(LIST_STYLE);}
<css_property>border              {BEGIN(css_border); RETURN(BORDER);}
<css_property>font-size           {BEGIN(css_font_size); RETURN(FONT_SIZE_PROPERTY);}
<css_property>{ident}             {RETURN(IDENT);}

<css_font>italic|oblique                        {RETURN(FONT_STYLE);}
<css_font>small-caps                            {RETURN(FONT_VARIANT);}
<css_font>bold|bolder|lighter                   {RETURN(FONT_WEIGHT);}
<css_font>100|200|300|400|500|600|700|800|900   {RETURN(FONT_WEIGHT);}
<css_font>normal                                {RETURN(FONT_NORMAL);}
<css_font,css_font_size>xx-small|x-small|small|medium         {RETURN(FONT_SIZE);}
<css_font,css_font_size>large|x-large|xx-large|larger|smaller {RETURN(FONT_SIZE);}
<css_font,css_font_size,css_line_height>{num}"%"       {BEGIN(css_font); RETURN(PERCENTAGE);}
<css_font,css_font_size>0                                  |
<css_font,css_font_size,css_line_height>{num}pt/{notnm}    |
<css_font,css_font_size,css_line_height>{num}mm/{notnm}    |  
<css_font,css_font_size,css_line_height>{num}cm/{notnm}    |
<css_font,css_font_size,css_line_height>{num}pc/{notnm}    |
<css_font,css_font_size,css_line_height>{num}in/{notnm}    |
<css_font,css_font_size,css_line_height>{num}px/{notnm}    {BEGIN(css_font); RETURN(LENGTH);}
<css_font,css_font_size,css_line_height>{num}em/{notnm}    {BEGIN(css_font); RETURN(EMS);}
<css_font,css_font_size,css_line_height>{num}ex/{notnm}    {BEGIN(css_font); RETURN(EXS);}
<css_font>"/"                {BEGIN(css_line_height); return (*css_text);}
<css_line_height>[-+]        {return (*css_text);}
<css_line_height>{num}       {BEGIN(css_font); RETURN(NUMBER);}
<css_line_height>normal      {BEGIN(css_font); RETURN(LINE_HEIGHT);}
<css_font>{ident}            {RETURN(IDENT);}
<css_font>{string}           {RETURN(STRING);}
<css_font,css_font_size,css_list_style>[:,+] {return (*css_text);}

<css_list_style>disc|circle|square|decimal|lower-roman|upper-roman|lower-alpha|upper-alpha  {RETURN(LS_TYPE);}
<css_list_style>none                        {RETURN(LS_NONE);}
<css_list_style>inside|outside              {RETURN(LS_POSITION);}

<css_bg>transparent                         {RETURN(BG_COLOR);}
<css_bg>none                                {RETURN(BG_IMAGE);}
<css_bg>repeat|repeat-x|repeat-y|no-repeat  {RETURN(BG_REPEAT);}
<css_bg>scroll|fixed                        {RETURN(BG_ATTACHMENT);}
<css_bg>left|right|top|center|bottom        {RETURN(BG_POSITION);}

<css_value,css_bg,css_border>[-/+,#:]           {return (*css_text);}
<css_value,css_bg>{ident}            {RETURN(IDENT);}
<css_value>[)(]                      {RETURN(IDENT); /* quick b2 hack */}
<css_value,css_bg>{string}           {RETURN(STRING);}
<css_value,css_bg,css_border>"#"{hex3}          |
<css_value,css_bg,css_border>"#"{hex6}          {RETURN(HEXCOLOR);}
<css_value,css_bg,css_border>rgb\({w}{num}%?{w}\,{w}{num}%?{w}\,{w}{num}%?{w}\)  {RETURN(RGB);}
<css_value,css_bg,css_list_style>url\({w}{string}{w}\)  |
<css_value,css_bg,css_list_style>url\({w}([^ \n\'\")]|\\\ |\\\'|\\\"|\\\))+{w}\) {RETURN(URL);}
<css_value,css_bg,css_border>{num}"%"           {RETURN(PERCENTAGE);}
<css_value,css_bg,css_border>0                  |
<css_value,css_bg,css_border>{num}pt/{notnm}    |
<css_value,css_bg,css_border>{num}mm/{notnm}    |
<css_value,css_bg,css_border>{num}cm/{notnm}    |
<css_value,css_bg,css_border>{num}pc/{notnm}    |
<css_value,css_bg,css_border>{num}in/{notnm}    |
<css_value,css_bg,css_border>{num}px/{notnm}    {RETURN(LENGTH);}
<css_value,css_bg,css_border>{num}em/{notnm}    {RETURN(EMS);}
<css_value,css_bg,css_border>{num}ex/{notnm}    {RETURN(EXS);}

<css_value>{num}                 {RETURN(NUMBER);}

<css_border>none|dotted|dashed|solid|double     |
<css_border>groove|ridge|inset|outset           {RETURN(BORDER_STYLE);}
<css_border>thin|medium|thick                   {RETURN(BORDER_WIDTH);}
<css_border>{ident}                             {RETURN(IDENT);}

<<EOF>>                   {BEGIN (0); yyterminate();}
<*>.                      {RETURN(WILD);}
%%
