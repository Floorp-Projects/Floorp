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
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is 
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Alex Fritze <alex@croczilla.com> (original author)
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

#ifndef __NS_XTFELEMENTWRAPPER_H__
#define __NS_XTFELEMENTWRAPPER_H__

#include "nsIXTFElementWrapper.h"
#include "nsIXTFElementWrapperPrivate.h"
#include "nsXMLElement.h"
#include "nsIXTFAttributeHandler.h"

class nsIXTFElement;

typedef nsXMLElement nsXTFElementWrapperBase;

// Pseudo IID for nsXTFElementWrapper
// {599EB85F-ABC0-4B52-A1B0-EA103D48E3AE}
#define NS_XTFELEMENTWRAPPER_IID \
{ 0x599eb85f, 0xabc0, 0x4b52, { 0xa1, 0xb0, 0xea, 0x10, 0x3d, 0x48, 0xe3, 0xae } }


class nsXTFElementWrapper : public nsXTFElementWrapperBase,
                            public nsIXTFElementWrapper,
                            public nsIXTFElementWrapperPrivate,
                            public nsIClassInfo
{
protected:
  nsXTFElementWrapper(nsINodeInfo* aNodeInfo);
  nsresult Init();
  
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_XTFELEMENTWRAPPER_IID)

  // nsISupports interface
  NS_DECL_ISUPPORTS_INHERITED

  // nsIXTFElementWrapper
  NS_DECL_NSIXTFELEMENTWRAPPER

  // nsIXTFElementWrapperPrivate
  // to be implemented by subclass
  // virtual PRUint32 GetElementType() = 0;
    
  // nsIContent specializations:
  void SetDocument(nsIDocument* aDocument, PRBool aDeep,
                   PRBool aCompileEventHandlers);
  void SetParent(nsIContent* aParent);
  nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                         PRBool aNotify, PRBool aDeepSetDocument);
  nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                         PRBool aDeepSetDocument);
  nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  nsIAtom *GetIDAttributeName() const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   nsIAtom* aPrefix, const nsAString& aValue,
                   PRBool aNotify);
  nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                   nsAString& aResult) const;
  PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                     PRBool aNotify);
  nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                         nsIAtom** aName, nsIAtom** aPrefix) const;
  PRUint32 GetAttrCount() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;

  virtual void DoneAddingChildren();
  
  // nsIDOMElement specializations:
  NS_IMETHOD GetAttribute(const nsAString& aName,
                          nsAString& aReturn);
  NS_IMETHOD RemoveAttribute(const nsAString& aName);
  NS_IMETHOD HasAttribute(const nsAString& aName, PRBool* aReturn);
  
  // nsIClassInfo interface
  NS_DECL_NSICLASSINFO

protected:
  // to be implemented by subclasses:
  virtual nsIXTFElement *GetXTFElement() const = 0;
  
  // implementation helpers:  
  PRBool AggregatesInterface(REFNSIID aIID);

  PRBool HandledByInner(nsIAtom* attr) const;
  
  PRUint32 mNotificationMask;
  nsCOMPtr<nsIXTFAttributeHandler> mAttributeHandler;
};

#endif // __NS_XTFELEMENTWRAPPER_H__
