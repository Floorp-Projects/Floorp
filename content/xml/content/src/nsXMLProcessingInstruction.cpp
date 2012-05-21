/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericElement.h"
#include "nsGkAtoms.h"
#include "nsUnicharUtils.h"
#include "nsXMLProcessingInstruction.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"

nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               nsNodeInfoManager *aNodeInfoManager,
                               const nsAString& aTarget,
                               const nsAString& aData)
{
  NS_PRECONDITION(aNodeInfoManager, "Missing nodeinfo manager");

  nsCOMPtr<nsIAtom> target = do_GetAtom(aTarget);
  NS_ENSURE_TRUE(target, NS_ERROR_OUT_OF_MEMORY);

  if (target == nsGkAtoms::xml_stylesheet) {
    return NS_NewXMLStylesheetProcessingInstruction(aInstancePtrResult,
                                                    aNodeInfoManager, aData);
  }

  *aInstancePtrResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni;
  ni = aNodeInfoManager->GetNodeInfo(nsGkAtoms::processingInstructionTagName,
                                     nsnull, kNameSpaceID_None,
                                     nsIDOMNode::PROCESSING_INSTRUCTION_NODE,
                                     target);
  NS_ENSURE_TRUE(ni, NS_ERROR_OUT_OF_MEMORY);

  nsXMLProcessingInstruction *instance =
    new nsXMLProcessingInstruction(ni.forget(), aData);
  if (!instance) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aInstancePtrResult = instance);

  return NS_OK;
}

nsXMLProcessingInstruction::nsXMLProcessingInstruction(already_AddRefed<nsINodeInfo> aNodeInfo,
                                                       const nsAString& aData)
  : nsGenericDOMDataNode(aNodeInfo)
{
  NS_ABORT_IF_FALSE(mNodeInfo->NodeType() ==
                      nsIDOMNode::PROCESSING_INSTRUCTION_NODE,
                    "Bad NodeType in aNodeInfo");

  SetTextInternal(0, mText.GetLength(),
                  aData.BeginReading(), aData.Length(),
                  false);  // Don't notify (bug 420429).
}

nsXMLProcessingInstruction::~nsXMLProcessingInstruction()
{
}


DOMCI_NODE_DATA(ProcessingInstruction, nsXMLProcessingInstruction)

// QueryInterface implementation for nsXMLProcessingInstruction
NS_INTERFACE_TABLE_HEAD(nsXMLProcessingInstruction)
  NS_NODE_OFFSET_AND_INTERFACE_TABLE_BEGIN(nsXMLProcessingInstruction)
    NS_INTERFACE_TABLE_ENTRY(nsXMLProcessingInstruction, nsIDOMNode)
    NS_INTERFACE_TABLE_ENTRY(nsXMLProcessingInstruction, nsIDOMCharacterData)
    NS_INTERFACE_TABLE_ENTRY(nsXMLProcessingInstruction,
                             nsIDOMProcessingInstruction)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_OFFSET_AND_INTERFACE_TABLE_TO_MAP_SEGUE
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ProcessingInstruction)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)


NS_IMPL_ADDREF_INHERITED(nsXMLProcessingInstruction, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsXMLProcessingInstruction, nsGenericDOMDataNode)


NS_IMETHODIMP
nsXMLProcessingInstruction::GetTarget(nsAString& aTarget)
{
  aTarget = NodeName();

  return NS_OK;
}

bool
nsXMLProcessingInstruction::GetAttrValue(nsIAtom *aName, nsAString& aValue)
{
  nsAutoString data;

  GetData(data);
  return nsContentUtils::GetPseudoAttributeValue(data, aName, aValue);
}

bool
nsXMLProcessingInstruction::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | ePROCESSING_INSTRUCTION | eDATA_NODE));
}

nsGenericDOMDataNode*
nsXMLProcessingInstruction::CloneDataNode(nsINodeInfo *aNodeInfo,
                                          bool aCloneText) const
{
  nsAutoString data;
  nsGenericDOMDataNode::GetData(data);
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  return new nsXMLProcessingInstruction(ni.forget(), data);
}

#ifdef DEBUG
void
nsXMLProcessingInstruction::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%d<", mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  tmp.Insert(nsDependentAtomString(NodeInfo()->GetExtraName()).get(), 0);
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void
nsXMLProcessingInstruction::DumpContent(FILE* out, PRInt32 aIndent,
                                        bool aDumpAll) const
{
}
#endif
