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
#ifndef nsIStyleSheet_h___
#define nsIStyleSheet_h___

#include <stdio.h>
#include "nsISupports.h"

class nsIAtom;
class nsString;
class nsIURI;
class nsIStyleRule;
class nsISupportsArray;
class nsPresContext;
class nsIContent;
class nsIDocument;
class nsIStyleRuleProcessor;

// IID for the nsIStyleSheet interface
// 6fbfb2cb-a1c0-4576-9354-a4af4e0029ad
#define NS_ISTYLE_SHEET_IID     \
{0x6fbfb2cb, 0xa1c0, 0x4576, {0x93, 0x54, 0xa4, 0xaf, 0x4e, 0x00, 0x29, 0xad}}

/**
 * A style sheet is a thing associated with a document that has style
 * rules.  Those style rules can be reached in one of two ways, depending
 * on which level of the nsStyleSet it is in:
 *   1) It can be |QueryInterface|d to nsIStyleRuleProcessor
 *   2) It can be |QueryInterface|d to nsICSSStyleSheet, with which the
 *      |nsStyleSet| uses an |nsCSSRuleProcessor| to access the rules.
 */
class nsIStyleSheet : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLE_SHEET_IID)

  // basic style sheet data
  NS_IMETHOD GetURL(nsIURI*& aURL) const = 0;
  NS_IMETHOD GetTitle(nsString& aTitle) const = 0;
  NS_IMETHOD GetType(nsString& aType) const = 0;
  NS_IMETHOD GetMediumCount(PRInt32& aCount) const = 0;
  NS_IMETHOD GetMediumAt(PRInt32 aIndex, nsIAtom*& aMedium) const = 0;
  NS_IMETHOD_(PRBool) UseForMedium(nsIAtom* aMedium) const = 0;
  NS_IMETHOD_(PRBool) HasRules() const = 0;

  /**
   * Whether the sheet is applicable.  A sheet that is not applicable
   * should never be inserted into a style set.  A sheet may not be
   * applicable for a variety of reasons including being disabled and
   * being incomplete.
   *
   */
  NS_IMETHOD GetApplicable(PRBool& aApplicable) const = 0;

  /**
   * Set the stylesheet to be enabled.  This may or may not make it
   * applicable.
   */
  NS_IMETHOD SetEnabled(PRBool aEnabled) = 0;

  /**
   * Whether the sheet is complete.
   */
  NS_IMETHOD GetComplete(PRBool& aComplete) const = 0;
  NS_IMETHOD SetComplete() = 0;

  // style sheet owner info
  NS_IMETHOD GetParentSheet(nsIStyleSheet*& aParent) const = 0;  // may be null
  NS_IMETHOD GetOwningDocument(nsIDocument*& aDocument) const = 0; // may be null
  NS_IMETHOD SetOwningDocument(nsIDocument* aDocument) = 0;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
#endif
};

#endif /* nsIStyleSheet_h___ */
