/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <stdio.h>
#include "cssI.h"

%}

%token  NUMBER STRING PERCENTAGE LENGTH EMS
%token  EXS IDENT HEXCOLOR URL RGB
%token  CDO CDC
%token  IMPORTANT_SYM
%token  IMPORT_SYM
%token  DOT_AFTER_IDENT DOT
%token  LINK_PSCLASS VISITED_PSCLASS ACTIVE_PSCLASS
%token  LEADING_LINK_PSCLASS
%token  LEADING_VISITED_PSCLASS
%token  LEADING_ACTIVE_PSCLASS
%token  FIRST_LINE
%token  FIRST_LETTER
%token  WILD
%token  BACKGROUND
%token  BG_COLOR BG_IMAGE BG_REPEAT BG_ATTACHMENT BG_POSITION
%token  FONT
%token  FONT_STYLE FONT_VARIANT FONT_WEIGHT FONT_SIZE
%token  FONT_NORMAL LINE_HEIGHT
%token  LIST_STYLE LS_TYPE LS_NONE LS_POSITION
%token  BORDER BORDER_STYLE BORDER_WIDTH
%token  FONT_SIZE_PROPERTY
%token  FONTDEF

%type   <binary_node> stylesheet
%type   <binary_node> import ruleset unary_operator operator property
%type   <binary_node> import_value
%type   <binary_node> selector_list declaration_list declaration selector
%type   <binary_node> pseudo_element simple_selector
%type   <binary_node> contextual_selector
%type   <binary_node> contextual_selector_list
%type   <binary_node> element_name pseudo_class class id
%type   <binary_node> leading_pseudo_class
%type   <binary_node> expr prio term optional_priority
%type   <binary_node> numeric_const unsigned_numeric_const
%type   <binary_node> numeric_unit unsigned_numeric_unit
%type   <binary_node> unsigned_symbol color_code
%type   <binary_node> url
%type   <binary_node> background_property
%type   <binary_node> background_values_list
%type   <binary_node> background_value
%type   <binary_node> background_values
%type   <binary_node> background_position_expr
%type   <binary_node> background_position_value
%type   <binary_node> background_position_keyword
%type   <binary_node> font_property font_size_property
%type   <binary_node> font_values_list
%type   <binary_node> font_size_value font_family_value line_height_value
%type   <binary_node> font_optional_value font_optional_values_list
%type   <binary_node> font_family_operator font_family_expr
%type   <binary_node> list_style_property
%type   <binary_node> list_style_values_list list_style_value
%type   <binary_node> border_property
%type   <binary_node> border_values_list border_value
%type   <binary_node> atrule fontdef

%union {
  css_node binary_node;
}


%{

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

%}
%%

stylesheet
    :   {
        $$ = NULL;
        }
    | stylesheet CDO {
        TRACE1("-cdo\n");
        $$ = $1;
        }
    | stylesheet atrule {
        $$ = $1;
        if ($$ == NULL) {
            css_tree_root = $2;
            $$ = css_tree_root;
        }
        else
            LeftAppendNode($$, $2);
        }
    | stylesheet ruleset {
        css_node tmp;
        tmp = NewNode(NODE_RULESET_LIST, NULL, NULL, $2);
        
        $$ = $1;
        if ($$ == NULL) {
            css_tree_root = tmp;
            $$ = css_tree_root;
        }
        else
            LeftAppendNode($$, tmp);
        }
    | stylesheet CDC {
        TRACE1("-cdc\n");
        $$ = $1;
        }
    | stylesheet error {
        yyerrok;
        yyclearin;
        }        
    ;

atrule
    : import {
        $$ = NewNode(NODE_IMPORT_LIST, NULL, NULL, $1);
        }
    | fontdef {
        $$ = NewNode(NODE_FONTDEF_LIST, NULL, NULL, $1);
        }
    ;

import 
 : IMPORT_SYM import_value ';' { 
     $$ = $2;
   }
 ;

fontdef
    : FONTDEF import_value ';' { 
        $$ = $2;
        }
    ;

