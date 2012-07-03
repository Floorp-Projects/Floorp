/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIXMLProcessingInstruction_h___
#define nsIXMLProcessingInstruction_h___

#include "nsIDOMProcessingInstruction.h"
#include "nsGenericDOMDataNode.h"
#include "nsAString.h"


class nsXMLProcessingInstruction : public nsGenericDOMDataNode,
                                   public nsIDOMProcessingInstruction
{
public:
  nsXMLProcessingInstruction(already_AddRefed<nsINodeInfo> aNodeInfo,
                             const nsAString& aData);
  virtual ~nsXMLProcessingInstruction();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericDOMDataNode::)

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)

  // nsIDOMProcessingInstruction
  NS_DECL_NSIDOMPROCESSINGINSTRUCTION

  // nsINode
  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  virtual nsGenericDOMDataNode* CloneDataNode(nsINodeInfo *aNodeInfo,
                                              bool aCloneText) const;

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent, bool aDumpAll) const;
#endif

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
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
};

#endif //nsIXMLProcessingInstruction_h___
