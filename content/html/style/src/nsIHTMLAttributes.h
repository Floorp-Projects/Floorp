/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIHTMLAttributes_h___
#define nsIHTMLAttributes_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsHTMLValue.h"
#include "nsIHTMLContent.h"
class nsIAtom;
class nsISupportsArray;
class nsIHTMLStyleSheet;
class nsIRuleWalker;

// IID for the nsIHTMLAttributes interface {a18f85f0-c058-11d1-8031-006008159b5a}
#define NS_IHTML_ATTRIBUTES_IID     \
{0xa18f85f0, 0xc058, 0x11d1,        \
  {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

// IID for the nsIHTMLMappedAttributes interface {0fdd27a0-2e7b-11d3-8060-006008159b5a}
#define NS_IHTML_MAPPED_ATTRIBUTES_IID  \
{0x0fdd27a0, 0x2e7b, 0x11d3,            \
    {0x80, 0x60, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class nsIHTMLAttributes : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTML_ATTRIBUTES_IID);

  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                             PRBool aMappedToStyle, 
                             nsIHTMLContent* aContent,
                             nsIHTMLStyleSheet* aSheet,
                             PRInt32& aAttrCount) = 0;
  // this string value version lets you avoid an extra string copy, 
  // the value is still stored in a nsHTMLValue
  NS_IMETHOD SetAttributeFor(nsIAtom* aAttribute, const nsAReadableString& aValue,
                             PRBool aMappedToStyle, 
                             nsIHTMLContent* aContent,
                             nsIHTMLStyleSheet* aSheet) = 0;
  NS_IMETHOD UnsetAttributeFor(nsIAtom* aAttribute, 
                               nsIHTMLContent* aContent,
                               nsIHTMLStyleSheet* aSheet,
                               PRInt32& aAttrCount) = 0;

  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aValue) const = 0;
  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          const nsHTMLValue** aValue) const = 0;

  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                nsIAtom*& aName) const = 0;

  NS_IMETHOD GetAttributeCount(PRInt32& aAttrCount) const = 0;

  NS_IMETHOD GetID(nsIAtom*& aResult) const = 0;
  NS_IMETHOD GetClasses(nsVoidArray& aArray) const = 0;
  NS_IMETHOD HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const = 0;

  NS_IMETHOD Clone(nsIHTMLAttributes** aInstancePtrResult) const = 0;

  NS_IMETHOD SetStyleSheet(nsIHTMLStyleSheet* aSheet) = 0;

  NS_IMETHOD WalkMappedAttributeStyleRules(nsIRuleWalker* aRuleWalker) const = 0;

#ifdef UNIQUE_ATTR_SUPPORT
  NS_IMETHOD Equals(const nsIHTMLAttributes* aAttributes, PRBool& aResult) const = 0;
  NS_IMETHOD HashValue(PRUint32& aValue) const = 0;

  NS_IMETHOD AddContentRef(void) = 0;
  NS_IMETHOD ReleaseContentRef(void) = 0;
  NS_IMETHOD GetContentRefCount(PRInt32& aCount) const = 0;

  NS_IMETHOD Reset(void) = 0;
#endif

#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;

  virtual void SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult) = 0;
#endif
};

class nsIHTMLMappedAttributes : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTML_MAPPED_ATTRIBUTES_IID);

  NS_IMETHOD GetAttribute(nsIAtom* aAttrName, nsHTMLValue& aValue) const = 0;
  NS_IMETHOD GetAttribute(nsIAtom* aAttrName, const nsHTMLValue** aValue) const = 0;
  NS_IMETHOD GetAttributeCount(PRInt32& aCount) const = 0;

  NS_IMETHOD Equals(const nsIHTMLMappedAttributes* aAttributes, PRBool& aResult) const = 0;
  NS_IMETHOD HashValue(PRUint32& aValue) const = 0;

  // Sheet accessors for unique table management
  NS_IMETHOD SetUniqued(PRBool aUniqued) = 0;
  NS_IMETHOD GetUniqued(PRBool& aUniqued) = 0;
  NS_IMETHOD DropStyleSheetReference(void) = 0;

#ifdef DEBUG
  virtual void SizeOf(nsISizeOfHandler* aSizer, PRUint32 &aResult) = 0;
#endif
};

extern NS_HTML nsresult
  NS_NewHTMLAttributes(nsIHTMLAttributes** aInstancePtrResult);

#endif /* nsIHTMLAttributes_h___ */

