/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_XTFELEMENTWRAPPER_H__
#define __NS_XTFELEMENTWRAPPER_H__

#include "nsIXTFElementWrapper.h"
#include "nsXMLElement.h"
#include "nsIXTFAttributeHandler.h"
#include "nsIXTFElement.h"
#include "mozilla/Attributes.h"

typedef nsXMLElement nsXTFElementWrapperBase;
class nsXTFClassInfo;

// Pseudo IID for nsXTFElementWrapper
// {599EB85F-ABC0-4B52-A1B0-EA103D48E3AE}
#define NS_XTFELEMENTWRAPPER_IID \
{ 0x599eb85f, 0xabc0, 0x4b52, { 0xa1, 0xb0, 0xea, 0x10, 0x3d, 0x48, 0xe3, 0xae } }


class nsXTFElementWrapper : public nsXTFElementWrapperBase,
                            public nsIXTFElementWrapper
{
public:
  nsXTFElementWrapper(already_AddRefed<nsINodeInfo> aNodeInfo, nsIXTFElement* aXTFElement);
  virtual ~nsXTFElementWrapper();
  nsresult Init();

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_XTFELEMENTWRAPPER_IID)

  // nsISupports interface
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsXTFElementWrapper,
                                                     nsXTFElementWrapperBase)

  // nsIXTFElementWrapper
  NS_DECL_NSIXTFELEMENTWRAPPER
    
  // nsIContent specializations:
#ifdef HAVE_CPP_AMBIGUITY_RESOLVING_USING
  using nsINode::GetProperty;
  using nsINode::SetProperty;
#endif

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                         bool aNotify);
  void RemoveChildAt(PRUint32 aIndex, bool aNotify);
  nsIAtom *GetIDAttributeName() const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   nsIAtom* aPrefix, const nsAString& aValue,
                   bool aNotify);
  bool GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                 nsAString& aResult) const;
  bool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  virtual bool AttrValueIs(PRInt32 aNameSpaceID, nsIAtom* aName,
                             const nsAString& aValue,
                             nsCaseTreatment aCaseSensitive) const;
  virtual bool AttrValueIs(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aValue,
                             nsCaseTreatment aCaseSensitive) const;
  virtual PRInt32 FindAttrValueIn(PRInt32 aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const;
  nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                     bool aNotify);
  const nsAttrName* GetAttrNameAt(PRUint32 aIndex) const;
  PRUint32 GetAttrCount() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;

  virtual nsEventStates IntrinsicState() const;

  virtual void BeginAddingChildren();
  virtual void DoneAddingChildren(bool aHaveNotified);

  virtual nsIAtom *GetClassAttributeName() const;
  virtual const nsAttrValue* DoGetClasses() const;

  virtual void PerformAccesskey(bool aKeyCausesActivation,
                                bool aIsTrustedEvent);

  // nsIDOMElement specializations:
  NS_IMETHOD GetAttribute(const nsAString& aName,
                          nsAString& aReturn);
  NS_IMETHOD RemoveAttribute(const nsAString& aName);
  NS_IMETHOD HasAttribute(const nsAString& aName, bool* aReturn);
  
  // nsIClassInfo interface
  NS_DECL_NSICLASSINFO

  // nsIXPCScriptable interface
  NS_FORWARD_SAFE_NSIXPCSCRIPTABLE(GetBaseXPCClassInfo())

  // nsXPCClassInfo
  virtual void PreserveWrapper(nsISupports *aNative)
  {
    nsXPCClassInfo *ci = GetBaseXPCClassInfo();
    if (ci) {
      ci->PreserveWrapper(aNative);
    }
  }
  virtual PRUint32 GetInterfacesBitmap()
  {
    nsXPCClassInfo *ci = GetBaseXPCClassInfo();
    return ci ? ci->GetInterfacesBitmap() :  0;
  }

  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  nsresult CloneState(nsIDOMElement *aElement)
  {
    return GetXTFElement()->CloneState(aElement);
  }
  nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual void NodeInfoChanged(nsINodeInfo* aOldNodeInfo)
  {
  }

protected:
  virtual nsIXTFElement* GetXTFElement() const
  {
    return mXTFElement;
  }

  static nsXPCClassInfo* GetBaseXPCClassInfo()
  {
    return static_cast<nsXPCClassInfo*>(
      NS_GetDOMClassInfoInstance(eDOMClassInfo_Element_id));
  }

  // implementation helpers:  
  bool QueryInterfaceInner(REFNSIID aIID, void** result);

  bool HandledByInner(nsIAtom* attr) const;

  void RegUnregAccessKey(bool aDoReg);

  nsCOMPtr<nsIXTFElement> mXTFElement;

  PRUint32 mNotificationMask;
  nsCOMPtr<nsIXTFAttributeHandler> mAttributeHandler;

  /*
   * The intrinsic state of the element.
   * @see nsIContent::IntrinsicState()
   */
  nsEventStates mIntrinsicState;

  // Temporary owner used by GetAttrNameAt
  nsAttrName mTmpAttrName;

  nsCOMPtr<nsIAtom> mClassAttributeName;

  nsRefPtr<nsXTFClassInfo> mClassInfo;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXTFElementWrapper, NS_XTFELEMENTWRAPPER_IID)

class nsXTFClassInfo MOZ_FINAL : public nsXPCClassInfo
{
public:
  nsXTFClassInfo(nsXTFElementWrapper* aWrapper) : mWrapper(aWrapper) {}

  void Disconnect() { mWrapper = nsnull; }

  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSICLASSINFO(mWrapper);
  NS_FORWARD_SAFE_NSIXPCSCRIPTABLE(mWrapper);

  // nsXPCClassInfo
  virtual void PreserveWrapper(nsISupports* aNative)
  {
    if (mWrapper) {
      mWrapper->PreserveWrapper(aNative);
    }
  }

  virtual PRUint32 GetInterfacesBitmap()
  {
    return mWrapper ? mWrapper->GetInterfacesBitmap() : 0;
  }

private:
  nsXTFElementWrapper* mWrapper;  
};

nsresult
NS_NewXTFElementWrapper(nsIXTFElement* aXTFElement, already_AddRefed<nsINodeInfo> aNodeInfo,
                        nsIContent** aResult);

#endif // __NS_XTFELEMENTWRAPPER_H__
