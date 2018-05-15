/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGkAtoms.h"
#include "nsUnicharUtils.h"
#include "mozilla/dom/ProcessingInstruction.h"
#include "mozilla/dom/ProcessingInstructionBinding.h"
#include "mozilla/dom/XMLStylesheetProcessingInstruction.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "nsContentUtils.h"

already_AddRefed<mozilla::dom::ProcessingInstruction>
NS_NewXMLProcessingInstruction(nsNodeInfoManager *aNodeInfoManager,
                               const nsAString& aTarget,
                               const nsAString& aData)
{
  using mozilla::dom::ProcessingInstruction;
  using mozilla::dom::XMLStylesheetProcessingInstruction;

  MOZ_ASSERT(aNodeInfoManager, "Missing nodeinfo manager");

  RefPtr<nsAtom> target = NS_Atomize(aTarget);
  MOZ_ASSERT(target);

  if (target == nsGkAtoms::xml_stylesheet) {
    RefPtr<XMLStylesheetProcessingInstruction> pi =
      new XMLStylesheetProcessingInstruction(aNodeInfoManager, aData);
    return pi.forget();
  }

  RefPtr<mozilla::dom::NodeInfo> ni;
  ni = aNodeInfoManager->GetNodeInfo(nsGkAtoms::processingInstructionTagName,
                                     nullptr, kNameSpaceID_None,
                                     nsINode::PROCESSING_INSTRUCTION_NODE,
                                     target);

  RefPtr<ProcessingInstruction> instance =
    new ProcessingInstruction(ni.forget(), aData);

  return instance.forget();
}

namespace mozilla {
namespace dom {

ProcessingInstruction::ProcessingInstruction(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                                             const nsAString& aData)
  : CharacterData(Move(aNodeInfo))
{
  MOZ_ASSERT(mNodeInfo->NodeType() == nsINode::PROCESSING_INSTRUCTION_NODE,
             "Bad NodeType in aNodeInfo");

  SetTextInternal(0, mText.GetLength(),
                  aData.BeginReading(), aData.Length(),
                  false);  // Don't notify (bug 420429).
}

ProcessingInstruction::~ProcessingInstruction()
{
}

// If you add nsIStyleSheetLinkingElement here, make sure we actually
// implement the nsStyleLinkElement methods.
NS_IMPL_ISUPPORTS_INHERITED(ProcessingInstruction, CharacterData, nsIDOMNode)

JSObject*
ProcessingInstruction::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ProcessingInstructionBinding::Wrap(aCx, this, aGivenProto);
}

bool
ProcessingInstruction::GetAttrValue(nsAtom *aName, nsAString& aValue)
{
  nsAutoString data;

  GetData(data);
  return nsContentUtils::GetPseudoAttributeValue(data, aName, aValue);
}

already_AddRefed<CharacterData>
ProcessingInstruction::CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo,
                                     bool aCloneText) const
{
  nsAutoString data;
  GetData(data);
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  return do_AddRef(new ProcessingInstruction(ni.forget(), data));
}

Maybe<nsStyleLinkElement::SheetInfo>
ProcessingInstruction::GetStyleSheetInfo()
{
  MOZ_ASSERT_UNREACHABLE("XMLStylesheetProcessingInstruction should override "
                         "this and we don't try to do stylesheet stuff.  In "
                         "particular, we do not implement "
                         "nsIStyleSheetLinkingElement");
  return Nothing();
}

#ifdef DEBUG
void
ProcessingInstruction::List(FILE* out, int32_t aIndent) const
{
  int32_t index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%" PRIuPTR "<", mRefCnt.get());

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
