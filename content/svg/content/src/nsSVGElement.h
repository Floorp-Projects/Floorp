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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_SVGELEMENT_H__
#define __NS_SVGELEMENT_H__

/*
  nsSVGElement is the base class for all SVG content elements.
  It implements all the common DOM interfaces and handles attributes.
*/

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIDOMSVGElement.h"
#include "nsGenericElement.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsISVGContent.h"
#include "nsICSSStyleRule.h"

class nsSVGElement : public nsGenericElement,    // :nsIHTMLContent:nsIXMLContent:nsIStyledContent:nsIContent
                     public nsISVGValueObserver, 
                     public nsSupportsWeakReference, // :nsISupportsWeakReference
                     public nsISVGContent
{
protected:
  nsSVGElement(nsINodeInfo *aNodeInfo);
  virtual ~nsSVGElement();

public:
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface methods

  virtual void SetParent(nsIContent* aParent);
  virtual nsIAtom *GetIDAttributeName() const;
  virtual nsIAtom *GetClassAttributeName() const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);
  
  virtual nsresult SetBindingParent(nsIContent* aParent);
  virtual PRBool IsContentOfType(PRUint32 aFlags) const;

  // nsIStyledContent
  NS_IMETHOD GetID(nsIAtom** aResult) const;
  
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify);
  NS_IMETHOD GetInlineStyleRule(nsICSSStyleRule** aStyleRule);

  static const MappedAttributeEntry sFillStrokeMap[];
  static const MappedAttributeEntry sGraphicsMap[];
  static const MappedAttributeEntry sTextContentElementsMap[];
  static const MappedAttributeEntry sFontSpecificationMap[];
  static const MappedAttributeEntry sGradientStopMap[];
  
  // nsIDOMNode
  NS_IMETHOD IsSupported(const nsAString& aFeature, const nsAString& aVersion, PRBool* aReturn);
  
  // nsIDOMSVGElement
  NS_IMETHOD GetId(nsAString & aId);
  NS_IMETHOD SetId(const nsAString & aId);
  NS_IMETHOD GetOwnerSVGElement(nsIDOMSVGSVGElement** aOwnerSVGElement);
  NS_IMETHOD GetViewportElement(nsIDOMSVGElement** aViewportElement);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

  // nsISVGContent
  virtual void ParentChainChanged(); 
  
protected:
  // Hooks for subclasses
  virtual PRBool IsEventName(nsIAtom* aName);

  /**
   * Set attribute and (if needed) notify documentobservers and fire off
   * mutation events.
   *
   * @param aNamespaceID  namespace of attribute
   * @param aAttribute    local-name of attribute
   * @param aPrefix       aPrefix of attribute
   * @param aOldValue     previous value of attribute. Only needed if
   *                      aFireMutation is true.
   * @param aParsedValue  parsed new value of attribute
   * @param aModification is this a attribute-modification or addition. Only
   *                      needed if aFireMutation or aNotify is true.
   * @param aFireMutation should mutation-events be fired?
   * @param aNotify       should we notify document-observers?
   */
  nsresult SetAttrAndNotify(PRInt32 aNamespaceID,
                            nsIAtom* aAttribute,
                            nsIAtom* aPrefix,
                            const nsAString& aOldValue,
                            nsAttrValue& aParsedValue,
                            PRBool aModification,
                            PRBool aFireMutation,
                            PRBool aNotify);

  void UpdateContentStyleRule();
  nsISVGValue* GetMappedAttribute(PRInt32 aNamespaceID, nsIAtom* aName);
  nsresult AddMappedSVGValue(nsIAtom* aName, nsISupports* aValue,
                             PRInt32 aNamespaceID = kNameSpaceID_None);
  
  static PRBool IsGraphicElementEventName(nsIAtom* aName);

  nsCOMPtr<nsICSSStyleRule> mContentStyleRule;
  nsAttrAndChildArray mMappedAttributes;
};

/**
 * A macro to implement the NS_NewSVGXXXElement() functions.
 */
#define NS_IMPL_NS_NEW_SVG_ELEMENT(_elementName)                             \
nsresult                                                                     \
NS_NewSVG##_elementName##Element(nsIContent **aResult,                       \
                                 nsINodeInfo *aNodeInfo)                     \
{                                                                            \
  nsSVG##_elementName##Element *it =                                         \
    new nsSVG##_elementName##Element(aNodeInfo);                             \
  if (!it)                                                                   \
    return NS_ERROR_OUT_OF_MEMORY;                                           \
                                                                             \
  NS_ADDREF(it);                                                             \
                                                                             \
  nsresult rv = it->Init();                                                  \
                                                                             \
  if (NS_FAILED(rv)) {                                                       \
    NS_RELEASE(it);                                                          \
    return rv;                                                               \
  }                                                                          \
                                                                             \
  *aResult = it;                                                             \
                                                                             \
  return rv;                                                                 \
}


#endif // __NS_SVGELEMENT_H__
