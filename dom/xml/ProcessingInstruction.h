/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ProcessingInstruction_h
#define mozilla_dom_ProcessingInstruction_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/CharacterData.h"
#include "nsAString.h"

class nsIPrincipal;
class nsIURI;

namespace mozilla {

class StyleSheet;

namespace dom {

class ProcessingInstruction : public CharacterData {
 public:
  ProcessingInstruction(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                        const nsAString& aData);

  virtual already_AddRefed<CharacterData> CloneDataNode(
      dom::NodeInfo* aNodeInfo, bool aCloneText) const override;

#ifdef MOZ_DOM_LIST
  virtual void List(FILE* out, int32_t aIndent) const override;
  virtual void DumpContent(FILE* out, int32_t aIndent,
                           bool aDumpAll) const override;
#endif

  // WebIDL API
  void GetTarget(nsAString& aTarget) { aTarget = NodeName(); }
  // This is the WebIDL API for LinkStyle, even though only
  // XMLStylesheetProcessingInstruction actually implements LinkStyle.
  StyleSheet* GetSheetForBindings() const;

  NS_IMPL_FROMNODE_HELPER(ProcessingInstruction, IsProcessingInstruction())

 protected:
  virtual ~ProcessingInstruction();

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
  bool GetAttrValue(nsAtom* aName, nsAString& aValue);

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;
};

}  // namespace dom
}  // namespace mozilla

/**
 * aNodeInfoManager must not be null.
 */
already_AddRefed<mozilla::dom::ProcessingInstruction>
NS_NewXMLProcessingInstruction(nsNodeInfoManager* aNodeInfoManager,
                               const nsAString& aTarget,
                               const nsAString& aData);

#endif  // mozilla_dom_ProcessingInstruction_h
