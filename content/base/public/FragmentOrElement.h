/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all element classes as well as nsDocumentFragment.  This
 * provides an implementation of nsIDOMNode, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#ifndef FragmentOrElement_h___
#define FragmentOrElement_h___

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "nsAttrAndChildArray.h"          // member
#include "nsCycleCollectionParticipant.h" // NS_DECL_CYCLE_*
#include "nsIContent.h"                   // base class
#include "nsIDOMXPathNSResolver.h"        // base class
#include "nsINodeList.h"                  // base class
#include "nsIWeakReference.h"             // base class
#include "nsNodeUtils.h"                  // class member nsNodeUtils::CloneNodeImpl
#include "nsIHTMLCollection.h"

class ContentUnbinder;
class nsContentList;
class nsDOMAttributeMap;
class nsDOMTokenList;
class nsIControllers;
class nsICSSDeclaration;
class nsIDocument;
class nsDOMStringMap;
class nsINodeInfo;
class nsIURI;

/**
 * Class that implements the nsIDOMNodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating GetLength
 * and Item to its existing child list.
 * @see nsIDOMNodeList
 */
class nsChildContentList MOZ_FINAL : public nsINodeList
{
public:
  nsChildContentList(nsINode* aNode)
    : mNode(aNode)
  {
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(nsChildContentList)

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext *cx,
                               JS::Handle<JSObject*> scope) MOZ_OVERRIDE;

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST

  // nsINodeList interface
  virtual int32_t IndexOf(nsIContent* aContent) MOZ_OVERRIDE;
  virtual nsIContent* Item(uint32_t aIndex) MOZ_OVERRIDE;

  void DropReference()
  {
    mNode = nullptr;
  }

  virtual nsINode* GetParentObject() MOZ_OVERRIDE
  {
    return mNode;
  }

private:
  // The node whose children make up the list (weak reference)
  nsINode* mNode;
};

/**
 * A tearoff class for FragmentOrElement to implement additional interfaces
 */
class nsNode3Tearoff : public nsIDOMXPathNSResolver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNode3Tearoff)

  NS_DECL_NSIDOMXPATHNSRESOLVER

  nsNode3Tearoff(nsINode *aNode) : mNode(aNode)
  {
  }

protected:
  virtual ~nsNode3Tearoff() {}

private:
  nsCOMPtr<nsINode> mNode;
};

/**
 * A class that implements nsIWeakReference
 */

class nsNodeWeakReference MOZ_FINAL : public nsIWeakReference
{
public:
  nsNodeWeakReference(nsINode* aNode)
    : mNode(aNode)
  {
  }

  ~nsNodeWeakReference();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWeakReference
  NS_DECL_NSIWEAKREFERENCE

  void NoticeNodeDestruction()
  {
    mNode = nullptr;
  }

private:
  nsINode* mNode;
};

/**
 * Tearoff to use for nodes to implement nsISupportsWeakReference
 */
class nsNodeSupportsWeakRefTearoff MOZ_FINAL : public nsISupportsWeakReference
{
public:
  nsNodeSupportsWeakRefTearoff(nsINode* aNode)
    : mNode(aNode)
  {
  }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsISupportsWeakReference
  NS_DECL_NSISUPPORTSWEAKREFERENCE

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNodeSupportsWeakRefTearoff)

private:
  nsCOMPtr<nsINode> mNode;
};

/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
namespace mozilla {
namespace dom {

class ShadowRoot;
class UndoManager;

class FragmentOrElement : public nsIContent
{
public:
  FragmentOrElement(already_AddRefed<nsINodeInfo>& aNodeInfo);
  FragmentOrElement(already_AddRefed<nsINodeInfo>&& aNodeInfo);
  virtual ~FragmentOrElement();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_SIZEOF_EXCLUDING_THIS

  // nsINode interface methods
  virtual uint32_t GetChildCount() const MOZ_OVERRIDE;
  virtual nsIContent *GetChildAt(uint32_t aIndex) const MOZ_OVERRIDE;
  virtual nsIContent * const * GetChildArray(uint32_t* aChildCount) const MOZ_OVERRIDE;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const MOZ_OVERRIDE;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) MOZ_OVERRIDE;
  virtual void RemoveChildAt(uint32_t aIndex, bool aNotify) MOZ_OVERRIDE;
  virtual void GetTextContentInternal(nsAString& aTextContent) MOZ_OVERRIDE;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      mozilla::ErrorResult& aError) MOZ_OVERRIDE;

