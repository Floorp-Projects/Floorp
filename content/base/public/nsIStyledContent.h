/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIStyledContent_h___
#define nsIStyledContent_h___

#include "nsIContent.h"
#include "nsChangeHint.h"

class nsString;
class nsICSSStyleRule;
class nsISupportsArray;
class nsRuleWalker;
class nsAttrValue;

// IID for the nsIStyledContent class
#define NS_ISTYLEDCONTENT_IID   \
{ 0x13cc12f1, 0x2892, 0x47e4, { 0xaa, 0x77, 0x24, 0xc, 0xa7, 0x49, 0x82, 0xde } };

// Abstract interface for all styled content (that supports ID, CLASS, STYLE, and
// the ability to specify style hints on an attribute change).
class nsIStyledContent : public nsIContent {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLEDCONTENT_IID)

  // XXX Currently callers (e.g., CSSStyleSheetImpl) assume that the ID
  // corresponds to the attribute nsHTMLAtoms::id and that the Class
  // corresponds to the attribute nsHTMLAtoms::kClass.  If this becomes
  // incorrect, then new methods need to be added here.
  NS_IMETHOD GetID(nsIAtom** aResult) const = 0;
  virtual const nsAttrValue* GetClasses() const = 0;
  NS_IMETHOD_(PRBool) HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const = 0;

  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) = 0;

  virtual nsICSSStyleRule* GetInlineStyleRule() = 0;
  NS_IMETHOD SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify) = 0;

  /**
   * Is the attribute named stored in the mapped attributes?
   *
   * This really belongs on nsIHTMLContent instead.
   */
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const = 0;


  /**
   * Get a hint that tells the style system what to do when 
   * an attribute on this node changes, if something needs to happen
   * in response to the change *other* than the result of what is
   * mapped into style data via any type of style rule.
   */
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute,
                                              PRInt32 aModType) const = 0;

};

#endif /* nsIStyledContent_h___ */
