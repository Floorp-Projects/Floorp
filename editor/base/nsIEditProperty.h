/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsIEditProperty_h__
#define __nsIEditProperty_h__

#include "nsISupports.h"

class nsIAtom;
class nsString;

#define NS_IEDITPROPERTY_IID \
{/* 9875cd40-ca81-11d2-8f4d-006008159b0c*/ \
0x9875cd40, 0xca81, 0x11d2, \
{0x8f, 0x4d, 0x0, 0x60, 0x8, 0x15, 0x9b, 0x0c} }

/** simple interface for describing a single property as it relates to a range of content.
  *
  */

class nsIEditProperty : public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IEDITPROPERTY_IID; return iid; }

public:

/* we're still trying to decide how edit atoms will work.  Until then, use these */
// XXX: fix ASAP!
  // inline tags
  static nsIAtom *b;         
  static nsIAtom *big;       
  static nsIAtom *i;         
  static nsIAtom *small;     
  static nsIAtom *strike;    
  static nsIAtom *sub;       
  static nsIAtom *sup;       
  static nsIAtom *tt;        
  static nsIAtom *u;         
  static nsIAtom *em;        
  static nsIAtom *strong;    
  static nsIAtom *dfn;       
  static nsIAtom *code;      
  static nsIAtom *samp;      
  static nsIAtom *kbd;       
  static nsIAtom *var;       
  static nsIAtom *cite;      
  static nsIAtom *abbr;      
  static nsIAtom *acronym;   
  static nsIAtom *font;      
  static nsIAtom *a;         
  static nsIAtom *href;         
  static nsIAtom *img;       
  static nsIAtom *object;    
  static nsIAtom *br;        
  static nsIAtom *script;    
  static nsIAtom *map;       
  static nsIAtom *q;         
  static nsIAtom *span;      
  static nsIAtom *bdo;       
  static nsIAtom *input;     
  static nsIAtom *select;    
  static nsIAtom *textarea;  
  static nsIAtom *label;     
  static nsIAtom *button;   
  // Block tags
  static nsIAtom *p;         
  static nsIAtom *div;       
  static nsIAtom *blockquote;
  static nsIAtom *h1;        
  static nsIAtom *h2;        
  static nsIAtom *h3;        
  static nsIAtom *h4;        
  static nsIAtom *h5;        
  static nsIAtom *h6;        
  static nsIAtom *ul;        
  static nsIAtom *ol;        
  static nsIAtom *dl;        
  static nsIAtom *pre;       
  static nsIAtom *noscript;  
  static nsIAtom *form;      
  static nsIAtom *hr;        
  static nsIAtom *table;     
  static nsIAtom *fieldset;  
  static nsIAtom *address;   
  // Assumed to be block:
  static nsIAtom *body;      
  static nsIAtom *tr;        
  static nsIAtom *td;        
  static nsIAtom *th;        
  static nsIAtom *caption;   
  static nsIAtom *col;       
  static nsIAtom *colgroup;  
  static nsIAtom *tbody;     
  static nsIAtom *thead;     
  static nsIAtom *tfoot;     
  static nsIAtom *li;        
  static nsIAtom *dt;        
  static nsIAtom *dd;        
  static nsIAtom *legend;    

  /** properties **/
  static nsIAtom *color;     
  static nsIAtom *face;      
  static nsIAtom *size;      

  /** special strings */
  static nsString *allProperties;   // this magic string represents the union of all inline style tags

  // XXX: end temp code




};

extern nsresult NS_NewEditProperty(nsIEditProperty **aResult);

#endif
