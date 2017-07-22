/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Core's nsIDOMText node.
 */

#include "nsTextNode.h"
#include "mozilla/dom/TextBinding.h"
#include "nsContentUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsStubMutationObserver.h"
#include "mozilla/IntegerPrintfMacros.h"
#ifdef DEBUG
#include "nsRange.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

/**
 * class used to implement attr() generated content
 */
class nsAttributeTextNode final : public nsTextNode,
                                  public nsStubMutationObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsAttributeTextNode(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                      int32_t aNameSpaceID,
                      nsIAtom* aAttrName) :
    nsTextNode(aNodeInfo),
    mGrandparent(nullptr),
    mNameSpaceID(aNameSpaceID),
    mAttrName(aAttrName)
  {
    NS_ASSERTION(mNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");
    NS_ASSERTION(mAttrName, "Must have attr name");
  }

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;

  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  virtual nsGenericDOMDataNode *CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                              bool aCloneText) const override
  {
    already_AddRefed<mozilla::dom::NodeInfo> ni =
      RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
    nsAttributeTextNode *it = new nsAttributeTextNode(ni,
                                                      mNameSpaceID,
                                                      mAttrName);
    if (it && aCloneText) {
      it->mText = mText;
    }

    return it;
  }

  // Public method for the event to run
  void UpdateText() {
    UpdateText(true);
  }

private:
  virtual ~nsAttributeTextNode() {
    NS_ASSERTION(!mGrandparent, "We were not unbound!");
  }

  // Update our text to our parent's current attr value
  void UpdateText(bool aNotify);

  // This doesn't need to be a strong pointer because it's only non-null
  // while we're bound to the document tree, and it points to an ancestor
  // so the ancestor must be bound to the document tree the whole time
  // and can't be deleted.
  nsIContent* mGrandparent;
  // What attribute we're showing
  int32_t mNameSpaceID;
  nsCOMPtr<nsIAtom> mAttrName;
};

nsTextNode::~nsTextNode()
{
}

NS_IMPL_ISUPPORTS_INHERITED(nsTextNode, nsGenericDOMDataNode, nsIDOMNode,
                            nsIDOMText, nsIDOMCharacterData)

JSObject*
nsTextNode::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TextBinding::Wrap(aCx, this, aGivenProto);
}

bool
nsTextNode::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | eTEXT | eDATA_NODE));
}

nsGenericDOMDataNode*
nsTextNode::CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo, bool aCloneText) const
{
  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  nsTextNode *it = new nsTextNode(ni);
  if (aCloneText) {
    it->mText = mText;
  }

  return it;
}

nsresult
nsTextNode::AppendTextForNormalize(const char16_t* aBuffer, uint32_t aLength,
                                   bool aNotify, nsIContent* aNextSibling)
{
  CharacterDataChangeInfo::Details details = {
    CharacterDataChangeInfo::Details::eMerge, aNextSibling
  };
  return SetTextInternal(mText.GetLength(), 0, aBuffer, aLength, aNotify, &details);
}

nsresult
nsTextNode::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                       nsIContent* aBindingParent, bool aCompileEventHandlers)
{
  nsresult rv = nsGenericDOMDataNode::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  SetDirectionFromNewTextNode(this);

  return NS_OK;
}

void nsTextNode::UnbindFromTree(bool aDeep, bool aNullParent)
{
  ResetDirectionSetByTextNode(this);

  nsGenericDOMDataNode::UnbindFromTree(aDeep, aNullParent);
}

#ifdef DEBUG
void
nsTextNode::List(FILE* out, int32_t aIndent) const
{
  int32_t index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Text@%p", static_cast<const void*>(this));
  fprintf(out, " flags=[%08x]", static_cast<unsigned int>(GetFlags()));
  if (IsCommonAncestorForRangeInSelection()) {
    const nsTHashtable<nsPtrHashKey<nsRange>>* ranges =
      GetExistingCommonAncestorRanges();
    fprintf(out, " ranges:%d", ranges ? ranges->Count() : 0);
  }
  fprintf(out, " primaryframe=%p", static_cast<void*>(GetPrimaryFrame()));
  fprintf(out, " refcount=%" PRIuPTR "<", mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void
nsTextNode::DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const
{
  if(aDumpAll) {
    int32_t index;
    for (index = aIndent; --index >= 0; ) fputs("  ", out);

    nsAutoString tmp;
    ToCString(tmp, 0, mText.GetLength());

    if(!tmp.EqualsLiteral("\\n")) {
      fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
      if(aIndent) fputs("\n", out);
    }
  }
}
#endif

nsresult
NS_NewAttributeContent(nsNodeInfoManager *aNodeInfoManager,
                       int32_t aNameSpaceID, nsIAtom* aAttrName,
                       nsIContent** aResult)
{
  NS_PRECONDITION(aNodeInfoManager, "Missing nodeInfoManager");
  NS_PRECONDITION(aAttrName, "Must have an attr name");
  NS_PRECONDITION(aNameSpaceID != kNameSpaceID_Unknown, "Must know namespace");

  *aResult = nullptr;

  already_AddRefed<mozilla::dom::NodeInfo> ni = aNodeInfoManager->GetTextNodeInfo();

  nsAttributeTextNode* textNode = new nsAttributeTextNode(ni,
                                                          aNameSpaceID,
                                                          aAttrName);
  NS_ADDREF(*aResult = textNode);

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsAttributeTextNode, nsTextNode,
                            nsIMutationObserver)

nsresult
nsAttributeTextNode::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                bool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent && aParent->GetParent(),
                  "This node can't be a child of the document or of the document root");

  nsresult rv = nsTextNode::BindToTree(aDocument, aParent,
                                       aBindingParent, aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(!mGrandparent, "We were already bound!");
  mGrandparent = aParent->GetParent();
  mGrandparent->AddMutationObserver(this);

  // Note that there is no need to notify here, since we have no
  // frame yet at this point.
  UpdateText(false);

  return NS_OK;
}

void
nsAttributeTextNode::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // UnbindFromTree can be called anytime so we have to be safe.
  if (mGrandparent) {
    // aNullParent might not be true here, but we want to remove the
    // mutation observer anyway since we only need it while we're
    // in the document.
    mGrandparent->RemoveMutationObserver(this);
    mGrandparent = nullptr;
  }
  nsTextNode::UnbindFromTree(aDeep, aNullParent);
}

void
nsAttributeTextNode::AttributeChanged(nsIDocument* aDocument,
                                      Element* aElement,
                                      int32_t aNameSpaceID,
                                      nsIAtom* aAttribute,
                                      int32_t aModType,
                                      const nsAttrValue* aOldValue)
{
  if (aNameSpaceID == mNameSpaceID && aAttribute == mAttrName &&
      aElement == mGrandparent) {
    // Since UpdateText notifies, do it when it's safe to run script.  Note
    // that if we get unbound while the event is up that's ok -- we'll just
    // have no grandparent when it fires, and will do nothing.
    void (nsAttributeTextNode::*update)() = &nsAttributeTextNode::UpdateText;
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("nsAttributeTextNode::AttributeChanged", this, update));
  }
}

void
nsAttributeTextNode::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_ASSERTION(aNode == static_cast<nsINode*>(mGrandparent), "Wrong node!");
  mGrandparent = nullptr;
}

void
nsAttributeTextNode::UpdateText(bool aNotify)
{
  if (mGrandparent) {
    nsAutoString attrValue;
    mGrandparent->GetAttr(mNameSpaceID, mAttrName, attrValue);
    SetText(attrValue, aNotify);
  }
}

