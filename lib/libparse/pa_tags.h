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

/*****************
 * These define all the strings that match
 * to all the MDL tag elements.  Note, these strings
 * MUST be in all lower case.  The actual language
 * has them as caseless, but for optimization purposes
 * all these are assumed to already be all lower case.
 *******************/
#define PT_TITLE        "title"
#define PT_INDEX        "isindex"
#define PT_BASE         "base"
#define PT_LINK         "link"
#define PT_HEADER_1     "h1"
#define PT_HEADER_2     "h2"
#define PT_HEADER_3     "h3"
#define PT_HEADER_4     "h4"
#define PT_HEADER_5     "h5"
#define PT_HEADER_6     "h6"
#define PT_ANCHOR       "a"
#define PT_PARAGRAPH    "p"
#define PT_ADDRESS      "address"
#define PT_IMAGE        "img"
#define PT_PLAIN_TEXT   "plaintext"
#define PT_PLAIN_PIECE  "xmp"
#define PT_PREFORMAT    "pre"
#define PT_LISTING_TEXT "listing"
#define PT_UNUM_LIST    "ul"
#define PT_NUM_LIST     "ol"
#define PT_MENU         "menu"
#define PT_DIRECTORY    "dir"
#define PT_LIST_ITEM    "li"
#define PT_DESC_LIST    "dl"
#define PT_DESC_TITLE   "dt"
#define PT_DESC_TEXT    "dd"
#define PT_STRIKEOUT    "strike"
#define PT_FIXED        "tt"
#define PT_BOLD         "b"
#define PT_ITALIC       "i"
#define PT_EMPHASIZED   "em"
#define PT_STRONG       "strong"
#define PT_CODE         "code"
#define PT_SAMPLE       "samp"
#define PT_KEYBOARD     "kbd"
#define PT_VARIABLE     "var"
#define PT_CITATION     "cite"
#define PT_BLOCKQUOTE   "blockquote"
#define PT_FORM         "form"
#define PT_INPUT        "input"
#define PT_SELECT       "select"
#define PT_OPTION       "option"
#define PT_TEXTAREA     "textarea"
#define PT_HRULE        "hr"
#define PT_LINEBREAK    "br"
#define PT_WORDBREAK    "wbr"
#define PT_NOBREAK      "nobr"
#define PT_BASEFONT     "basefont"
#define PT_FONT         "font"
#define PT_BLINK	"blink"
#define PT_NEW_IMAGE    "image"
#define PT_CENTER       "center"
#define PT_SUBDOC       "subdoc"
#define PT_CELL         "cell"
#define PT_TABLE        "table"
#define PT_CAPTION      "caption"
#define PT_TABLE_ROW    "tr"
#define PT_TABLE_HEADER "th"
#define PT_TABLE_DATA   "td"
#define PT_EMBED        "embed"
#define PT_BODY         "body"
#define PT_META         "meta"
#define PT_COLORMAP     "colormap"
#define PT_HYPE         "hype"
#define PT_BIG          "big"
#define PT_SMALL        "small"
#define PT_SUPER        "sup"
#define PT_SUB          "sub"
#define PT_GRID         "frameset"
#define PT_GRID_CELL    "frame"
#define PT_NOGRIDS      "noframes"
#define PT_JAVA_APPLET  "applet"
#define PT_PARAM        "param"
#define PT_MAP		"map"
#define PT_AREA		"area"
#define PT_DIVISION	"div"
#define PT_KEYGEN	"keygen"
#define PT_SCRIPT	"script"
#define PT_NOSCRIPT	"noscript"
#define PT_NOEMBED	"noembed"
#define PT_HEAD		"head"
#define PT_HTML		"html"
#define PT_SERVER	"server"
#define PT_CERTIFICATE	"certificate"
#define PT_STRIKE	"s"
#define PT_UNDERLINE	"u"
#define PT_SPACER	"spacer"
#define PT_MULTICOLUMN	"multicol"
#define PT_NSCP_CLOSE	"nscp_close"
#define PT_NSCP_OPEN	"nscp_open"
#define PT_ILAYER       "ilayer"
#define PT_LAYER        "layer"
#define PT_NOLAYER      "nolayer"
#define PT_STYLE	"style"
#define PARAM_MEDIA	"media"
#define PT_MQUOTE   	"mquote"
#define PT_OBJECT	"object"
#define PT_SPAN		"span"
#define PT_SPELL    "spell"
#define PT_INLINEINPUT    "inlineinput"
#define PT_NSCP_REBLOCK	"nscp_reblock"
#define PT_NSDT     "nsdt"
#define PT_INLINEINPUTTHICK "jean"
#define PT_INLINEINPUTDOTTED "charles"
#ifdef SHACK
#define PT_BUILTIN      "builtin"
#endif

/***************
 * Unique IDs to identify
 * all the above tag elements.
 * P_TEXT is for text that
 * is not part of a tag.
 * NB: these must fit in an int8
 * (see include/structs.h)
 ***************/
