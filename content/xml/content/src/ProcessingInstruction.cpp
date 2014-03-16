/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsUnicharUtils.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/ProcessingInstructionBinding.h"
#include "mozilla/dom/XMLStylesheetProcessingInstruction.h"
#include "nsContentUtils.h"

already_AddRefed<mozilla::dom::ProcessingInstruction>
NS_NewXMLProcessingInstruction(nsNodeInfoManager *aNodeInfoManager,
                               const nsAString& aTarget,
                               const nsAString& aData)
{
  using mozilla::dom::ProcessingInstruction;
  using mozilla::dom::XMLStylesheetProcessingInstruction;

  NS_PRECONDITION(aNodeInfoManager, "Missing nodeinfo manager");

  nsCOMPtr<nsIAtom> target = do_GetAtom(aTarget);
  MOZ_ASSERT(target);

  if (target == nsGkAtoms::xml_stylesheet) {
    nsRefPtr<XMLStylesheetProcessingInstruction> pi =
      new XMLStylesheetProcessingInstruction(aNodeInfoManager, aData);
    return pi.forget();
  }

  nsCOMPtr<nsINodeInfo> ni;
  ni = aNodeInfoManager->GetNodeInfo(nsGkAtoms::processingInstructionTagName,
                                     nullptr, kNameSpaceID_None,
                                     nsIDOMNode::PROCESSING_INSTRUCTION_NODE,
                                     target);

  nsRefPtr<ProcessingInstruction> instance =
    new ProcessingInstruction(ni.forget(), aData);

  return instance.forget();
}

namespace mozilla {
namespace dom {

ProcessingInstruction::ProcessingInstruction(already_AddRefed<nsINodeInfo>&& aNodeInfo,
                                             const nsAString& aData)
  : nsGenericDOMDataNode(Move(aNodeInfo))
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() ==
                      nsIDOMNode::PROCESSING_INSTRUCTION_NODE,
                    "Bad NodeType in aNodeInfo");

  SetTextInternal(0, mText.GetLength(),
                  aData.BeginReading(), aData.Length(),
                  false);  // Don't notify (bug 420429).
}

ProcessingInstruction::~ProcessingInstruction()
{
}

NS_IMPL_ISUPPORTS_INHERITED3(ProcessingInstruction, nsGenericDOMDataNode,
                             nsIDOMNode, nsIDOMCharacterData,
                             nsIDOMProcessingInstruction)

JSObject*
ProcessingInstruction::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return ProcessingInstructionBinding::Wrap(aCx, aScope, this);
}

NS_IMETHODIMP
ProcessingInstruction::GetTarget(nsAString& aTarget)
{
  aTarget = NodeName();

  return NS_OK;
}

bool
ProcessingInstruction::GetAttrValue(nsIAtom *aName, nsAString& aValue)
{
  nsAutoString data;

  GetData(data);
  return nsContentUtils::GetPseudoAttributeValue(data, aName, aValue);
}

bool
ProcessingInstruction::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | ePROCESSING_INSTRUCTION | eDATA_NODE));
}

nsGenericDOMDataNode*
ProcessingInstruction::CloneDataNode(nsINodeInfo *aNodeInfo,
                                     bool aCloneText) const
{
  nsAutoString data;
  nsGenericDOMDataNode::GetData(data);
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  return new ProcessingInstruction(ni.forget(), data);
}

#ifdef DEBUG
void
ProcessingInstruction::List(FILE* out, int32_t aIndent) const
{
  int32_t index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%d<", mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  tmp.Insert(nsDependentAtomString(NodeInfo()->GetExtraName()).get(), 0);
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void
ProcessingInstruction::DumpContent(FILE* out, int32_t aIndent,
                                   bool aDumpAll) const
{
}
#endif

} // namespace dom
} // namespace mozilla
