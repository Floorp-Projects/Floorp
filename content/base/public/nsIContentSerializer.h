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

#ifndef _nsIContentSerializer_h__
#define _nsIContentSerializer_h__

#include "nsISupports.h"
#include "nsAWritableString.h"
#include "nsIAtom.h"

class nsIDOMText; /* forward declaration */
class nsIDOMCDATASection; /* forward declaration */
class nsIDOMProcessingInstruction; /* forward declaration */
class nsIDOMComment; /* forward declaration */
class nsIDOMDocumentType; /* forward declaration */
class nsIDOMElement; /* forward declaration */

/* starting interface:    nsIContentSerializer */
#define NS_ICONTENTSERIALIZER_IID_STR "61e9b9a3-d30c-429e-b0cf-ade73466df06"

#define NS_ICONTENTSERIALIZER_IID \
  {0x61e9b9a3, 0xd30c, 0x429e, \
    { 0xb0, 0xcf, 0xad, 0xe7, 0x34, 0x66, 0xdf, 0x06 }}

class nsIContentSerializer : public nsISupports {
 public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICONTENTSERIALIZER_IID)

  NS_IMETHOD Init(PRUint32 flags, PRUint32 aWrapColumn,
                  nsIAtom* aCharSet) = 0;

  NS_IMETHOD AppendText(nsIDOMText* aText, PRInt32 aStartOffset,
                        PRInt32 aEndOffset, nsAWritableString& aStr) = 0;

  NS_IMETHOD AppendCDATASection(nsIDOMCDATASection* aCDATASection,
                                PRInt32 aStartOffset, PRInt32 aEndOffset,
                                nsAWritableString& aStr) = 0;

  NS_IMETHOD AppendProcessingInstruction(nsIDOMProcessingInstruction* aPI,
                                         PRInt32 aStartOffset,
                                         PRInt32 aEndOffset,
                                         nsAWritableString& aStr) = 0;

  NS_IMETHOD AppendComment(nsIDOMComment* aComment, PRInt32 aStartOffset,
                           PRInt32 aEndOffset, nsAWritableString& aStr) = 0;

  NS_IMETHOD AppendDoctype(nsIDOMDocumentType *aDoctype,
                           nsAWritableString& aStr) = 0;

  NS_IMETHOD AppendElementStart(nsIDOMElement *aElement,
                                nsAWritableString& aStr) = 0;

  NS_IMETHOD AppendElementEnd(nsIDOMElement *aElement,
                              nsAWritableString& aStr) = 0;

  NS_IMETHOD Flush(nsAWritableString& aStr) = 0;
};

#define NS_CONTENTSERIALIZER_CONTRACTID_PREFIX \
"@mozilla.org/layout/contentserializer;1?mimetype="

#endif /* __gen_nsIContentSerializer_h__ */