import_value
 : STRING {
    $$ = NewNode(NODE_IMPORT_STRING, css_text, NULL, NULL);
   }
 | url {
     TRACE1("-import url\n");
     $1->node_id = NODE_IMPORT_URL;
     $$ = $1;
   }
 ;

unary_operator
 : '-' { 
     TRACE1("-unary_op '-'\n");
     $$ = NewNode(NODE_UNARY_OP, css_text, NULL, NULL);
   }
 | '+' {
     TRACE1("-unary_op '+'\n");
     $$ = NewNode(NODE_UNARY_OP, css_text, NULL, NULL);
   }
 ;

operator
 :       { 
     TRACE1("-empty operator\n");
     $$ = NewNode(NODE_EMPTY_OP, NULL, NULL, NULL);
   }
 | '/'	{ 
     TRACE1("-operator : '/'\n"); 
     $$ = NewNode(NODE_EXPR_OP, css_text, NULL, NULL);
   }
 | ',' 	{ 
     TRACE1("-operator : ','\n"); 
     $$ = NewNode(NODE_EXPR_OP, css_text, NULL, NULL);
   }
 ;

property
 : IDENT   {
     TRACE2("-property : IDENT '%s'\n", css_text); 
     $$ = NewNode(NODE_PROPERTY, css_text, NULL, NULL);
   }
 ;

font_size_property
 : FONT_SIZE_PROPERTY {
     $$ = NewNode(NODE_PROPERTY, css_text, NULL, NULL);
   }
 ;

background_property
 : BACKGROUND {
     TRACE2("-property : BACKGROUND '%s'\n", css_text);
     ClearBackground();
     $$ = NULL;
     /* This shorthand property will generate five component properties. */
   }
 ;

font_property
 : FONT {
     TRACE2("-property : FONT '%s'\n", css_text);
     ClearFont();
     $$ = NULL;
     /* This shorthand property will generate six component properties. */
   }
 ;

list_style_property
 : LIST_STYLE {
     ClearListStyle();
     $$ = NULL;
     /* This shorthand property will generate three component properties. */
   }
 ;

border_property
 : BORDER {
     ClearBorder();
     $$ = NULL;
     /* This shorthand property will generate 2 or 3 component properties. */
   }
 ;

ruleset
 : selector_list '{' declaration_list '}'  { 
     TRACE1("-selector_list { declaration_list }\n"); 
     $$ = NewNode(NODE_RULESET, NULL, $1, $3);
   }
 ;

selector_list
 : selector { 
     TRACE1("-selector\n"); 
     $$ = NewNode(NODE_SELECTOR_LIST, NULL, NULL, $1);
   }
 | selector_list ',' selector { 
     css_node tmp;
     TRACE1("-selector_list , selector\n"); 
     tmp = NewNode(NODE_SELECTOR_LIST, NULL, NULL, $3);
     
     $$ = $1;
     if( $$ == NULL )
       $$ = tmp;
     else
       LeftAppendNode( $$, tmp );
   }
 ;

selector
: simple_selector {
    TRACE1("-simple_selector\n");
    $$ = NewNode(NODE_SELECTOR, NULL, $1, NULL);
}
| simple_selector pseudo_element {
    TRACE1("-simple_selector : pseudo_element\n");
    $$ = NewNode(NODE_SELECTOR, NULL, $1, $2);
}
| contextual_selector {
    TRACE1("-contextual_selector\n");
    $$ = NewNode(NODE_SELECTOR, NULL, $1, NULL);
}
| contextual_selector pseudo_element {
    TRACE1("-contextual_selector : pseudo_element\n");
     $$ = NewNode(NODE_SELECTOR, NULL, $1, $2);
}
;

         /*
         * A simple_selector is something like H1, PRE.FOO,
         * .FOO, etc., or it is an ID: #p004
         */

contextual_selector
: contextual_selector_list simple_selector {
    TRACE1("-contextual_selector_list simple_selector\n");
    $$ = NewNode(NODE_SELECTOR_CONTEXTUAL, NULL, $1, $2);
}
;

