/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/UniquePtr.h"
#include "nsAttrAndChildArray.h"          // member
#include "nsCycleCollectionParticipant.h" // NS_DECL_CYCLE_*
#include "nsIContent.h"                   // base class
#include "nsIWeakReference.h"             // base class
#include "nsNodeUtils.h"                  // class member nsNodeUtils::CloneNodeImpl
#include "nsIHTMLCollection.h"
#include "nsDataHashtable.h"
#include "nsXBLBinding.h"

class ContentUnbinder;
class nsContentList;
class nsLabelsNodeList;
class nsDOMAttributeMap;
class nsDOMTokenList;
class nsIControllers;
class nsICSSDeclaration;
class nsIDocument;
class nsDOMStringMap;
class nsIURI;

namespace mozilla {
class DeclarationBlock;
namespace dom {
struct CustomElementData;
class Element;
} // namespace dom
} // namespace mozilla

/**
 * A class that implements nsIWeakReference
 */

class nsNodeWeakReference final : public nsIWeakReference
{
public:
  explicit nsNodeWeakReference(nsINode* aNode)
    : nsIWeakReference(aNode)
  {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIWeakReference
  NS_DECL_NSIWEAKREFERENCE
  virtual size_t SizeOfOnlyThis(mozilla::MallocSizeOf aMallocSizeOf) const override;

  void NoticeNodeDestruction()
  {
    mObject = nullptr;
  }

private:
  ~nsNodeWeakReference();
};

/**
 * Tearoff to use for nodes to implement nsISupportsWeakReference
 */
class nsNodeSupportsWeakRefTearoff final : public nsISupportsWeakReference
{
public:
  explicit nsNodeSupportsWeakRefTearoff(nsINode* aNode)
    : mNode(aNode)
  {
  }

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsISupportsWeakReference
  NS_DECL_NSISUPPORTSWEAKREFERENCE

  NS_DECL_CYCLE_COLLECTION_CLASS(nsNodeSupportsWeakRefTearoff)

private:
  ~nsNodeSupportsWeakRefTearoff() {}

  nsCOMPtr<nsINode> mNode;
};

/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
namespace mozilla {
namespace dom {

class ShadowRoot;

class FragmentOrElement : public nsIContent
{
public:
  explicit FragmentOrElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo);
  explicit FragmentOrElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_ADDSIZEOFEXCLUDINGTHIS

  // nsINode interface methods
  virtual uint32_t GetChildCount() const override;
  virtual nsIContent *GetChildAt_Deprecated(uint32_t aIndex) const override;
  virtual int32_t IndexOf(const nsINode* aPossibleChild) const override;
  virtual nsresult InsertChildAt(nsIContent* aKid, uint32_t aIndex,
                                 bool aNotify) override;
  virtual void RemoveChildAt_Deprecated(uint32_t aIndex, bool aNotify) override;
  virtual void RemoveChildNode(nsIContent* aKid, bool aNotify) override;
  virtual void GetTextContentInternal(nsAString& aTextContent,
                                      mozilla::OOMReporter& aError) override;
  virtual void SetTextContentInternal(const nsAString& aTextContent,
                                      nsIPrincipal* aSubjectPrincipal,
                                      mozilla::ErrorResult& aError) override;

  // nsIContent interface methods
  virtual already_AddRefed<nsINodeList> GetChildren(uint32_t aFilter) override;
  virtual const nsTextFragment *GetText() override;
  virtual uint32_t TextLength() const override;
  virtual nsresult SetText(const char16_t* aBuffer, uint32_t aLength,
                           bool aNotify) override;
  // Need to implement this here too to avoid hiding.
  nsresult SetText(const nsAString& aStr, bool aNotify)
  {
    return SetText(aStr.BeginReading(), aStr.Length(), aNotify);
  }
  virtual nsresult AppendText(const char16_t* aBuffer, uint32_t aLength,
                              bool aNotify) override;
  virtual bool TextIsOnlyWhitespace() override;
  virtual bool ThreadSafeTextIsOnlyWhitespace() const override;
  virtual bool HasTextForTranslation() override;
  virtual void AppendTextTo(nsAString& aResult) override;
  MOZ_MUST_USE
  virtual bool AppendTextTo(nsAString& aResult, const mozilla::fallible_t&) override;
  virtual nsXBLBinding* DoGetXBLBinding() const override;
  virtual bool IsLink(nsIURI** aURI) const override;

  virtual void DestroyContent() override;
  virtual void SaveSubtreeState() override;

  nsIHTMLCollection* Children();
  uint32_t ChildElementCount()
  {
    return Children()->Length();
  }

  /**
   * Sets the IsElementInStyleScope flag on each element in the subtree rooted
   * at this node, including any elements reachable through shadow trees.
   *
   * @param aInStyleScope The flag value to set.
   */
  void SetIsElementInStyleScopeFlagOnSubtree(bool aInStyleScope);

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