#define P_UNKNOWN       -1
#define P_TEXT          0
#define P_TITLE         1
#define P_INDEX         2
#define P_BASE          3
#define P_LINK          4
#define P_HEADER_1      5
#define P_HEADER_2      6
#define P_HEADER_3      7
#define P_HEADER_4      8
#define P_HEADER_5      9
#define P_HEADER_6      10
#define P_ANCHOR        11
#define P_PARAGRAPH     12
#define P_ADDRESS       13
#define P_IMAGE         14
#define P_PLAIN_TEXT    15
#define P_PLAIN_PIECE   16
#define P_PREFORMAT     17
#define P_LISTING_TEXT  18
#define P_UNUM_LIST     19
#define P_NUM_LIST      20
#define P_MENU          21
#define P_DIRECTORY     22
#define P_LIST_ITEM     23
#define P_DESC_LIST     24
#define P_DESC_TITLE    25
#define P_DESC_TEXT     26
#define P_STRIKEOUT     27
#define P_FIXED         28
#define P_BOLD          29
#define P_ITALIC        30
#define P_EMPHASIZED    31
#define P_STRONG        32
#define P_CODE          33
#define P_SAMPLE        34
#define P_KEYBOARD      35
#define P_VARIABLE      36
#define P_CITATION      37
#define P_BLOCKQUOTE    38
#define P_FORM          39
#define P_INPUT         40
#define P_SELECT        41
#define P_OPTION        42
#define P_TEXTAREA      43
#define P_HRULE         44
#define P_LINEBREAK     45
#define P_WORDBREAK     46
#define P_NOBREAK       47
#define P_BASEFONT      48
#define P_FONT          49
#define P_BLINK		50
#define P_NEW_IMAGE	51
#define P_CENTER        52
#define P_SUBDOC        53
#define P_CELL          54
#define P_TABLE         55
#define P_CAPTION       56
#define P_TABLE_ROW     57
#define P_TABLE_HEADER  58
#define P_TABLE_DATA    59
#define P_EMBED         60
#define P_BODY          61
#define P_META          62
#define P_COLORMAP      63
#define P_HYPE          64
#define P_BIG           65
#define P_SMALL         66
#define P_SUPER         67
#define P_SUB           68
#define P_GRID          69
#define P_GRID_CELL     70
#define P_NOGRIDS       71
#define P_JAVA_APPLET   72
#define P_PARAM         73
#define P_MAP		74
#define P_AREA		75
#define P_DIVISION	76
#define P_KEYGEN	77
#define P_SCRIPT	78
#define P_NOSCRIPT	79
#define P_NOEMBED	80
#define P_HEAD		81
#define P_HTML		82
#define P_SERVER	83
#define P_CERTIFICATE	84
#define P_ILAYER        85
#define P_STRIKE	86
#define P_UNDERLINE	87
#define P_SPACER	88
#define P_MULTICOLUMN	89
#define P_NSCP_CLOSE	90
#define P_NSCP_OPEN	91
#define P_BLOCK		92
#define P_LAYER		93
#define P_STYLE		94
#define P_MQUOTE	95
#define P_SPAN		96
#define P_SPELL		97
#define P_INLINEINPUT	98
#define P_NSCP_REBLOCK	99
#define P_OBJECT	100
#define P_NSDT  	101
#define P_INLINEINPUTTHICK  102
#define P_INLINEINPUTDOTTED 103
#define P_NOLAYER       104
#ifdef SHACK
#define P_BUILTIN	105
#define P_MAX		106
#else
#define P_MAX		105
#endif

/*****************
 * These define all the strings that match
 * to all the parameters that can occur in MDL tag elements.
 * Note, these strings
 * MUST be in all lower case.  The actual language
 * has them as caseless, but for optimization purposes
 * all these are assumed to already be all lower case.
 *******************/