contextual_selector_list
 : simple_selector {
     TRACE1("-simple_selector\n");
     $$ = NewNode(NODE_SELECTOR_CONTEXTUAL, NULL, NULL, $1);
   }
 | contextual_selector_list simple_selector {
     TRACE1("-contextual_selector_list simple_selector\n");
     $$ = NewNode(NODE_SELECTOR_CONTEXTUAL, NULL, $1, $2);
   }
 ;

simple_selector
 : element_name { 
     TRACE1("-element_name\n");
     $$ = NewNode(NODE_SIMPLE_SELECTOR_NAME_ONLY, NULL, $1, NULL);
   }
 | DOT class {
     TRACE1("-dot class\n");
     $$ = NewNode(NODE_SIMPLE_SELECTOR_DOT_AND_CLASS, NULL, $2, NULL);
   }
 | id {
     TRACE1("-id\n");
     $$ = NewNode(NODE_SIMPLE_SELECTOR_ID_SELECTOR, NULL, $1, NULL);
   }
 | element_name DOT_AFTER_IDENT class {
     TRACE1("-element dot class\n");
     $$ = NewNode(NODE_SIMPLE_SELECTOR_NAME_AND_CLASS, NULL, $1, $3);
   }
 | element_name pseudo_class {
     TRACE1("-element pseudo_class\n");
     $$ = NewNode(NODE_SIMPLE_SELECTOR_NAME_PSEUDO_CLASS, NULL, $1, $2);
   }
 | element_name DOT_AFTER_IDENT class pseudo_class {
     TRACE2("-element_name '%s' dot class pseudo_class\n", $1->string);
     $1->node_id = NODE_SIMPLE_SELECTOR_NAME_CLASS_PSEUDO_CLASS;
     $1->left = $3;
     $1->right = $4;
     $$ = $1;
   }
 | DOT class pseudo_class {
     TRACE1("-dot class pseudo_class\n");
     $$ = NewNode(NODE_SIMPLE_SELECTOR_NAME_CLASS_PSEUDO_CLASS, "A", $2, $3);
   }
 | leading_pseudo_class {
     css_node tmp;
     TRACE1("solitary pseudo_class\n");
     /* See CSS1 spec of 17 December 1996 section 2.1 Anchor pseudo-classes */
     tmp = NewNode(NODE_ELEMENT_NAME, "A", NULL, NULL);
     $$ = NewNode(NODE_SIMPLE_SELECTOR_NAME_PSEUDO_CLASS, NULL, tmp, $1);
   }
 | WILD {
     $$ = NewNode(NODE_WILD, NULL, NULL, NULL);
   }
 ;

element_name
 : IDENT { 
     TRACE2("-IDENT '%s' to element_name\n", css_text); 
     $$ = NewNode(NODE_ELEMENT_NAME, css_text, NULL, NULL);
   }
 ;

class
    : IDENT {
        TRACE2("-IDENT '%s' to class\n", css_text);
        $$ = NewNode(NODE_CLASS, css_text, NULL, NULL);
      }
    ;

id
    : '#' IDENT {
        TRACE2("-IDENT '%s' to id\n", css_text);
        $$ = NewNode(NODE_ID_SELECTOR, css_text, NULL, NULL);
      }
    ;

pseudo_class
 : LINK_PSCLASS {
     $$ = NewNode(NODE_LINK_PSCLASS, css_link, NULL, NULL);
   }
 | VISITED_PSCLASS {
     $$ = NewNode(NODE_VISITED_PSCLASS, css_visited, NULL, NULL);
   }
 | ACTIVE_PSCLASS {
     $$ = NewNode(NODE_ACTIVE_PSCLASS, css_active, NULL, NULL);
   }
 ;

leading_pseudo_class
 : LEADING_LINK_PSCLASS {
     $$ = NewNode(NODE_LINK_PSCLASS, css_link, NULL, NULL);
   }
 | LEADING_VISITED_PSCLASS {
     $$ = NewNode(NODE_VISITED_PSCLASS, css_visited, NULL, NULL);
   }
 | LEADING_ACTIVE_PSCLASS {
     $$ = NewNode(NODE_ACTIVE_PSCLASS, css_active, NULL, NULL);
   }
 ;

