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
#ifndef nsIStyleRule_h___
#define nsIStyleRule_h___

#include <stdio.h>

#include "nsISupports.h"

class nsIStyleSheet;
class nsPresContext;
class nsIContent;
struct nsRuleData;

// IID for the nsIStyleRule interface {40ae5c90-ad6a-11d1-8031-006008159b5a}
#define NS_ISTYLE_RULE_IID     \
{0x40ae5c90, 0xad6a, 0x11d1, {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

/**
 * An object implementing |nsIStyleRule| (henceforth, a rule) represents
 * immutable stylistic information that either applies or does not apply
 * to a given element.  It belongs to an object or group of objects that
 * implement |nsIStyleSheet| and |nsIStyleRuleProcessor| (henceforth, a
 * sheet).
 *
 * A rule becomes relevant to the computation of style data when
 * |nsIStyleRuleProcessor::RulesMatching| creates a rule node that
 * points to the rule.  (A rule node, |nsRuleNode|, is a node in the
 * rule tree, which is a lexicographic tree indexed by rules.  The path
 * from the root of the rule tree to the |nsRuleNode| for a given
 * |nsStyleContext| contains exactly the rules that match the element
 * that the style context is for, in priority (weight, origin,
 * specificity) order.)
 *
 * The computation of style data uses the rule tree, which calls
 * |nsIStyleRule::MapRuleInfoInto| below.
 *
 * It is worth emphasizing that the data represented by a rule
 * implementation are immutable.  When the data need to be changed, a
 * new rule object must be created.  Failing to do this will lead to
 * bugs in the handling of dynamic style changes, since the rule tree
 * caches the results of |MapRuleInfoInto|.
 *
 * |nsIStyleRule| objects are owned by |nsRuleNode| objects (in addition
 * to typically being owned by their sheet), which are in turn garbage
 * collected (with the garbage collection roots being style contexts).
 */


class nsIStyleRule : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLE_RULE_IID)

  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const = 0;

  /**
   * |nsIStyleRule::MapRuleInfoInto| is a request to copy all stylistic
   * data represented by the rule that:
   *   + are relevant for |aRuleData->mSID| (the style struct ID)
   *   + are not already filled into the data struct
   * into the appropriate data struct in |aRuleData|.  It is important
   * that only empty data are filled in, since the rule tree is walked
   * from highest priority rule to least, so that the walk can stop if
   * all needed data are found.  Thus overwriting non-empty data will
   * break CSS cascading rules.
   */
  NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData)=0;

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
#endif
};


#endif /* nsIStyleRule_h___ */
