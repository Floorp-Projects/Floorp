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
 *   Daniel Glazman <glazman@netscape.com>
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
#ifndef nsCSSDeclaration_h___
#define nsCSSDeclaration_h___

#include "nsISupports.h"
#include "nsColor.h"
#include <stdio.h>
#include "nsString.h"
#include "nsCoord.h"
#include "nsCSSValue.h"
#include "nsCSSProps.h"
#include "nsVoidArray.h"
#include "nsValueArray.h"
#include "nsCSSStruct.h"

class nsCSSDeclaration {
public:
  nsCSSDeclaration(void);
  nsCSSDeclaration(const nsCSSDeclaration& aCopy);

public:
  NS_DECL_ZEROING_OPERATOR_NEW

  nsCSSStruct* GetData(const nsID& aSID);
  nsCSSStruct* EnsureData(const nsID& aSID);

  nsresult AppendValue(nsCSSProperty aProperty, const nsCSSValue& aValue);
  nsresult AppendStructValue(nsCSSProperty aProperty, void* aStruct);
  nsresult SetValueImportant(nsCSSProperty aProperty);
  nsresult AppendComment(const nsAString& aComment);
  nsresult RemoveProperty(nsCSSProperty aProperty, nsCSSValue& aValue);

  nsresult GetValue(nsCSSProperty aProperty, nsCSSValue& aValue);
  nsresult GetValue(nsCSSProperty aProperty, nsAString& aValue);
  nsresult GetValue(const nsAString& aProperty, nsAString& aValue);

  nsCSSDeclaration* GetImportantValues();
  PRBool GetValueIsImportant(nsCSSProperty aProperty);
  PRBool GetValueIsImportant(const nsAString& aProperty);

  PRUint32 Count();
  nsresult GetNthProperty(PRUint32 aIndex, nsAString& aReturn);

  nsChangeHint GetStyleImpact() const;

  nsresult ToString(nsAString& aString);

  nsCSSDeclaration* Clone() const;

#ifdef DEBUG
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif
  
protected:
  nsresult RemoveProperty(nsCSSProperty aProperty);

private:
  nsresult GetValueOrImportantValue(nsCSSProperty aProperty, nsCSSValue& aValue);
  void     AppendImportanceToString(PRBool aIsImportant, nsAString& aString);
  PRBool   AppendValueToString(nsCSSProperty aProperty, nsAString& aResult);
  PRBool   AppendValueOrImportantValueToString(nsCSSProperty aProperty, nsAString& aResult);
  PRBool   AppendValueToString(nsCSSProperty aProperty, const nsCSSValue& aValue, nsAString& aResult);
  nsCSSDeclaration& operator=(const nsCSSDeclaration& aCopy);
  PRBool operator==(const nsCSSDeclaration& aCopy) const;

  void   PropertyIsSet(PRInt32 & aPropertyIndex, PRInt32 aIndex, PRUint32 & aSet, PRUint32 aValue);
  PRBool TryBorderShorthand(nsAString & aString, PRUint32 aPropertiesSet,
                            PRInt32 aBorderTopWidth,
                            PRInt32 aBorderTopStyle,
                            PRInt32 aBorderTopColor,
                            PRInt32 aBorderBottomWidth,
                            PRInt32 aBorderBottomStyle,
                            PRInt32 aBorderBottomColor,
                            PRInt32 aBorderLeftWidth,
                            PRInt32 aBorderLeftStyle,
                            PRInt32 aBorderLeftColor,
                            PRInt32 aBorderRightWidth,
                            PRInt32 aBorderRightStyle,
                            PRInt32 aBorderRightColor);
  PRBool  TryBorderSideShorthand(nsAString & aString,
                                 nsCSSProperty  aShorthand,
                                 PRInt32 aBorderWidth,
                                 PRInt32 aBorderStyle,
                                 PRInt32 aBorderColor);
  PRBool  TryFourSidesShorthand(nsAString & aString,
                                nsCSSProperty aShorthand,
                                PRInt32 & aTop,
                                PRInt32 & aBottom,
                                PRInt32 & aLeft,
                                PRInt32 & aRight,
                                PRBool aClearIndexes);
  void  DoClipShorthand(nsAString & aString,
                        PRInt32 aTop,
                        PRInt32 aBottom,
                        PRInt32 aLeft,
                        PRInt32 aRight);
  void  TryBackgroundShorthand(nsAString & aString,
                               PRInt32 & aBgColor, PRInt32 & aBgImage,
                               PRInt32 & aBgRepeat, PRInt32 & aBgAttachment,
                               PRInt32 & aBgPositionX,
                               PRInt32 & aBgPositionY);
  void  UseBackgroundPosition(nsAString & aString,
                              PRInt32 & aBgPositionX,
                              PRInt32 & aBgPositionY);

  PRBool   AllPropertiesSameImportance(PRInt32 aFirst, PRInt32 aSecond,
                                       PRInt32 aThird, PRInt32 aFourth,
                                       PRInt32 aFifth, PRInt32 aSixth,
                                       PRBool & aImportance);
  PRBool   AllPropertiesSameValue(PRInt32 aFirst, PRInt32 aSecond,
                                  PRInt32 aThird, PRInt32 aFourth);
  void     AppendPropertyAndValueToString(nsCSSProperty aProperty,
                                          nsAString& aResult);

protected:
    //
    // Specialized ref counting.
    // We do not want everyone to ref count us, only the rules which hold
    //  onto us (our well defined lifetime is when the last rule releases
    //  us).
    // It's worth a comment here that the main nsCSSDeclaration is refcounted,
    //  but it's |mImportant| is not refcounted, but just owned by the
    //  non-important declaration.
    //
    friend class CSSStyleRuleImpl;
    void AddRef(void) {
      mRuleRefs++;
    }
    void Release(void) {
      NS_ASSERTION(0 < mRuleRefs, "bad Release");
      if (0 == --mRuleRefs) {
        delete this;
      }
    }
public:
    void RuleAbort(void) {
      NS_ASSERTION(0 == mRuleRefs, "bad RuleAbort");
      delete this;
    }
protected:
    //
    // Block everyone, except us or a derivitive, from deleting us.
    //
  ~nsCSSDeclaration(void);
    

private:
    nsValueArray* mOrder;
    nsCSSDeclaration* mImportant;
    nsSmallVoidArray mStructs;

    //
    // Keep these two together, as they should pack.
    //
    nsCSSDeclRefCount mRuleRefs;
    nsCSSDeclContains mContains;
};


nsresult
NS_NewCSSDeclaration(nsCSSDeclaration** aInstancePtrResult);


#endif /* nsCSSDeclaration_h___ */