pseudo_element
 : FIRST_LINE { 
     TRACE2("-IDENT '%s' to pseudo_element\n", css_text);
     $$ = NewNode(NODE_PSEUDO_ELEMENT, "first-Line", NULL, NULL);
   }
 | FIRST_LETTER { 
     TRACE2("-IDENT '%s' to pseudo_element\n", css_text);
     $$ = NewNode(NODE_PSEUDO_ELEMENT, "first-Letter", NULL, NULL);
   }
 ;

declaration_list 
 : declaration {
     TRACE1("-declaration\n");
     $$ = $1;
   }
 | declaration_list ';' declaration {
     /* to keep the order, append the new node to the end. */
     TRACE1("-declaration_list ';' declaration\n");
     $$ = $1;
     if (NULL == $$)
       $$ = $3;
     else
       LeftAppendNode($$, $3);
   }
 ;

declaration
 :       { 
     TRACE1("-empty declaration\n");
     $$ = NewNode(NODE_DECLARATION_LIST, NULL, NULL, NULL);
   } 
 | property ':' expr optional_priority {
     css_node dcl;
     TRACE1("-property : expr\n");
     dcl = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, $1, $3);
     $$ = NewNode(NODE_DECLARATION_LIST, NULL, NULL, dcl);
   }
 | font_size_property ':' font_size_value optional_priority {
     css_node expr, dcl;
     expr = NewNode(NODE_EXPR, NULL, NULL, $3);
     dcl = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, $1, expr);
     $$ = NewNode(NODE_DECLARATION_LIST, NULL, NULL, dcl);
    }
 | background_property ':' background_values_list optional_priority {
     /* The priority notation is ignored by the translator
      * so we conveniently ignore it here, too.
      */
     TRACE1("-background property : background_values_list\n");
     $$ = AssembleBackground();
   }
 | font_property ':' font_values_list optional_priority {
     TRACE1("-font property : font_values_list prio\n");
     /* The priority notation is ignored by the translator
      * so we conveniently ignore it here, too.
      */
     $$ = AssembleFont();
   }
  | list_style_property ':' list_style_values_list optional_priority {
     $$ = AssembleListStyle();
   }
  | border_property ':' border_values_list optional_priority {
     $$ = AssembleBorder();
    }
 ;

optional_priority
    : { /* nothing */ }
    | prio
    ;

prio
 : IMPORTANT_SYM {                       /* !important */
     TRACE1("-IMPORTANT_SYM\n");
     $$ = NULL;
   }
 ;

expr
 : term {
     TRACE1("-term\n");
     $$ = NewNode(NODE_EXPR, NULL, NULL, $1);
   }
 | expr operator term  {
     TRACE1("-expr op term\n");

     $$ = $1;
     /* put the new term at the end */
     $2->right = $3;

     if ($$ == NULL)
       $$ = $2;
     else
       LeftAppendNode( $$, $2 ); 
   }
 ;

term
 : unsigned_symbol
 | color_code
 | url
 | numeric_unit
 | numeric_const
 ;

numeric_const
 : unsigned_numeric_const
 | unary_operator unsigned_numeric_const {
     TRACE1("-unary_operator signed_const to term\n"); 
     $$ = $1;
     $$->left = $2;
   }
 ;

unsigned_numeric_const
 : NUMBER {
     $$ = NewNode(NODE_NUMBER, css_text, NULL, NULL);
   }
 ;

numeric_unit
 : unsigned_numeric_unit
 | unary_operator unsigned_numeric_unit {
     $$ = $1;
     $$->left = $2;
   }
 ;

unsigned_numeric_unit
 : PERCENTAGE {
     $$ = NewNode(NODE_PERCENTAGE, css_text, NULL, NULL);
   }
 | LENGTH {
     $$ = NewNode(NODE_LENGTH, css_text, NULL, NULL);
   }
 | EMS {
     $$ = NewNode(NODE_EMS, css_text, NULL, NULL);
   }
 | EXS{
     $$ = NewNode(NODE_EMS, css_text, NULL, NULL);
   }
 ;

