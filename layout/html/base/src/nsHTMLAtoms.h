/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsHTMLAtoms_h___
#define nsHTMLAtoms_h___

#include "nsIAtom.h"

#define NS_HTML_BASE_HREF   "_base_href"
#define NS_HTML_BASE_TARGET "_base_target"

/**
 * This class wraps up the creation (and destruction) of the standard
 * set of html atoms used during normal html handling. This objects
 * are created when the first html content object is created and they
 * are destroyed when the last html content object is destroyed.
 */
class nsHTMLAtoms {
public:

  static void AddrefAtoms();
  static void ReleaseAtoms();

  static nsIAtom* mozAnonymousBlock;
  static nsIAtom* mozListBulletPseudo;

  // Special attribute atoms
  static nsIAtom* _baseHref;
  static nsIAtom* _baseTarget;

  // Alphabetical list of html attribute atoms
  static nsIAtom* a;
  static nsIAtom* abbr;
  static nsIAtom* above;
  static nsIAtom* accept;
  static nsIAtom* acceptcharset;
  static nsIAtom* accesskey;
  static nsIAtom* action;
  static nsIAtom* align;
  static nsIAtom* alink;
  static nsIAtom* alt;
  static nsIAtom* applet;
  static nsIAtom* archive;
  static nsIAtom* area;
  static nsIAtom* axis;

  static nsIAtom* background;
  static nsIAtom* below;
  static nsIAtom* bgcolor;
  static nsIAtom* blockFrame;
  static nsIAtom* body;
  static nsIAtom* border;
  static nsIAtom* bordercolor;
  static nsIAtom* bottompadding;
  static nsIAtom* br;
  static nsIAtom* button;
  static nsIAtom* buttonContentPseudo;

  static nsIAtom* caption;
  static nsIAtom* cellContentPseudo;
  static nsIAtom* cellpadding;
  static nsIAtom* cellspacing;
  static nsIAtom* ch;
  static nsIAtom* _char;
  static nsIAtom* charoff;
  static nsIAtom* charset;
  static nsIAtom* checked;
  static nsIAtom* choff;
  static nsIAtom* cite;
  static nsIAtom* kClass;
  static nsIAtom* classid;
  static nsIAtom* clear;
  static nsIAtom* clip;
  static nsIAtom* code;
  static nsIAtom* codebase;
  static nsIAtom* codetype;
  static nsIAtom* color;
  static nsIAtom* col;
  static nsIAtom* colgroup;
  static nsIAtom* cols;
  static nsIAtom* colspan;
  static nsIAtom* columnPseudo;
  static nsIAtom* commentPseudo;
  static nsIAtom* compact;
  static nsIAtom* content;
  static nsIAtom* coords;

  static nsIAtom* data;
  static nsIAtom* datetime;
  static nsIAtom* declare;
  static nsIAtom* defer;
  static nsIAtom* defaultchecked;
  static nsIAtom* defaultselected;
  static nsIAtom* defaultvalue;
  static nsIAtom* dir;
  static nsIAtom* disabled;
  static nsIAtom* div;
  static nsIAtom* dl;
  static nsIAtom* dropDownVisible;
  static nsIAtom* dropDownHidden;
  static nsIAtom* dropDownBtnOut;
  static nsIAtom* dropDownBtnPressed;

  static nsIAtom* embed;
  static nsIAtom* encoding;
  static nsIAtom* enctype;

  static nsIAtom* face;
  static nsIAtom* fieldset;
  static nsIAtom* fieldsetContentPseudo;
  static nsIAtom* fileTextStylePseudo;
  static nsIAtom* fileButtonStylePseudo;
  static nsIAtom* firstLetterPseudo;
  static nsIAtom* firstLinePseudo;
  static nsIAtom* font;
  static nsIAtom* fontWeight;
  static nsIAtom* _for;
  static nsIAtom* form;
  static nsIAtom* frame;
  static nsIAtom* frameborder;
  static nsIAtom* frameset;
  static nsIAtom* framesetBlankPseudo;

  static nsIAtom* gutter;

  static nsIAtom* h1;
  static nsIAtom* h2;
  static nsIAtom* h3;
  static nsIAtom* h4;
  static nsIAtom* h5;
  static nsIAtom* h6;
  static nsIAtom* headerContentBase;
  static nsIAtom* headerContentLanguage;
  static nsIAtom* headerContentScriptType;
  static nsIAtom* headerContentStyleType;
  static nsIAtom* headerContentType;
  static nsIAtom* headerDefaultStyle;
  static nsIAtom* headerWindowTarget;
  static nsIAtom* headers;
  static nsIAtom* height;
  static nsIAtom* hidden;
  static nsIAtom* horizontalFramesetBorderPseudo;
  static nsIAtom* hr;
  static nsIAtom* href;
  static nsIAtom* hreflang;
  static nsIAtom* hspace;
  static nsIAtom* html;
  static nsIAtom* httpEquiv;

