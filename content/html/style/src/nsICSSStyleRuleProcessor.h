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
#ifndef nsICSSStyleRuleProcessor_h___
#define nsICSSStyleRuleProcessor_h___

#include "nsIStyleRuleProcessor.h"

class nsICSSStyleSheet;

// IID for the nsICSSStyleRuleProcessor interface {98bf169c-7b7c-11d3-ba05-001083023c2b}
#define NS_ICSS_STYLE_RULE_PROCESSOR_IID     \
{0x98bf169c, 0x7b7c, 0x11d3, {0xba, 0x05, 0x00, 0x10, 0x83, 0x02, 0x3c, 0x2b}}

/* The CSS style rule processor provides a mechanism for sibling style sheets
 * to combine their rule processing in order to allow proper cascading to happen.
 *
 * When queried for a rule processor, a CSS style sheet will append itself to 
 * the previous CSS processor if present, and return nsnull. Otherwise it will
 * create a new processor for itself.
 *
 * CSS style rule processors keep a live reference on all style sheets bound to them
 * The CSS style sheets keep a weak reference on all the processors that they are
 * bound to (many to many). The CSS style sheet is told when the rule processor is 
 * going away (via DropRuleProcessorReference).
 */
class nsICSSStyleRuleProcessor: public nsIStyleRuleProcessor {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICSS_STYLE_RULE_PROCESSOR_IID; return iid; }

  NS_IMETHOD  AppendStyleSheet(nsICSSStyleSheet* aStyleSheet) = 0;

  NS_IMETHOD  ClearRuleCascades(void) = 0;
};

#endif /* nsICSSStyleRuleProcessor_h___ */
