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
#ifndef nsIStyleSheetLinkingElement_h__
#define nsIStyleSheetLinkingElement_h__


#include "nsISupports.h"

class nsIParser;
class nsIDocument;

#define NS_ISTYLESHEETLINKINGELEMENT_IID          \
  {0xa6cf90e9, 0x15b3, 0x11d2,                    \
  {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

class nsIStyleSheet;

class nsIStyleSheetLinkingElement : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ISTYLESHEETLINKINGELEMENT_IID)

  /**
   * Used to make the association between a style sheet and
   * the element that linked it to the document.
   *
   * @param aStyleSheet the style sheet associated with this
   *                    element.
   */
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet) = 0;

  /**
   * Used to obtain the style sheet linked in by this element.
   *
   * @param aStyleSheet out parameter that returns the style
   *                    sheet associated with this element.
   */
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet) = 0;

  /**
   * Initialize the stylesheet linking element. This method passes
   * in a parser that the element blocks if the stylesheet is
   * a stylesheet that should be loaded with the parser blocked.
   * If aDontLoadStyle is true the element will ignore the first
   * modification to the element that would cause a stylesheet to
   * be loaded. Subsequent modifications to the element will not
   * be ignored.
   */
  NS_IMETHOD InitStyleLinkElement(nsIParser *aParser, PRBool aDontLoadStyle) = 0;

  /**
   * Tells this element to update the stylesheet.
   *
   * @param aNotify if true we notify the document of the
   *                removal of the stylesheet.
   * @param aOldDocument the document that this element was part
   *                     of (nsnull if we're not moving the element
   *                     from one document to another).
   * @param aDocIndex index of the stylesheet in the document's
   *                  stylesheet list. -1 means we'll look up the
   *                  index from the position of the element.
   */
  NS_IMETHOD UpdateStyleSheet(PRBool aNotify,
                              nsIDocument *aOldDocument,
                              PRInt32 aDocIndex) = 0;

  /**
   * Tells this element wether to update the stylesheet when the
   * element's properties change.
   *
   * @param aEnableUpdates update on changes or not.
   */
  NS_IMETHOD SetEnableUpdates(PRBool aEnableUpdates) = 0;

  /**
   * Gets the charset that the element claims the style sheet is in
   *
   * @param aCharset the charset
   */
  NS_IMETHOD GetCharset(nsAString& aCharset) = 0;
};

#endif // nsILinkingElement_h__
