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
 * The Original Code is Mozilla Communicator client code.
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

#ifndef nsDOMDocumentType_h___
#define nsDOMDocumentType_h___

#include "nsIDOMDocumentType.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsString.h"
#include "nsISizeOfHandler.h"

class nsDOMDocumentType : public nsIContent,
                          public nsIDOMDocumentType
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
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMDocumentType
  NS_DECL_NSIDOMDOCUMENTTYPE

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

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

extern nsresult
NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                      const nsAReadableString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations,
                      const nsAReadableString& aPublicId,
                      const nsAReadableString& aSystemId,
                      const nsAReadableString& aInternalSubset);

#endif // nsDOMDocument_h___