  static nsIAtom* ibPseudo;
  static nsIAtom* id;
  static nsIAtom* iframe;
  static nsIAtom* img;
  static nsIAtom* index;
  static nsIAtom* inlineFrame;
  static nsIAtom* input;
  static nsIAtom* ismap;

  static nsIAtom* label;
  static nsIAtom* labelContentPseudo;
  static nsIAtom* lang;
  static nsIAtom* layout;
  static nsIAtom* li;
  static nsIAtom* link;
  static nsIAtom* left;
  static nsIAtom* leftpadding;
  static nsIAtom* legend;
  static nsIAtom* legendContentPseudo;
  static nsIAtom* length;
  static nsIAtom* longdesc;
  static nsIAtom* lowsrc;

  static nsIAtom* marginheight;
  static nsIAtom* marginwidth;
  static nsIAtom* maxlength;
  static nsIAtom* mayscript;
  static nsIAtom* media;
  static nsIAtom* menu;
  static nsIAtom* method;
  static nsIAtom* multicol;
  static nsIAtom* multiple;

  static nsIAtom* name;
  static nsIAtom* nohref;
  static nsIAtom* noresize;
  static nsIAtom* noshade;
  static nsIAtom* nowrap;

  static nsIAtom* object;
  static nsIAtom* ol;
  static nsIAtom* onabort;
  static nsIAtom* onblur;
  static nsIAtom* onchange;
  static nsIAtom* onclick;
  static nsIAtom* ondblclick;
  static nsIAtom* ondragdrop;
  static nsIAtom* onerror;
  static nsIAtom* onfocus;
  static nsIAtom* onkeydown;
  static nsIAtom* onkeypress;
  static nsIAtom* onkeyup;
  static nsIAtom* onload;
  static nsIAtom* onmousedown;
  static nsIAtom* onmousemove;
  static nsIAtom* onmouseover;
  static nsIAtom* onmouseout;
  static nsIAtom* onmouseup;
  static nsIAtom* onpaint;
  static nsIAtom* onreset;
  static nsIAtom* onsubmit;
  static nsIAtom* onunload;
  static nsIAtom* overflow;

  static nsIAtom* p;
  static nsIAtom* pagex;
  static nsIAtom* pagey;
  static nsIAtom* param;
  static nsIAtom* placeholderPseudo;
  static nsIAtom* pointSize;
  static nsIAtom* pre;
  static nsIAtom* profile;
  static nsIAtom* prompt;

  static nsIAtom* readonly;
  static nsIAtom* rel;
  static nsIAtom* repeat;
  static nsIAtom* rev;
  static nsIAtom* rightpadding;
  static nsIAtom* rootPseudo;
  static nsIAtom* rows;
  static nsIAtom* rowspan;
  static nsIAtom* rules;

  static nsIAtom* scheme;
  static nsIAtom* scope;
  static nsIAtom* script;
  static nsIAtom* scrolledContentPseudo;
  static nsIAtom* scrolling;
  static nsIAtom* select;
  static nsIAtom* selected;
  static nsIAtom* selectedindex;
  static nsIAtom* shape;
  static nsIAtom* size;
  static nsIAtom* spacer;
  static nsIAtom* span;
  static nsIAtom* src;
  static nsIAtom* standby;
  static nsIAtom* start;
  static nsIAtom* style;
  static nsIAtom* summary;
  static nsIAtom* suppress;

  static nsIAtom* tabindex;
  static nsIAtom* table;
  static nsIAtom* tablePseudo;
  static nsIAtom* tableCellPseudo;
  static nsIAtom* tableColGroupPseudo;
  static nsIAtom* tableColPseudo;
  static nsIAtom* tableOuterPseudo;
  static nsIAtom* tableRowGroupPseudo;
  static nsIAtom* tableRowPseudo;
  static nsIAtom* tabstop;
  static nsIAtom* target;
  static nsIAtom* tbody;
  static nsIAtom* td;
  static nsIAtom* tfoot;
  static nsIAtom* thead;
  static nsIAtom* text;
  static nsIAtom* textarea;
  static nsIAtom* textPseudo;
  static nsIAtom* th;
  static nsIAtom* title;
  static nsIAtom* top;
  static nsIAtom* toppadding;
  static nsIAtom* tr;
  static nsIAtom* type;

  static nsIAtom* ul;
  static nsIAtom* usemap;

  static nsIAtom* valign;
  static nsIAtom* value;
  static nsIAtom* valuetype;
  static nsIAtom* variable;
  static nsIAtom* version;
  static nsIAtom* verticalFramesetBorderPseudo;
  static nsIAtom* visibility;
  static nsIAtom* vlink;
  static nsIAtom* vspace;

  static nsIAtom* wbr;
  static nsIAtom* width;
  static nsIAtom* wrap;
  static nsIAtom* wrappedFramePseudo;

  static nsIAtom* zindex;
};

#endif /* nsHTMLAtoms_h___ */
