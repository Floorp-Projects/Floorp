/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
#include "nsSVGAttributes.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"
#include "nsISVGStyleValue.h"

class nsSVGElement : public nsGenericElement,    // :nsIHTMLContent:nsIXMLContent:nsIStyledContent:nsIContent
                     public nsIDOMSVGElement,    // :nsIDOMElement:nsIDOMNode
                     public nsISVGValueObserver, 
                     public nsSupportsWeakReference // :nsISupportsWeakReference
{
protected:
  nsSVGElement();
  virtual ~nsSVGElement();

  virtual nsresult Init();

public:
  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIContent interface methods

  virtual void SetParent(nsIContent* aParent);
  virtual PRBool CanContainChildren() const;
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const;
  virtual nsIAtom *GetIDAttributeName() const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsIAtom* aPrefix, const nsAString& aValue,
                           PRBool aNotify);
  virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsAString& aResult) const;
  virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                             PRBool aNotify);
  virtual nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                 nsIAtom** aName, nsIAtom** aPrefix) const;
  virtual PRUint32 GetAttrCount() const;

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif // DEBUG
  
  virtual const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

  virtual nsresult SetBindingParent(nsIContent* aParent);

  // nsIStyledContent
  NS_IMETHOD GetID(nsIAtom** aResult) const;
//   NS_IMETHOD GetClasses(nsVoidArray& aArray) const;
//   NS_IMETHOD HasClass(nsIAtom* aClass) const;
  
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  NS_IMETHOD GetInlineStyleRule(nsICSSStyleRule** aStyleRule);

  static const MappedAttributeEntry sFillStrokeMap[];
  static const MappedAttributeEntry sGraphicsMap[];
  static const MappedAttributeEntry sTextContentElementsMap[];
  static const MappedAttributeEntry sFontSpecificationMap[];
  
  // nsIDOMNode
  NS_DECL_NSIDOMNODE
  
  // nsIDOMElement
  // NS_DECL_IDOMELEMENT
  NS_FORWARD_NSIDOMELEMENT(nsGenericElement::)
  
  // nsIDOMSVGElement
  NS_DECL_NSIDOMSVGELEMENT

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
protected:

  nsresult CopyNode(nsSVGElement* dest, PRBool deep);
  virtual void ParentChainChanged(){}; 
  
  nsSVGAttributes*             mAttributes;
  nsCOMPtr<nsISVGStyleValue>   mStyle;
};

#endif // __NS_SVGELEMENT_H__
