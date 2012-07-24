/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMDocumentFragment.
 */

#include "nsIDOMDocumentFragment.h"
#include "mozilla/dom/FragmentOrElement.h"
#include "nsGenericElement.h" // for DOMCI_NODE_DATA
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsDOMError.h"
#include "nsGkAtoms.h"
#include "nsDOMString.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsDocumentFragment : public FragmentOrElement,
                           public nsIDOMDocumentFragment
{
public:
  using FragmentOrElement::GetFirstChild;

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // interface nsIDOMNode
  NS_FORWARD_NSIDOMNODE(FragmentOrElement::)

  // interface nsIDOMDocumentFragment
  // NS_DECL_NSIDOCUMENTFRAGMENT  Empty

  nsDocumentFragment(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~nsDocumentFragment()
  {
  }

  // nsIContent
  virtual already_AddRefed<nsINodeInfo>
    GetExistingAttrNameFromQName(const nsAString& aStr) const
  {
    return nullptr;
  }

  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nullptr, aValue, aNotify);
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
  virtual bool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
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
    return nullptr;
  }
  virtual PRUint32 GetAttrCount() const
  {
    return 0;
  }

  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }

  virtual nsIAtom* DoGetID() const;
  virtual nsIAtom *GetIDAttributeName() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers)
  {
    NS_ASSERTION(false, "Trying to bind a fragment to a tree");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual void UnbindFromTree(bool aDeep, bool aNullParent)
  {
    NS_ASSERTION(false, "Trying to unbind a fragment from a tree");
    return;
  }

  virtual Element* GetNameSpaceElement()
  {
    return nullptr;
  }

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent, bool aDumpAll) const;
#endif

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
                                           nullptr, kNameSpaceID_None,
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
  : FragmentOrElement(aNodeInfo)
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() ==
                    nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
                    mNodeInfo->Equals(nsGkAtoms::documentFragmentNodeName,
                                      kNameSpaceID_None),
                    "Bad NodeType in aNodeInfo");
}

bool
nsDocumentFragment::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eDOCUMENT_FRAGMENT));
}

nsIAtom*
nsDocumentFragment::DoGetID() const
{
  return nullptr;  
}

nsIAtom*
nsDocumentFragment::GetIDAttributeName() const
{
  return nullptr;
}

#ifdef DEBUG
void
nsDocumentFragment::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) {
    fputs("  ", out);
  }

  fprintf(out, "DocumentFragment@%p", (void *)this);

  fprintf(out, " flags=[%08x]", static_cast<unsigned int>(GetFlags()));
  fprintf(out, " refcount=%d<", mRefCnt.get());

  nsIContent* child = GetFirstChild();
  if (child) {
    fputs("\n", out);

    for (; child; child = child->GetNextSibling()) {
      child->List(out, aIndent + 1);
    }

    for (indent = aIndent; --indent >= 0; ) {
      fputs("  ", out);
    }
  }

  fputs(">\n", out);
}

void
nsDocumentFragment::DumpContent(FILE* out, PRInt32 aIndent,
                                bool aDumpAll) const
{
  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) {
    fputs("  ", out);
  }

  fputs("<DocumentFragment>", out);

  if(aIndent) {
    fputs("\n", out);
  }

  for (nsIContent* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    PRInt32 indent = aIndent ? aIndent + 1 : 0;
    child->DumpContent(out, indent, aDumpAll);
  }
  for (indent = aIndent; --indent >= 0; ) {
    fputs("  ", out);
  }
  fputs("</DocumentFragment>", out);

  if(aIndent) {
    fputs("\n", out);
  }
}
#endif


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

NS_IMPL_ADDREF_INHERITED(nsDocumentFragment, FragmentOrElement)
NS_IMPL_RELEASE_INHERITED(nsDocumentFragment, FragmentOrElement)

NS_IMPL_ELEMENT_CLONE(nsDocumentFragment)