unsigned_symbol
 : STRING {
     $$ = NewNode(NODE_STRING, css_text, NULL, NULL);
   }
 | IDENT { 
     TRACE2("-IDENT '%s' to unsigned_symbol\n", css_text);
     $$ = NewNode(NODE_IDENT, css_text, NULL, NULL);
   }
 ;

url
 : URL {
     $$ = NewNode(NODE_URL, css_text, NULL, NULL);
   }
 ;

color_code
 : HEXCOLOR {
     $$ = NewNode(NODE_HEXCOLOR, css_text, NULL, NULL);
   }
 | RGB {
     $$ = NewNode(NODE_RGB, css_text, NULL, NULL);
   }
 ;

background_values_list
    : background_values
    | background_position_value
    | background_values background_position_value
    | background_position_value background_values
    | background_values background_position_value background_values
    ;

background_values
    : background_value
    | background_values background_value
    ;

background_value
    : URL {
        $$ = NewDeclarationNode(NODE_URL, css_text, bg_image);
        AddBackground(BackgroundImage, $$);
      }
    | BG_IMAGE {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, bg_image);
        AddBackground(BackgroundImage, $$);
      }
    | color_code {
        $$ = NewComponentNode($1, bg_color);
        AddBackground(BackgroundColor, $$);
      }
    | IDENT {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, bg_color);
        AddBackground(BackgroundColor, $$);
      }
    | BG_COLOR {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, bg_color);
        AddBackground(BackgroundColor, $$);
      }
    | BG_REPEAT {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, "background-repeat");
        AddBackground(BackgroundRepeat, $$);
      }
    | BG_ATTACHMENT {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, "background-attachment");
        AddBackground(BackgroundAttachment, $$);
      }
    ;

background_position_value
    : background_position_expr {
        css_node property, declaration;
        property = NewNode(NODE_PROPERTY, bg_position, NULL, NULL);
        declaration = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL,
                              property, $1);
        AddBackground(BackgroundPosition, declaration);
      }
    ;

background_position_expr
    : numeric_unit {
        $$ = NewNode(NODE_EXPR, NULL, NULL, $1);
      }
    | numeric_unit numeric_unit {
        css_node operator;
        $$ = NewNode(NODE_EXPR, NULL, NULL, $1);
        operator = NewNode(NODE_EMPTY_OP, NULL, NULL, $2);
        if ($$ == NULL)
            $$ = operator;
        else
            LeftAppendNode($$, operator);
      }
    | background_position_keyword {
        $$ = NewNode(NODE_EXPR, NULL, NULL, $1);
      }
    | background_position_keyword background_position_keyword {
        css_node operator;
        $$ = $1;
        operator = NewNode(NODE_EMPTY_OP, NULL, NULL, $2);
        if ($$ == NULL)
            $$ = operator;
        else
            LeftAppendNode($$, operator);
      }
    ;

background_position_keyword
    : BG_POSITION {
        $$ = NewNode(NODE_IDENT, css_text, NULL, NULL);
      }
    ;

font_values_list
    : font_optional_values_list font_size_value font_family_value {
        css_node tmp;
        tmp = NewComponentNode($2, font_size);
        AddFont(FontSize, tmp);
        AddFont(FontFamily, $3);
        $$ = NULL;
      }
   | font_optional_values_list font_size_value '/' line_height_value font_family_value {
        css_node tmp;
        tmp = NewComponentNode($2, font_size);
        AddFont(FontSize, tmp);
        tmp = NewComponentNode($4, line_height);
        AddFont(FontLeading, tmp);
        AddFont(FontFamily, $5);
        $$ = NULL;
      }
    ;

font_family_operator
 :       { 
     TRACE1("-empty operator\n");
     $$ = NewNode(NODE_EMPTY_OP, NULL, NULL, NULL);
   }
 | ',' 	{ 
     TRACE1("-operator : ','\n"); 
     $$ = NewNode(NODE_EXPR_OP, css_text, NULL, NULL);
   }
 ;