  // nsIContent interface methods
  virtual already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) MOZ_OVERRIDE;
  virtual const nsTextFragment *GetText() MOZ_OVERRIDE;
  virtual uint32_t TextLength() const MOZ_OVERRIDE;
  virtual nsresult SetText(const char16_t* aBuffer, uint32_t aLength,
                           bool aNotify) MOZ_OVERRIDE;
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, bool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const char16_t* aBuffer, uint32_t aLength,
                              bool aNotify) MOZ_OVERRIDE;
  virtual bool TextIsOnlyWhitespace() MOZ_OVERRIDE;
  virtual void AppendTextTo(nsAString& aResult) MOZ_OVERRIDE;
  virtual nsIContent *GetBindingParent() const MOZ_OVERRIDE;
  virtual nsXBLBinding *GetXBLBinding() const MOZ_OVERRIDE;
  virtual void SetXBLBinding(nsXBLBinding* aBinding,
                             nsBindingManager* aOldBindingManager = nullptr) MOZ_OVERRIDE;
  virtual ShadowRoot *GetShadowRoot() const MOZ_OVERRIDE;
  virtual ShadowRoot *GetContainingShadow() const MOZ_OVERRIDE;
  virtual void SetShadowRoot(ShadowRoot* aBinding) MOZ_OVERRIDE;
  virtual nsIContent *GetXBLInsertionParent() const MOZ_OVERRIDE;
  virtual void SetXBLInsertionParent(nsIContent* aContent) MOZ_OVERRIDE;
  virtual bool IsLink(nsIURI** aURI) const MOZ_OVERRIDE;

  virtual CustomElementData *GetCustomElementData() const MOZ_OVERRIDE;
  virtual void SetCustomElementData(CustomElementData* aData) MOZ_OVERRIDE;

  virtual void DestroyContent() MOZ_OVERRIDE;
  virtual void SaveSubtreeState() MOZ_OVERRIDE;

  virtual const nsAttrValue* DoGetClasses() const MOZ_OVERRIDE;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker) MOZ_OVERRIDE;

  nsIHTMLCollection* Children();
  uint32_t ChildElementCount()
  {
    return Children()->Length();
  }

public:
  /**
   * If there are listeners for DOMNodeInserted event, fires the event on all
   * aNodes
   */
  static void FireNodeInserted(nsIDocument* aDoc,
                               nsINode* aParent,
                               nsTArray<nsCOMPtr<nsIContent> >& aNodes);

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(FragmentOrElement)

  /**
   * Fire a DOMNodeRemoved mutation event for all children of this node
   */
  void FireNodeRemovedForChildren();

  virtual bool OwnedOnlyByTheDOMTree() MOZ_OVERRIDE
  {
    uint32_t rc = mRefCnt.get();
    if (GetParent()) {
      --rc;
    }
    rc -= mAttrsAndChildren.ChildCount();
    return rc == 0;
  }

  virtual bool IsPurple() MOZ_OVERRIDE
  {
    return mRefCnt.IsPurple();
  }

  virtual void RemovePurple() MOZ_OVERRIDE
  {
    mRefCnt.RemovePurple();
  }

  static void ClearContentUnbinder();
  static bool CanSkip(nsINode* aNode, bool aRemovingAllowed);
  static bool CanSkipInCC(nsINode* aNode);
  static bool CanSkipThis(nsINode* aNode);
  static void RemoveBlackMarkedNode(nsINode* aNode);
  static void MarkNodeChildren(nsINode* aNode);
  static void InitCCCallbacks();
  static void MarkUserData(void* aObject, nsIAtom* aKey, void* aChild,
                           void *aData);
  static void MarkUserDataHandler(void* aObject, nsIAtom* aKey, void* aChild,
                                  void* aData);

protected:
  /**
   * Copy attributes and state to another element
   * @param aDest the object to copy to
   */
  nsresult CopyInnerTo(FragmentOrElement* aDest);