#define PARAM_TEXT		"text"
#define PARAM_HREF		"href"
#define PARAM_PRE		"pre" /* prefetch value */
#define PARAM_SRC		"src"
#define PARAM_LOWSRC		"lowsrc"
#define PARAM_ALT		"alt"
#define PARAM_SUPPRESS          "suppress"
#define PARAM_NAME		"name"
#define PARAM_SIZE		"size"
#define PARAM_POINT_SIZE	"point-size"
#define PARAM_FONT_WEIGHT	"font-weight"
#define PARAM_ALIGN		"align"
#define PARAM_VALIGN		"valign"
#define PARAM_NOSHADE		"noshade"
#define PARAM_WIDTH		"width"
#define PARAM_HEIGHT		"height"
#define PARAM_TYPE		"type"
#define PARAM_START		"start"
#define PARAM_VALUE		"value"
#define PARAM_BORDER		"border"
#define PARAM_VSPACE		"vspace"
#define PARAM_HSPACE		"hspace"
#define PARAM_CLEAR		"clear"
#define PARAM_ACTION		"action"
#define PARAM_METHOD		"method"
#define PARAM_MAXLENGTH		"maxlength"
#define PARAM_CHECKED		"checked"
#define PARAM_ROWS		"rows"
#define PARAM_COLS		"cols"
#define PARAM_MULTIPLE		"multiple"
#define PARAM_SELECTED		"selected"
#define PARAM_PROMPT		"prompt"
#define PARAM_COMPACT		"compact"
#define PARAM_ISMAP		"ismap"
#define PARAM_ROWSPAN		"rowspan"
#define PARAM_COLSPAN		"colspan"
#define PARAM_WRAP		"wrap"
#define PARAM_NOWRAP		"nowrap"
#define PARAM_TABSTOP		"tabstop"
#define PARAM_FOREGROUND	"foreground"
#define PARAM_BACKGROUND	"background"
#define PARAM_BGCOLOR		"bgcolor"
#define PARAM_COLOR		"color"
#define PARAM_LINK		"link"
#define PARAM_VLINK		"vlink"
#define PARAM_ALINK		"alink"
#define PARAM_CELLPAD		"cellpadding"
#define PARAM_TOPPAD		"toppadding"
#define PARAM_BOTTOMPAD		"bottompadding"
#define PARAM_LEFTPAD		"leftpadding"
#define PARAM_RIGHTPAD		"rightpadding"
#define PARAM_CELLSPACE		"cellspacing"
#define PARAM_HTTP_EQUIV	"http-equiv"
#define PARAM_CONTENT		"content"
#define PARAM_TARGET		"target"
#define PARAM_ENCODING		"enctype"
#define PARAM_SHAPE		"shape"
#define PARAM_COORDS		"coords"
#define PARAM_USEMAP		"usemap"
#define PARAM_SCROLLING		"scrolling"
#define PARAM_NORESIZE		"noresize"
#define PARAM_MARGINWIDTH	"marginwidth"
#define PARAM_MARGINHEIGHT	"marginheight"
#define PARAM_CODE		"code"
#define PARAM_CODEBASE		"codebase"
#define PARAM_ARCHIVE		"archive"
#define PARAM_MAYSCRIPT		"mayscript"
#define PARAM_LANGUAGE		"language"
#define PARAM_VARIABLE		"variable"
#define PARAM_HIDDEN		"hidden"
#define PARAM_VISIBILITY	"visibility"
#define PARAM_GUTTER		"gutter"
#define PARAM_LEFT		"left"
#define PARAM_TOP		"top"
#define PARAM_ZINDEX            "z-index"
#define PARAM_MATCH             "match"
#define PARAM_GROUP		"group"
#define PARAM_ABOVE		"above"
#define PARAM_BELOW		"below"
#define PARAM_LOCKED            "locked"
#define PARAM_DIRECT		"direct"
#define PARAM_FRAMEBORDER	"frameborder"
#define PARAM_BORDERCOLOR	"bordercolor"
#define PARAM_FACE		"face"
#define PARAM_CLASS		"class"
#define PARAM_ID		"id"
#define PARAM_STYLE		"style"
#define PARAM_OVERFLOW          "overflow"
#define PARAM_X                 "x"
#define PARAM_PAGEX             "pagex"
#define PARAM_Y                 "y"
#define PARAM_PAGEY             "pagey"
#define PARAM_CLIP		"clip"
#define PARAM_CODETYPE		"codetype"
#define PARAM_CLASSID		"classid"
#define PARAM_DATA			"data"
#define PARAM_REL			"rel"
#define PARAM_SMALL_BM_ICON	"small_bookmark_icon"
#define PARAM_LARGE_BM_ICON	"large_bookmark_icon"
#define PARAM_MAIL		"mail"
#define PARAM_NOSAVE  		"nosave"
#define PARAM_LOCALDATA "localdata"
#define PARAM_DISABLED		"disabled"
#define PARAM_READONLY		"readonly"

/* Mocha event handler names */
#define PARAM_ONLOAD		"onload"
#define PARAM_ONUNLOAD		"onunload"
#define PARAM_ONSCROLL		"onscroll"
#define PARAM_ONFOCUS		"onfocus"
#define PARAM_ONBLUR		"onblur"
#define PARAM_ONSELECT		"onselect"
#define PARAM_ONCHANGE		"onchange"
#define PARAM_ONRESET		"onreset"
#define PARAM_ONSUBMIT		"onsubmit"
#define PARAM_ONCLICK		"onclick"
#define PARAM_ONMOUSEOVER	"onmouseover"
#define PARAM_ONMOUSEOUT	"onmouseout"
#define PARAM_ONLOCATE		"onlocate"
#define PARAM_ONABORT		"onabort"
#define PARAM_ONERROR		"onerror"
#define PARAM_ONHELP		"onhelp"
#define PARAM_ONMOUSEDOWN	"onmousedown"
#define PARAM_ONMOUSEUP		"onmouseup"
#define PARAM_ONDBLCLICK    	"ondblclick"
#define PARAM_ONKEYDOWN		"onkeydown"
#define PARAM_ONKEYUP		"onkeyup"
#define PARAM_ONKEYPRESS	"onkeypress"
#define PARAM_ONDRAGDROP	"ondragdrop"
#define PARAM_ONMOVE		"onmove"
#define PARAM_ONRESIZE		"onresize"

/* Security (keygen) attributes */
#define PARAM_CHALLENGE		"challenge"
#define PARAM_PQG		"pqg"
#define PARAM_KEYTYPE		"keytype"
