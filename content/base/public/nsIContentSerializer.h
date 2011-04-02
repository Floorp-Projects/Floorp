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

#ifndef nsIContentSerializer_h
#define nsIContentSerializer_h

#include "nsISupports.h"

class nsIContent;
class nsIDocument;
class nsAString;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

#define NS_ICONTENTSERIALIZER_IID \
{ 0xb1ee32f2, 0xb8c4, 0x49b9, \
  { 0x93, 0xdf, 0xb6, 0xfa, 0xb5, 0xd5, 0x46, 0x88 } }

class nsIContentSerializer : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICONTENTSERIALIZER_IID)

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  const char* aCharSet, PRBool aIsCopying,
                  PRBool aIsWholeDocument) = 0;

  NS_IMETHOD AppendText(nsIContent* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendCDATASection(nsIContent* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAString& aStr) = 0;

  NS_IMETHOD AppendProcessingInstruction(nsIContent* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAString& aStr) = 0;

  NS_IMETHOD AppendComment(nsIContent* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAString& aStr) = 0;

  NS_IMETHOD AppendDoctype(nsIContent *aDoctype,
                           nsAString& aStr) = 0;

  NS_IMETHOD AppendElementStart(mozilla::dom::Element* aElement,
                                mozilla::dom::Element* aOriginalElement,
                                nsAString& aStr) = 0;

  NS_IMETHOD AppendElementEnd(mozilla::dom::Element* aElement,
                              nsAString& aStr) = 0;

  NS_IMETHOD Flush(nsAString& aStr) = 0;

  /**
   * Append any items in the beginning of the document that won't be 
   * serialized by other methods. XML declaration is the most likely 
   * thing this method can produce.
   */
  NS_IMETHOD AppendDocumentStart(nsIDocument *aDocument,
                                 nsAString& aStr) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIContentSerializer, NS_ICONTENTSERIALIZER_IID)

#define NS_CONTENTSERIALIZER_CONTRACTID_PREFIX \
"@mozilla.org/layout/contentserializer;1?mimetype="

#endif /* nsIContentSerializer_h */
