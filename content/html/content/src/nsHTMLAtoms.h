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

#define NS_HTML_BASE_HREF   "_BASE_HREF"
#define NS_HTML_BASE_TARGET "_BASE_TARGET"

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

  // Special attribute atoms
  static nsIAtom* _baseHref;
  static nsIAtom* _baseTarget;

  // Alphabetical list of html attribute atoms
  static nsIAtom* a;
  static nsIAtom* above;
  static nsIAtom* accept;
  static nsIAtom* acceptcharset;
  static nsIAtom* accesskey;
  static nsIAtom* action;
  static nsIAtom* active;
  static nsIAtom* align;
  static nsIAtom* alink;
  static nsIAtom* alt;
  static nsIAtom* applet;
  static nsIAtom* archive;

  static nsIAtom* background;
  static nsIAtom* below;
  static nsIAtom* bgcolor;
  static nsIAtom* body;
  static nsIAtom* border;
  static nsIAtom* bordercolor;
  static nsIAtom* bottompadding;
  static nsIAtom* br;

  static nsIAtom* cellpadding;
  static nsIAtom* cellspacing;
  static nsIAtom* checked;
  static nsIAtom* kClass;
  static nsIAtom* classid;
  static nsIAtom* clear;
  static nsIAtom* clip;
  static nsIAtom* code;
  static nsIAtom* codebase;
  static nsIAtom* color;
  static nsIAtom* cols;
  static nsIAtom* colspan;
  static nsIAtom* columnPseudo;
  static nsIAtom* compact;
  static nsIAtom* coords;

  static nsIAtom* data;
  static nsIAtom* dir;
  static nsIAtom* disabled;
  static nsIAtom* div;
  static nsIAtom* dl;

  static nsIAtom* embed;
  static nsIAtom* encoding;
  static nsIAtom* enctype;

  static nsIAtom* face;
  static nsIAtom* font;
  static nsIAtom* fontWeight;
  static nsIAtom* form;
  static nsIAtom* frame;
  static nsIAtom* frameborder;
  static nsIAtom* frameset;

  static nsIAtom* gutter;

  static nsIAtom* h1;
  static nsIAtom* h2;
  static nsIAtom* h3;
  static nsIAtom* h4;
  static nsIAtom* h5;
  static nsIAtom* h6;
  static nsIAtom* height;
  static nsIAtom* hidden;
  static nsIAtom* hover;
  static nsIAtom* href;
  static nsIAtom* hspace;
  static nsIAtom* httpEquiv;

  static nsIAtom* id;
  static nsIAtom* iframe;
  static nsIAtom* img;
  static nsIAtom* ismap;

  static nsIAtom* lang;
  static nsIAtom* li;
  static nsIAtom* link;
  static nsIAtom* left;
  static nsIAtom* leftpadding;
  static nsIAtom* longdesc;
  static nsIAtom* lowsrc;

  static nsIAtom* marginheight;
  static nsIAtom* marginwidth;
  static nsIAtom* maxlength;
  static nsIAtom* mayscript;
  static nsIAtom* menu;
  static nsIAtom* method;
  static nsIAtom* multicol;
  static nsIAtom* multiple;

  static nsIAtom* name;
  static nsIAtom* noresize;
  static nsIAtom* noshade;
  static nsIAtom* nowrap;

  static nsIAtom* object;
  static nsIAtom* ol;
  static nsIAtom* onabort;
  static nsIAtom* onblur;
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
  static nsIAtom* onunload;
  static nsIAtom* outOfDate;
  static nsIAtom* overflow;

  static nsIAtom* p;
  static nsIAtom* pagex;
  static nsIAtom* pagey;
  static nsIAtom* pointSize;
  static nsIAtom* pre;
  static nsIAtom* prompt;

  static nsIAtom* readonly;
  static nsIAtom* rel;
  static nsIAtom* repeat;
  static nsIAtom* rightpadding;
  static nsIAtom* rootContentPseudo;
  static nsIAtom* rows;
  static nsIAtom* rowspan;

  static nsIAtom* scrolling;
  static nsIAtom* selected;
  static nsIAtom* shape;
  static nsIAtom* size;
  static nsIAtom* span;
  static nsIAtom* src;
  static nsIAtom* start;
  static nsIAtom* style;
  static nsIAtom* summary;
  static nsIAtom* suppress;

  static nsIAtom* tabindex;
  static nsIAtom* table;
  static nsIAtom* tabstop;
  static nsIAtom* target;
  static nsIAtom* td;
  static nsIAtom* text;
  static nsIAtom* th;
  static nsIAtom* title;
  static nsIAtom* top;
  static nsIAtom* toppadding;
  static nsIAtom* type;

  static nsIAtom* ul;
  static nsIAtom* usemap;

  static nsIAtom* valign;
  static nsIAtom* value;
  static nsIAtom* variable;
  static nsIAtom* visibility;
  static nsIAtom* visited;
  static nsIAtom* vlink;
  static nsIAtom* vspace;

  static nsIAtom* width;
  static nsIAtom* wrap;

  static nsIAtom* zindex;
};

#endif /* nsHTMLAtoms_h___ */
