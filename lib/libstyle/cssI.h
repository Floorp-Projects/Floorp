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
#ifndef __CSS_INTERNAL_H__
#define __CSS_INTERNAL_H__

typedef struct css_nodeRecord {
    int node_id;
    char * string;
    struct css_nodeRecord * left;
    struct css_nodeRecord * right;
} css_nodeRecord, *css_node;

enum {
  NODE_IMPORT_LIST = 300,
  NODE_IMPORT_STRING,
  NODE_IMPORT_URL,
  NODE_STRING,
  NODE_NUMBER,
  NODE_TERM,
  NODE_EXPR,
  NODE_PRIO,
  NODE_IDENT,
  NODE_EMS,
  NODE_LENGTH,
  NODE_PERCENTAGE,
  NODE_RGB,
  NODE_URL,
  NODE_HEXCOLOR,
  NODE_DECLARATION_PROPERTY_EXPR_PRIO,
  NODE_DECLARATION_PROPERTY_EXPR,
  NODE_ID_SELECTOR,
  NODE_PSEUDO_ELEMENT,
  NODE_CLASS,
  NODE_ACTIVE_PSCLASS,
  NODE_VISITED_PSCLASS,
  NODE_LINK_PSCLASS,
  NODE_SIMPLE_SELECTOR_ID_SELECTOR,
  NODE_SIMPLE_SELECTOR_DOT_AND_CLASS,
  NODE_SIMPLE_SELECTOR_NAME_PSEUDO_CLASS,
  NODE_SIMPLE_SELECTOR_NAME_ONLY,
  NODE_SIMPLE_SELECTOR_NAME_AND_CLASS,
  NODE_SIMPLE_SELECTOR_NAME_CLASS_PSEUDO_CLASS,
  NODE_SIMPLE_SELECTOR_LIST,
  NODE_SELECTOR,
  NODE_SELECTOR_CONTEXTUAL,
  NODE_DECLARATION_LIST,
  NODE_SELECTOR_LIST,
  NODE_PROPERTY,
  NODE_TERM_OP,
  NODE_EXPR_OP,
  NODE_EMPTY_OP,
  NODE_UNARY_OP,
  NODE_RULESET,
  NODE_RULESET_LIST,
  NODE_SIMPLE_SELECTOR_PSEUDO_CLASS,
  NODE_ELEMENT_NAME,
  NODE_WILD,
  NODE_FONTDEF_LIST
};

extern css_node css_tree_root;         /* root of parse tree */
#ifdef LEX
extern unsigned char css_text[];
#else 
extern char *css_text;
#endif

extern int  css_lex(void);
extern int  css_error(const char * diagnostic);
extern int  css_parse(void);
extern int  css_wrap(void);
extern void  css_GetBuf(char * buf, int * result, int max_to_read);
extern void css_FreeNode(css_node node);

#endif