font_family_value
 : font_family_expr {
     css_node property;
     TRACE1("-font_family_expr to font_family_value\n");
     property = NewNode(NODE_PROPERTY, font_family, NULL, NULL);
     $$ = NewNode(NODE_DECLARATION_PROPERTY_EXPR, NULL, property, $1);
    }
  ;

font_family_expr
 : unsigned_symbol {
     TRACE1("-unsigned_symbol\n");
     $$ = NewNode(NODE_EXPR, NULL, NULL, $1);
   }
 | font_family_expr font_family_operator unsigned_symbol  {
     TRACE1("-font_family_value font_family_op unsigned_symbol\n");

     $$ = $1;
     /* put the new term at the end */
     $2->right = $3;

     if ($$ == NULL)
       $$ = $2;
     else
       LeftAppendNode( $$, $2 ); 
   }
 ;

font_optional_values_list
    : { /* empty */
        $$ = NULL;
      }
    | font_optional_values_list font_optional_value {
        $$ = NULL;
      }
    ;

font_optional_value
    : FONT_STYLE {
	TRACE2("-FONT_STYLE '%s' to font_optional_value\n", css_text);
        $$ = NewDeclarationNode(NODE_IDENT, css_text, font_style);
        AddFont(FontStyle, $$);
      }
    | FONT_VARIANT {
	TRACE2("-FONT_VARIANT '%s' to font_optional_value\n", css_text);
        $$ = NewDeclarationNode(NODE_IDENT, css_text, font_variant);
        AddFont(FontVariant, $$);
      }
    | FONT_WEIGHT {
	TRACE2("-FONT_WEIGHT '%s' to font_optional_value\n", css_text);
        $$ = NewDeclarationNode(NODE_IDENT, css_text, font_weight);
        AddFont(FontWeight, $$);
      }
    | FONT_NORMAL {
        AddFont(FontNormal, NULL);
      }
    ;

font_size_value
    : unsigned_numeric_unit
    | FONT_SIZE {
        $$ = NewNode(NODE_IDENT, css_text, NULL, NULL);
      }
    | '+' unsigned_numeric_unit {
        /* just drop the '+' on the floor */
        $$ = $2;
      }
    ;

line_height_value
    : numeric_unit
    | numeric_const
    | LINE_HEIGHT {
        /* The only valid identifier is the word "normal".
         * There's an idea that a normal leading value is font-specific,
         * so the identifier is passed upwards.
         */
	$$ = NewNode(NODE_IDENT, css_text, NULL, NULL);
      }
    ;

list_style_values_list
    : list_style_value
    | list_style_values_list list_style_value
    ;

list_style_value
    : LS_TYPE {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, list_style_type);
        AddListStyle(ListStyleMarker, $$);
        $$ = NULL;
      }
    | LS_NONE {
        AddListStyle(ListStyleNone, NULL);
        $$ = NULL;
      }
    | LS_POSITION {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, list_style_position);
        AddListStyle(ListStylePosition, $$);
        $$ = NULL;
      }
    | url {
        $$ = NewComponentNode($1, list_style_image);
        AddListStyle(ListStyleImage, $$);
        $$ = NULL;
      }
    ;

border_values_list
    : border_value
    | border_values_list border_value
    ;

border_value
    : BORDER_STYLE {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, border_style);
        AddBorder(BorderStyle, $$);
        $$ = NULL;
      }
    | BORDER_WIDTH {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, border_width);
        AddBorder(BorderWidth, $$);
        $$ = NULL;
      }
    | numeric_unit {
        $$ = NewComponentNode($1, border_width);
        AddBorder(BorderWidth, $$);
        $$ = NULL;
      }
    | color_code {
        $$ = NewComponentNode($1, border_color);
        AddBorder(BorderColor, $$);
        $$ = NULL;
      }
    | IDENT {
        $$ = NewDeclarationNode(NODE_IDENT, css_text, border_color);
        AddBorder(BorderColor, $$);
        $$ = NULL;
      }
    ;
%%

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

