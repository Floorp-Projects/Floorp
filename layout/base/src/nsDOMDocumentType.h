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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDOMDocumentType_h___
#define nsDOMDocumentType_h___

#include "nsIDOMDocumentType.h"
#include "nsIScriptObjectOwner.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsString.h"
#include "nsISizeOfHandler.h"

class nsIDOMNamedNodeMap;

class nsDOMDocumentType : public nsIDOMDocumentType,
                          public nsIScriptObjectOwner,
                          public nsIContent
{
public:
  nsDOMDocumentType(const nsAReadableString& aName,
                    nsIDOMNamedNodeMap *aEntities,
                    nsIDOMNamedNodeMap *aNotations,
                    const nsAReadableString& aPublicId,
                    const nsAReadableString& aSystemId,
                    const nsAReadableString& aInternalSubset);

  virtual ~nsDOMDocumentType();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMDocumentType
  NS_DECL_IDOMDOCUMENTTYPE

  // nsIScriptObjectOwner interface
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC_DOM_DATA(mInner);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  // XXX DocumentType is currently implemented by using the generic
  // CharacterData inner object, even though DocumentType is not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mName;
  nsIDOMNamedNodeMap* mEntities;
  nsIDOMNamedNodeMap* mNotations;
  nsString mPublicId;
  nsString mSystemId;
  nsString mInternalSubset;
};

extern nsresult NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                                      const nsAReadableString& aName,
                                      nsIDOMNamedNodeMap *aEntities,
                                      nsIDOMNamedNodeMap *aNotations,
                                      const nsAReadableString& aPublicId,
                                      const nsAReadableString& aSystemId,
                                      const nsAReadableString& aInternalSubset);

#endif // nsDOMDocument_h___
