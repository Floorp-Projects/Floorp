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

/*
 * Implementation of DOM Core's nsIDOMDocumentFragment.
 */

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsGenericElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMAttr.h"
#include "nsDOMError.h"
#include "nsGkAtoms.h"
#include "nsDOMString.h"
#include "nsIDOMUserDataHandler.h"

class nsDocumentFragment : public nsGenericElement,
                           public nsIDOMDocumentFragment
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // interface nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericElement::)

  // interface nsIDOMDocumentFragment
  // NS_DECL_NSIDOCUMENTFRAGMENT  Empty

  nsDocumentFragment(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsDocumentFragment()
  {
  }

  // nsIContent
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify)
  {
    return NS_OK;
  }
  virtual bool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                         nsAString& aResult) const
  {
    return PR_FALSE;
  }
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                             bool aNotify)
  {
    return NS_OK;
  }
  virtual const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const
  {
    return nsnull;
  }

  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIAtom* DoGetID() const;
  virtual nsIAtom *GetIDAttributeName() const;

protected:
  nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
};

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsNodeInfoManager *aNodeInfoManager)
{
  NS_ENSURE_ARG(aNodeInfoManager);

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nodeInfo = aNodeInfoManager->GetNodeInfo(nsGkAtoms::documentFragmentNodeName,
                                           nsnull, kNameSpaceID_None,
                                           nsIDOMNode::DOCUMENT_FRAGMENT_NODE);
  NS_ENSURE_TRUE(nodeInfo, NS_ERROR_OUT_OF_MEMORY);

  nsDocumentFragment *it = new nsDocumentFragment(nodeInfo.forget());
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = it);

  return NS_OK;
}

nsDocumentFragment::nsDocumentFragment(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsGenericElement(aNodeInfo)
{
  ClearIsElement();
}

bool
nsDocumentFragment::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eDOCUMENT_FRAGMENT));
}

nsIAtom*
nsDocumentFragment::DoGetID() const
{
  return nsnull;  
}

nsIAtom*
nsDocumentFragment::GetIDAttributeName() const
{
  return nsnull;
}

DOMCI_NODE_DATA(DocumentFragment, nsDocumentFragment)

// QueryInterface implementation for nsDocumentFragment
NS_INTERFACE_TABLE_HEAD(nsDocumentFragment)
  NS_NODE_INTERFACE_TABLE2(nsDocumentFragment, nsIDOMNode,
                           nsIDOMDocumentFragment)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DocumentFragment)
NS_INTERFACE_MAP_END_INHERITING(nsGenericElement)


NS_IMPL_ADDREF_INHERITED(nsDocumentFragment, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsDocumentFragment, nsGenericElement)

NS_IMPL_ELEMENT_CLONE(nsDocumentFragment)
