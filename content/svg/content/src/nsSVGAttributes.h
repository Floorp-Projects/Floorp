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


#ifndef __NS_SVGATTRIBUTES_H__
#define __NS_SVGATTRIBUTES_H__

#include "nsCOMPtr.h"
#include "nsISVGAttribute.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsINodeInfo.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsWeakReference.h"

class nsIContent;
class nsSVGAttributes;

////////////////////////////////////////////////////////////////////////
// SVG Attribute Flags

// XXX these flags are not used yet

typedef PRUint32 nsSVGAttributeFlags;

// This is a #REQUIRED-attribute. Should not be allowed to unset
#define NS_SVGATTRIBUTE_FLAGS_REQUIRED 0x0001

// This is a #FIXED-attribute. Should not be allowed to set/unset
#define NS_SVGATTRIBUTE_FLAGS_FIXED    0x0002

// this attribute is a mapped value. if it is being unset we keep it
// around to be reused:
#define NS_SVGATTRIBUTE_FLAGS_MAPPED   0x0004

////////////////////////////////////////////////////////////////////////
// nsSVGAttribute

class nsSVGAttribute : public nsISVGAttribute, // :nsIDOMAttr
                       public nsISVGValueObserver,
                       public nsSupportsWeakReference
{
public:
  static nsresult
  Create(nsINodeInfo* aNodeInfo,
         nsISVGValue* value,
         nsSVGAttributeFlags flags,
         nsSVGAttribute** aResult);

  // create a generic string attribute:
  static nsresult
  Create(nsINodeInfo* aNodeInfo,
         const nsAString& value,
         nsSVGAttribute** aResult);

protected:

  nsSVGAttribute(nsINodeInfo* aNodeInfo,
                 nsISVGValue* value,
                 nsSVGAttributeFlags flags);
  
  virtual ~nsSVGAttribute();
  
public:
  // nsISupports interface
  NS_DECL_ISUPPORTS
  
  // nsIDOMNode interface
  NS_DECL_NSIDOMNODE
  
  // nsIDOMAttr interface
  NS_DECL_NSIDOMATTR

  // nsISVGAttribute interface
  NS_IMETHOD GetSVGValue(nsISVGValue** value);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference

  
  // other implementation functions
  nsINodeInfo* GetNodeInfo()const { return mNodeInfo; }
  void GetQualifiedName(nsAString& aQualifiedName)const;

  nsISVGValue* GetValue() { return mValue; }

  nsSVGAttributeFlags GetFlags()const { return mFlags; }
  PRBool IsRequired()const { return mFlags & NS_SVGATTRIBUTE_FLAGS_REQUIRED; }
  PRBool IsFixed()const    { return mFlags & NS_SVGATTRIBUTE_FLAGS_FIXED;    }
  
protected:
  friend class nsSVGAttributes;
  
  nsSVGAttributeFlags   mFlags;
  nsSVGAttributes*      mOwner;
  nsCOMPtr<nsINodeInfo> mNodeInfo;
  nsCOMPtr<nsISVGValue>  mValue;
};

////////////////////////////////////////////////////////////////////////
// nsSVGAttributes: the collection of attribs for one content element

class nsSVGAttributes : public nsIDOMNamedNodeMap
{
public:
  static nsresult
  Create(nsIContent* aElement, nsSVGAttributes** aResult);

protected:
  nsSVGAttributes(nsIContent* aContent);
  virtual ~nsSVGAttributes();

public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIDOMNamedNodeMap interface
  NS_DECL_NSIDOMNAMEDNODEMAP
  
  // interface for the content element:

  PRInt32 Count() const;
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                     nsIAtom** aPrefix,
                     nsAString& aResult);
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAString& aValue,
                     PRBool aNotify);
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                       PRBool aNotify);
  NS_IMETHOD_(PRBool) HasAttr(PRInt32 aNameSpaceID,
                              nsIAtom* aName) const;
  NS_IMETHOD_(already_AddRefed<nsINodeInfo>) GetExistingAttrNameFromQName(const nsAString& aStr);
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
                           PRInt32* aNameSpaceID,
                           nsIAtom** aName,
                           nsIAtom** aPrefix);

  NS_IMETHOD AddMappedSVGValue(nsIAtom* name, nsISupports* value);

  NS_IMETHOD CopyAttributes(nsSVGAttributes* dest);
  
  // interface for our attributes:
  nsIContent* GetContent(){ return mContent; }
  void AttributeWasModified(nsSVGAttribute* caller);
  
protected:
  // implementation helpers:
  void ReleaseAttributes();
  void ReleaseMappedAttributes();
  PRBool GetMappedAttribute(nsINodeInfo* aNodeInfo, nsSVGAttribute** 
attrib);

  PRBool IsExplicitAttribute(nsSVGAttribute* attrib);
  PRBool IsMappedAttribute(nsSVGAttribute* attrib);  

  nsSVGAttribute* ElementAt(PRInt32 index) const;
  void AppendElement(nsSVGAttribute* aElement);
  void RemoveElementAt(PRInt32 aIndex);

  
  nsIContent* mContent; // our owner
  
  nsAutoVoidArray mAttributes;
  nsAutoVoidArray mMappedAttributes;
};


#endif // __NS_SVGATTRIBUTES_H__

