/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsDOMError.h"
#include "nsGkAtoms.h"
#include "nsDOMString.h"
#include "nsContentUtils.h"

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
    return false;
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

  virtual nsIDOMNode* AsDOMNode() { return this; }

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
NS_INTERFACE_MAP_BEGIN(nsDocumentFragment)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNodeSelector,
                                 new nsNodeSelectorTearoff(this))
  // nsNodeSH::PreCreate() depends on the identity pointer being the
  // same as nsINode (which nsIContent inherits), so if you change the
  // below line, make sure nsNodeSH::PreCreate() still does the right
  // thing!
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DocumentFragment)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_INHERITED(nsDocumentFragment, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsDocumentFragment, nsGenericElement)

NS_IMPL_ELEMENT_CLONE(nsDocumentFragment)