  virtual bool OwnedOnlyByTheDOMTree() override
  {
    uint32_t rc = mRefCnt.get();
    if (GetParent()) {
      --rc;
    }
    rc -= mAttrsAndChildren.ChildCount();
    return rc == 0;
  }

  virtual bool IsPurple() override
  {
    return mRefCnt.IsPurple();
  }

  virtual void RemovePurple() override
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
  static void MarkUserData(void* aObject, nsAtom* aKey, void* aChild,
                           void *aData);

  /**
   * Is the HTML local name a void element?
   */
  static bool IsHTMLVoid(nsAtom* aLocalName);
protected:
  virtual ~FragmentOrElement();

  /**
   * Copy attributes and state to another element
   * @param aDest the object to copy to
   */
  nsresult CopyInnerTo(FragmentOrElement* aDest, bool aPreallocateChildren);

public:
  /**
   * There are a set of DOM- and scripting-specific instance variables
   * that may only be instantiated when a content object is accessed
   * through the DOM. Rather than burn actual slots in the content
   * objects for each of these instance variables, we put them off
   * in a side structure that's only allocated when the content is
   * accessed through the DOM.
   */

  class nsExtendedDOMSlots final : public nsIContent::nsExtendedContentSlots
  {
  public:
    nsExtendedDOMSlots();
    ~nsExtendedDOMSlots() final;

    void Traverse(nsCycleCollectionTraversalCallback&) final override;
    void Unlink() final override;

    /**
     * SMIL Overridde style rules (for SMIL animation of CSS properties)
     * @see Element::GetSMILOverrideStyle
     */
    nsCOMPtr<nsICSSDeclaration> mSMILOverrideStyle;

    /**
     * Holds any SMIL override style declaration for this element.
     */
    RefPtr<mozilla::DeclarationBlock> mSMILOverrideStyleDeclaration;

    /**
    * The controllers of the XUL Element.
    */
    nsCOMPtr<nsIControllers> mControllers;

    /**
     * An object implementing the .labels property for this element.
     */
    RefPtr<nsLabelsNodeList> mLabelsList;

    /**
     * ShadowRoot bound to the element.
     */
    RefPtr<ShadowRoot> mShadowRoot;

    /**
     * XBL binding installed on the element.
     */
    RefPtr<nsXBLBinding> mXBLBinding;

    /**
     * Web components custom element data.
     */
    RefPtr<CustomElementData> mCustomElementData;

    /**
     * For XUL to hold either frameloader or opener.
     */
    nsCOMPtr<nsISupports> mFrameLoaderOrOpener;

  };

  class nsDOMSlots final : public nsIContent::nsContentSlots
  {
  public:
    nsDOMSlots();
    ~nsDOMSlots() final;

    void Traverse(nsCycleCollectionTraversalCallback&) final override;
    void Unlink() final override;

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
     * An object implementing nsIDOMMozNamedAttrMap for this content (attributes)
     * @see FragmentOrElement::GetAttributes
     */
    RefPtr<nsDOMAttributeMap> mAttributeMap;

    /**
     * An object implementing the .children property for this element.
     */
    RefPtr<nsContentList> mChildrenList;

    /**
     * An object implementing the .classList property for this element.
     */
    RefPtr<nsDOMTokenList> mClassList;
  };

protected:
  void GetMarkup(bool aIncludeSelf, nsAString& aMarkup);
  void SetInnerHTMLInternal(const nsAString& aInnerHTML, ErrorResult& aError);

  // Override from nsINode
  nsIContent::nsContentSlots* CreateSlots() override
  {
    return new nsDOMSlots();
  }

  nsIContent::nsExtendedContentSlots* CreateExtendedSlots() final override
  {
    return new nsExtendedDOMSlots();
  }

  nsDOMSlots* DOMSlots()
  {
    return static_cast<nsDOMSlots*>(Slots());
  }

  nsDOMSlots *GetExistingDOMSlots() const
  {
    return static_cast<nsDOMSlots*>(GetExistingSlots());
  }

  nsExtendedDOMSlots* ExtendedDOMSlots()
  {
    return static_cast<nsExtendedDOMSlots*>(ExtendedContentSlots());
  }

  const nsExtendedDOMSlots* GetExistingExtendedDOMSlots() const
  {
    return static_cast<const nsExtendedDOMSlots*>(
      GetExistingExtendedContentSlots());
  }

  nsExtendedDOMSlots* GetExistingExtendedDOMSlots()
  {
    return static_cast<nsExtendedDOMSlots*>(GetExistingExtendedContentSlots());
  }

  /**
   * Calls SetIsElementInStyleScopeFlagOnSubtree for each shadow tree attached
   * to this node, which is assumed to be an Element.
   *
   * @param aInStyleScope The IsElementInStyleScope flag value to set.
   */
  void SetIsElementInStyleScopeFlagOnShadowTree(bool aInStyleScope);

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
