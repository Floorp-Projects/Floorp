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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
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

/*
 * A base class which implements nsIStyleSheetLinkingElement and can
 * be subclassed by various content nodes that want to load
 * stylesheets (<style>, <link>, processing instructions, etc).
 */

#ifndef nsStyleLinkElement_h___
#define nsStyleLinkElement_h___

#include "nsCOMPtr.h"
#include "nsIDOMLinkStyle.h"
#include "nsIStyleSheetLinkingElement.h"
#include "nsIStyleSheet.h"
#include "nsIURI.h"

class nsIDocument;
class nsStringArray;

class nsStyleLinkElement : public nsIDOMLinkStyle,
                           public nsIStyleSheetLinkingElement
{
public:
  nsStyleLinkElement();
  virtual ~nsStyleLinkElement();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr) = 0;

  // nsIDOMLinkStyle
  NS_DECL_NSIDOMLINKSTYLE

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet);
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet);
  NS_IMETHOD InitStyleLinkElement(PRBool aDontLoadStyle);
  NS_IMETHOD UpdateStyleSheet(nsICSSLoaderObserver* aObserver,
                              PRBool* aWillNotify,
                              PRBool* aIsAlternate);
  NS_IMETHOD SetEnableUpdates(PRBool aEnableUpdates);
  NS_IMETHOD GetCharset(nsAString& aCharset);

  virtual void OverrideBaseURI(nsIURI* aNewBaseURI);
  virtual void SetLineNumber(PRUint32 aLineNumber);

  static void ParseLinkTypes(const nsAString& aTypes, nsStringArray& aResult);

protected:
  /**
   * @param aOldDocument should be non-null only if we're updating because we
   *                     removed the node from the document.
   * @param aForceUpdate PR_TRUE will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   */
  nsresult UpdateStyleSheetInternal(nsIDocument *aOldDocument,
                                    PRBool aForceUpdate = PR_FALSE);

  virtual void GetStyleSheetURL(PRBool* aIsInline,
                                nsIURI** aURI) = 0;
  virtual void GetStyleSheetInfo(nsAString& aTitle,
                                 nsAString& aType,
                                 nsAString& aMedia,
                                 PRBool* aIsAlternate) = 0;

private:
  /**
   * @param aOldDocument should be non-null only if we're updating because we
   *                     removed the node from the document.
   * @param aForceUpdate PR_TRUE will force the update even if the URI has not
   *                     changed.  This should be used in cases when something
   *                     about the content that affects the resulting sheet
   *                     changed but the URI may not have changed.
   */
  nsresult DoUpdateStyleSheet(nsIDocument *aOldDocument,
                              nsICSSLoaderObserver* aObserver,
                              PRBool* aWillNotify,
                              PRBool* aIsAlternate,
                              PRBool aForceUpdate);

protected:
  nsCOMPtr<nsIStyleSheet> mStyleSheet;
  PRPackedBool mDontLoadStyle;
  PRPackedBool mUpdatesEnabled;
  PRUint32 mLineNumber;
};

#endif /* nsStyleLinkElement_h___ */