public:
  // Because of a bug in MS C++ compiler nsDOMSlots must be declared public,
  // otherwise nsXULElement::nsXULSlots doesn't compile.
  /**
   * There are a set of DOM- and scripting-specific instance variables
   * that may only be instantiated when a content object is accessed
   * through the DOM. Rather than burn actual slots in the content
   * objects for each of these instance variables, we put them off
   * in a side structure that's only allocated when the content is
   * accessed through the DOM.
   */
  class nsDOMSlots : public nsINode::nsSlots
  {
  public:
    nsDOMSlots();
    virtual ~nsDOMSlots();

    void Traverse(nsCycleCollectionTraversalCallback &cb, bool aIsXUL);
    void Unlink(bool aIsXUL);

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    /**
     * The .style attribute (an interface that forwards to the actual
     * style rules)
     * @see nsGenericHTMLElement::GetStyle
     */
    nsCOMPtr<nsICSSDeclaration> mStyle;

    /**
     * The .dataset attribute.
     * @see nsGenericHTMLElement::GetDataset
     */
    nsDOMStringMap* mDataset; // [Weak]

    /**
     * The .undoManager property.
     * @see nsGenericHTMLElement::GetUndoManager
     */
    nsRefPtr<UndoManager> mUndoManager;

    /**
     * SMIL Overridde style rules (for SMIL animation of CSS properties)
     * @see nsIContent::GetSMILOverrideStyle
     */
    nsCOMPtr<nsICSSDeclaration> mSMILOverrideStyle;

    /**
     * Holds any SMIL override style rules for this element.
     */
    nsRefPtr<mozilla::css::StyleRule> mSMILOverrideStyleRule;

    /**
     * An object implementing nsIDOMMozNamedAttrMap for this content (attributes)
     * @see FragmentOrElement::GetAttributes
     */
    nsRefPtr<nsDOMAttributeMap> mAttributeMap;

    union {
      /**
      * The nearest enclosing content node with a binding that created us.
      * @see FragmentOrElement::GetBindingParent
      */
      nsIContent* mBindingParent;  // [Weak]

      /**
      * The controllers of the XUL Element.
      */
      nsIControllers* mControllers; // [OWNER]
    };

    /**
     * An object implementing the .children property for this element.
     */
    nsRefPtr<nsContentList> mChildrenList;

    /**
     * An object implementing the .classList property for this element.
     */
    nsRefPtr<nsDOMTokenList> mClassList;

    /**
     * ShadowRoot bound to the element.
     */
    nsRefPtr<ShadowRoot> mShadowRoot;

    /**
     * The root ShadowRoot of this element if it is in a shadow tree.
     */
    nsRefPtr<ShadowRoot> mContainingShadow;

    /**
     * XBL binding installed on the element.
     */
    nsRefPtr<nsXBLBinding> mXBLBinding;

    /**
     * XBL binding installed on the lement.
     */
    nsCOMPtr<nsIContent> mXBLInsertionParent;

    /**
     * Web components custom element data.
     */
    nsAutoPtr<CustomElementData> mCustomElementData;
  };

protected:
  void GetMarkup(bool aIncludeSelf, nsAString& aMarkup);
  void SetInnerHTMLInternal(const nsAString& aInnerHTML, ErrorResult& aError);

  // Override from nsINode
  virtual nsINode::nsSlots* CreateSlots() MOZ_OVERRIDE;

  nsDOMSlots *DOMSlots()
  {
    return static_cast<nsDOMSlots*>(Slots());
  }

  nsDOMSlots *GetExistingDOMSlots() const
  {
    return static_cast<nsDOMSlots*>(GetExistingSlots());
  }

  friend class ::ContentUnbinder;
  /**
   * Array containing all attributes and children for this element
   */
  nsAttrAndChildArray mAttrsAndChildren;
};

} // namespace dom
} // namespace mozilla

#define NS_ELEMENT_INTERFACE_TABLE_TO_MAP_SEGUE                               \
    if (NS_SUCCEEDED(rv))                                                     \
      return rv;                                                              \
                                                                              \
    rv = FragmentOrElement::QueryInterface(aIID, aInstancePtr);               \
    NS_INTERFACE_TABLE_TO_MAP_SEGUE

#endif /* FragmentOrElement_h___ */
