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
 * The Original Code is MathML DOM code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vlad Sukhoy <vladimir.sukhoy@gmail.com> (original developer)
 *    Daniel Kraft <d@domob.eu> (nsMathMLElement patch, attachment 262925) 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsMathMLElement_h
#define nsMathMLElement_h

#include "nsMappedAttributeElement.h"
#include "nsIDOMElement.h"
#include "nsILink.h"
#include "Link.h"

class nsCSSValue;

typedef nsMappedAttributeElement nsMathMLElementBase;

/*
 * The base class for MathML elements.
 */
class nsMathMLElement : public nsMathMLElementBase,
                        public nsIDOMElement,
                        public nsILink,
                        public mozilla::dom::Link
{
public:
  nsMathMLElement(already_AddRefed<nsINodeInfo> aNodeInfo)
    : nsMathMLElementBase(aNodeInfo), Link(this),
      mIncrementScriptLevel(false)
  {}

  // Implementation of nsISupports is inherited from nsMathMLElementBase
  NS_DECL_ISUPPORTS_INHERITED

  // Forward implementations of parent interfaces of nsMathMLElement to 
  // our base class
  NS_FORWARD_NSIDOMNODE(nsMathMLElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsMathMLElementBase::)

  nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                      nsIContent* aBindingParent,
                      bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;

  enum {
    PARSE_ALLOW_UNITLESS = 0x01, // unitless 0 will be turned into 0px
    PARSE_ALLOW_NEGATIVE = 0x02
  };
  static bool ParseNumericValue(const nsString& aString,
                                  nsCSSValue&     aCSSValue,
                                  PRUint32        aFlags);

  static void MapMathMLAttributesInto(const nsMappedAttributes* aAttributes, 
                                      nsRuleData* aRuleData);
  
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);
  nsresult Clone(nsINodeInfo*, nsINode**) const;
  virtual nsEventStates IntrinsicState() const;
  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  // Set during reflow as necessary. Does a style change notification,
  // aNotify must be true.
  void SetIncrementScriptLevel(bool aIncrementScriptLevel, bool aNotify);
  bool GetIncrementScriptLevel() const {
    return mIncrementScriptLevel;
  }

  NS_IMETHOD LinkAdded() { return NS_OK; }
  NS_IMETHOD LinkRemoved() { return NS_OK; }
  virtual bool IsFocusable(PRInt32 *aTabIndex = nsnull,
                             bool aWithMouse = false);
  virtual bool IsLink(nsIURI** aURI) const;
  virtual void GetLinkTarget(nsAString& aTarget);
  virtual nsLinkState GetLinkState() const;
  virtual already_AddRefed<nsIURI> GetHrefURI() const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, bool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual nsXPCClassInfo* GetClassInfo();
private:
  bool mIncrementScriptLevel;
};

#endif // nsMathMLElement_h
