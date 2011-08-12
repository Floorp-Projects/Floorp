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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

/*
 * A unique per-element set of attributes that is used as an
 * nsIStyleRule; used to implement presentational attributes.
 */

#ifndef nsMappedAttributes_h___
#define nsMappedAttributes_h___

#include "nsAttrAndChildArray.h"
#include "nsMappedAttributeElement.h"
#include "nsIStyleRule.h"

class nsIAtom;
class nsHTMLStyleSheet;
class nsRuleWalker;

class nsMappedAttributes : public nsIStyleRule
{
public:
  nsMappedAttributes(nsHTMLStyleSheet* aSheet,
                     nsMapRuleToAttributesFunc aMapRuleFunc);

  void* operator new(size_t size, PRUint32 aAttrCount = 1) CPP_THROW_NEW;

  nsMappedAttributes* Clone(PRBool aWillAddAttr);

  NS_DECL_ISUPPORTS

  nsresult SetAndTakeAttr(nsIAtom* aAttrName, nsAttrValue& aValue);
  const nsAttrValue* GetAttr(nsIAtom* aAttrName) const;

  PRUint32 Count() const
  {
    return mAttrCount;
  }

  PRBool Equals(const nsMappedAttributes* aAttributes) const;
  PRUint32 HashValue() const;

  void DropStyleSheetReference()
  {
    mSheet = nsnull;
  }
  void SetStyleSheet(nsHTMLStyleSheet* aSheet);
  nsHTMLStyleSheet* GetStyleSheet()
  {
    return mSheet;
  }

  const nsAttrName* NameAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < mAttrCount, "out-of-bounds");
    return &Attrs()[aPos].mName;
  }
  const nsAttrValue* AttrAt(PRUint32 aPos) const
  {
    NS_ASSERTION(aPos < mAttrCount, "out-of-bounds");
    return &Attrs()[aPos].mValue;
  }
  // Remove the attr at position aPos.  The value of the attr is placed in
  // aValue; any value that was already in aValue is destroyed.
  void RemoveAttrAt(PRUint32 aPos, nsAttrValue& aValue);
  const nsAttrName* GetExistingAttrNameFromQName(const nsAString& aName) const;
  PRInt32 IndexOfAttr(nsIAtom* aLocalName, PRInt32 aNamespaceID) const;
  

  // nsIStyleRule 
  virtual void MapRuleInfoInto(nsRuleData* aRuleData);
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  PRInt64 SizeOf() const;

private:
  nsMappedAttributes(const nsMappedAttributes& aCopy);
  ~nsMappedAttributes();

  struct InternalAttr
  {
    nsAttrName mName;
    nsAttrValue mValue;
  };

  /**
   * Due to a compiler bug in VisualAge C++ for AIX, we need to return the 
   * address of the first index into mAttrs here, instead of simply
   * returning mAttrs itself.
   *
   * See Bug 231104 for more information.
   */
  const InternalAttr* Attrs() const
  {
    return reinterpret_cast<const InternalAttr*>(&(mAttrs[0]));
  }
  InternalAttr* Attrs()
  {
    return reinterpret_cast<InternalAttr*>(&(mAttrs[0]));
  }

  PRUint16 mAttrCount;
#ifdef DEBUG
  PRUint16 mBufferSize;
#endif
  nsHTMLStyleSheet* mSheet; //weak
  nsMapRuleToAttributesFunc mRuleMapper;
  void* mAttrs[1];
};

#endif /* nsMappedAttributes_h___ */
