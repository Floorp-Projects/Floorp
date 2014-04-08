/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessingInstruction_h
#define mozilla_dom_ProcessingInstruction_h

#include "mozilla/Attributes.h"
#include "nsIDOMProcessingInstruction.h"
#include "nsGenericDOMDataNode.h"
#include "nsAString.h"

namespace mozilla {
namespace dom {

class ProcessingInstruction : public nsGenericDOMDataNode,
                              public nsIDOMProcessingInstruction
{
public:
  ProcessingInstruction(already_AddRefed<nsINodeInfo>&& aNodeInfo,
                        const nsAString& aData);
  virtual ~ProcessingInstruction();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)
  using nsGenericDOMDataNode::SetData; // Prevent hiding overloaded virtual function.

  // nsIDOMProcessingInstruction
  NS_DECL_NSIDOMPROCESSINGINSTRUCTION

  // nsINode
  virtual bool IsNodeOfType(uint32_t aFlags) const;

  virtual nsGenericDOMDataNode* CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const MOZ_OVERRIDE;

#ifdef DEBUG
  virtual void List(FILE* out, int32_t aIndent) const MOZ_OVERRIDE;
  virtual void DumpContent(FILE* out, int32_t aIndent, bool aDumpAll) const MOZ_OVERRIDE;
#endif

  virtual nsIDOMNode* AsDOMNode() MOZ_OVERRIDE { return this; }

  // WebIDL API
  void GetTarget(nsString& aTarget)
  {
    aTarget = NodeName();
  }
protected:
  /**
   * This will parse the content of the PI, to extract the value of the pseudo
   * attribute with the name specified in aName. See
   * http://www.w3.org/TR/xml-stylesheet/#NT-StyleSheetPI for the specification
   * which is used to parse the content of the PI.
   *
   * @param aName the name of the attribute to get the value for
   * @param aValue [out] the value for the attribute with name specified in
   *                     aAttribute. Empty if the attribute isn't present.
   */
  bool GetAttrValue(nsIAtom *aName, nsAString& aValue);

  virtual JSObject* WrapNode(JSContext *aCx) MOZ_OVERRIDE;
};

} // namespace dom
} // namespace mozilla

/**
 * aNodeInfoManager must not be null.
 */
already_AddRefed<mozilla::dom::ProcessingInstruction>
NS_NewXMLProcessingInstruction(nsNodeInfoManager *aNodeInfoManager,
                               const nsAString& aTarget,
                               const nsAString& aData);

#endif // mozilla_dom_ProcessingInstruction_h
