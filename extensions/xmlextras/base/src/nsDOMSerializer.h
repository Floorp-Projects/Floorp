/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDOMSerializer_h__
#define nsDOMSerializer_h__

#include "nsIDOMSerializer.h"
#include "nsISecurityCheckedComponent.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsString.h"

class nsIDOMElement;
class nsIDOMDocumentType;
class nsIDOMText;
class nsIDOMCDATASection;
class nsIDOMComment;
class nsIDOMProcessingInstruction;
class nsIUnicodeEncoder;

class nsDOMSerializer : public nsIDOMSerializer,
                        public nsISecurityCheckedComponent
{
public:
  nsDOMSerializer();
  virtual ~nsDOMSerializer();

  NS_DECL_ISUPPORTS

  // nsIDOMSerializer
  NS_IMETHOD SerializeToString(nsIDOMNode *root, PRUnichar **_retval);
  NS_IMETHOD SerializeToStream(nsIDOMNode *root, 
                               nsIOutputStream *stream, 
                               const char *charset);

  NS_DECL_NSISECURITYCHECKEDCOMPONENT

protected:
  void SerializeText(nsIDOMText* aText, nsString& aStr);
  void SerializeCDATASection(nsIDOMCDATASection* aCDATASection, nsString& aStr);
  void SerializeProcessingInstruction(nsIDOMProcessingInstruction* aPI,
                                      nsString& aStr);
  void SerializeComment(nsIDOMComment* aComment, nsString& aStr);
  void SerializeDoctype(nsIDOMDocumentType* aDoctype, nsString& aStr);
  void PushNameSpaceDecl(nsString& aPrefix,
                         nsString& aURI,
                         nsIDOMElement* aOwner);
  void PopNameSpaceDeclsFor(nsIDOMElement* aOwner);
  PRBool ConfirmPrefix(nsString& aPrefix,
                       nsString& aURI);
  void SerializeAttr(nsString& aPrefix,
                     nsString& aName,
                     nsString& aValue,
                     nsString& aStr);
  void SerializeElementStart(nsIDOMElement* aElement, nsString& aStr);
  void SerializeElementEnd(nsIDOMElement* aElement, nsString& aStr);
  nsresult SerializeNodeStart(nsIDOMNode* aNode, nsString& aStr);
  nsresult SerializeNodeEnd(nsIDOMNode* aNode, nsString& aStr);
  nsresult SerializeToStringRecursive(nsIDOMNode* aNode, nsString& aStr);
  nsresult SerializeToStreamRecursive(nsIDOMNode* aNode, 
                                      nsIOutputStream* aStream,
                                      nsIUnicodeEncoder* aEncoder);

protected:
  PRInt32 mPrefixIndex;
  nsVoidArray mNameSpaceStack;
};


#endif

