/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2001
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
#ifndef nsIMediaList_h___
#define nsIMediaList_h___

#include "nsISupportsArray.h"

// IID for the nsIMediaList interface {c8c2bbce-1dd1-11b2-a108-f7290a0e6da2}
#define NS_IMEDIA_LIST_IID \
{0xc8c2bbce, 0x1dd1, 0x11b2, {0xa1, 0x08, 0xf7, 0x29, 0x0a, 0x0e, 0x6d, 0xa2}}

class nsIMediaList : public nsISupportsArray {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMEDIA_LIST_IID)

  NS_IMETHOD GetText(nsAString& aMediaText) = 0;
  NS_IMETHOD SetText(const nsAString& aMediaText) = 0;
  NS_IMETHOD MatchesMedium(nsIAtom* aMedium, PRBool* aMatch) = 0;
  NS_IMETHOD DropReference(void) = 0;
};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIMEDIALIST \
  NS_IMETHOD GetText(nsAString& aMediaText); \
  NS_IMETHOD SetText(const nsAString& aMediaText); \
  NS_IMETHOD MatchesMedium(nsIAtom* aMedium, PRBool* aMatch); \
  NS_IMETHOD DropReference(void);

nsresult
NS_NewMediaList(nsISupportsArray* aArray, nsICSSStyleSheet* aSheet,
                nsIMediaList** aInstancePtrResult);
#endif /* nsICSSLoader_h___ */
