/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _Accessible_H_
#define _Accessible_H_

#include "mozilla/a11y/AccTypes.h"
#include "mozilla/a11y/RelationType.h"
#include "mozilla/a11y/Role.h"
#include "mozilla/a11y/States.h"

#include "mozilla/UniquePtr.h"

#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsRefPtrHashtable.h"
#include "nsRect.h"

struct nsRoleMapEntry;

struct nsRect;
class nsIFrame;
class nsIAtom;
class nsIPersistentProperties;

namespace mozilla {
namespace a11y {

class Accessible;
class AccEvent;
class AccGroupInfo;
class ApplicationAccessible;
class DocAccessible;
class EmbeddedObjCollector;
class EventTree;
class HTMLImageMapAccessible;
class HTMLLIAccessible;
class HyperTextAccessible;
class ImageAccessible;
class KeyBinding;
class OuterDocAccessible;
class ProxyAccessible;
class Relation;
class RootAccessible;
class TableAccessible;
class TableCellAccessible;
class TextLeafAccessible;
class XULLabelAccessible;
class XULTreeAccessible;

#ifdef A11Y_LOG
namespace logging {
  typedef const char* (*GetTreePrefix)(void* aData, Accessible*);
  void Tree(const char* aTitle, const char* aMsgText, Accessible* aRoot,
            GetTreePrefix aPrefixFunc, void* GetTreePrefixData);
};
#endif

/**
 * Name type flags.
 */
enum ENameValueFlag {
  /**
   * Name either
   *  a) present (not empty): !name.IsEmpty()
   *  b) no name (was missed): name.IsVoid()
   */
 eNameOK,

 /**
  * Name was left empty by the author on purpose:
  * name.IsEmpty() && !name.IsVoid().
  */
 eNoNameOnPurpose,

 /**
  * Name was computed from the subtree.
  */
 eNameFromSubtree,

 /**
  * Tooltip was used as a name.
  */
 eNameFromTooltip
};

/**
 * Group position (level, position in set and set size).
 */
struct GroupPos
{
  GroupPos() : level(0), posInSet(0), setSize(0) { }
  GroupPos(int32_t aLevel, int32_t aPosInSet, int32_t aSetSize) :
    level(aLevel), posInSet(aPosInSet), setSize(aSetSize) { }

  int32_t level;
  int32_t posInSet;
  int32_t setSize;
};

/**
 * An index type. Assert if out of range value was attempted to be used.
 */
class index_t
{
public:
  MOZ_IMPLICIT index_t(int32_t aVal) : mVal(aVal) {}

  operator uint32_t() const
  {
    MOZ_ASSERT(mVal >= 0, "Attempt to use wrong index!");
    return mVal;
  }

  bool IsValid() const { return mVal >= 0; }

private:
  int32_t mVal;
};

typedef nsRefPtrHashtable<nsPtrHashKey<const void>, Accessible>
  AccessibleHashtable;


#define NS_ACCESSIBLE_IMPL_IID                          \
{  /* 133c8bf4-4913-4355-bd50-426bd1d6e1ad */           \
  0x133c8bf4,                                           \
  0x4913,                                               \
  0x4355,                                               \
  { 0xbd, 0x50, 0x42, 0x6b, 0xd1, 0xd6, 0xe1, 0xad }    \
}

class Accessible : public nsISupports
{
public:
  Accessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(Accessible)

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ACCESSIBLE_IMPL_IID)

  //////////////////////////////////////////////////////////////////////////////
  // Public methods

  /**
   * Return the document accessible for this accessible.
   */
  DocAccessible* Document() const { return mDoc; }

  /**
   * Return the root document accessible for this accessible.
   */
  a11y::RootAccessible* RootAccessible() const;

  /**
   * Return frame for this accessible.
   */
  virtual nsIFrame* GetFrame() const;

  /**
   * Return DOM node associated with the accessible.
   */
  virtual nsINode* GetNode() const;
  inline already_AddRefed<nsIDOMNode> DOMNode() const
  {
    nsCOMPtr<nsIDOMNode> DOMNode = do_QueryInterface(GetNode());
    return DOMNode.forget();
  }
  nsIContent* GetContent() const { return mContent; }
  mozilla::dom::Element* Elm() const
    { return mContent && mContent->IsElement() ? mContent->AsElement() : nullptr; }

  /**
   * Return node type information of DOM node associated with the accessible.
   */
  bool IsContent() const
    { return GetNode() && GetNode()->IsNodeOfType(nsINode::eCONTENT); }

  /**
   * Return the unique identifier of the accessible.
   */
  void* UniqueID() { return static_cast<void*>(this); }

  /**
   * Return language associated with the accessible.
   */
  void Language(nsAString& aLocale);

  /**
   * Get the description of this accessible.
   */
  virtual void Description(nsString& aDescription);

  /**
   * Get the value of this accessible.
   */
  virtual void Value(nsString& aValue);

  /**
   * Get help string for the accessible.
   */
  void Help(nsString& aHelp) const { aHelp.Truncate(); }

  /**
   * Get the name of this accessible.
   *
   * Note: aName.IsVoid() when name was left empty by the author on purpose.
   * aName.IsEmpty() when the author missed name, AT can try to repair a name.
   */
  virtual ENameValueFlag Name(nsString& aName);

  /**
   * Maps ARIA state attributes to state of accessible. Note the given state
   * argument should hold states for accessible before you pass it into this
   * method.
   *
   * @param  [in/out] where to fill the states into.
   */
  virtual void ApplyARIAState(uint64_t* aState) const;

  /**
   * Return enumerated accessible role (see constants in Role.h).
   */
  mozilla::a11y::role Role();

  /**
   * Return true if ARIA role is specified on the element.
   */
  bool HasARIARole() const;
  bool IsARIARole(nsIAtom* aARIARole) const;
  bool HasStrongARIARole() const;

  /**
   * Retrun ARIA role map if any.
   */
  const nsRoleMapEntry* ARIARoleMap() const;

  /**
   * Return accessible role specified by ARIA (see constants in
   * roles).
   */
  mozilla::a11y::role ARIARole();

  /**
   * Return a landmark role if applied.
   */
  virtual nsIAtom* LandmarkRole() const;

  /**
   * Returns enumerated accessible role from native markup (see constants in
   * Role.h). Doesn't take into account ARIA roles.
   */
  virtual mozilla::a11y::role NativeRole();

  /**
   * Return all states of accessible (including ARIA states).
   */
  virtual uint64_t State();

  /**
   * Return interactive states present on the accessible
   * (@see NativeInteractiveState).
   */
  uint64_t InteractiveState() const
  {
    uint64_t state = NativeInteractiveState();
    ApplyARIAState(&state);
    return state;
  }

  /**
   * Return link states present on the accessible.
   */
  uint64_t LinkState() const
  {
    uint64_t state = NativeLinkState();
    ApplyARIAState(&state);
    return state;
  }

  /**
   * Return if accessible is unavailable.
   */
  bool Unavailable() const
  {
    uint64_t state = NativelyUnavailable() ? states::UNAVAILABLE : 0;
    ApplyARIAState(&state);
    return state & states::UNAVAILABLE;
  }

  /**
   * Return the states of accessible, not taking into account ARIA states.
   * Use State() to get complete set of states.
   */
  virtual uint64_t NativeState();

  /**
   * Return native interactice state (unavailable, focusable or selectable).
   */
  virtual uint64_t NativeInteractiveState() const;

  /**
   * Return native link states present on the accessible.
   */
  virtual uint64_t NativeLinkState() const;

  /**
   * Return bit set of invisible and offscreen states.
   */
  uint64_t VisibilityState();

  /**
   * Return true if native unavailable state present.
   */
  virtual bool NativelyUnavailable() const;

  /**
   * Return object attributes for the accessible.
   */
  virtual already_AddRefed<nsIPersistentProperties> Attributes();

  /**
   * Return group position (level, position in set and set size).
   */
  virtual mozilla::a11y::GroupPos GroupPosition();

  /**
   * Used by ChildAtPoint() method to get direct or deepest child at point.
   */
  enum EWhichChildAtPoint {
    eDirectChild,
    eDeepestChild
  };

  /**
   * Return direct or deepest child at the given point.
   *
   * @param  aX           [in] x coordinate relative screen
   * @param  aY           [in] y coordinate relative screen
   * @param  aWhichChild  [in] flag points if deepest or direct child
   *                        should be returned
   */
  virtual Accessible* ChildAtPoint(int32_t aX, int32_t aY,
                                   EWhichChildAtPoint aWhichChild);

  /**
   * Return the focused child if any.
   */
  virtual Accessible* FocusedChild();

  /**
   * Return calculated group level based on accessible hierarchy.
   */
  virtual int32_t GetLevelInternal();

  /**
   * Calculate position in group and group size ('posinset' and 'setsize') based
   * on accessible hierarchy.
   *
   * @param  aPosInSet  [out] accessible position in the group
   * @param  aSetSize   [out] the group size
   */
  virtual void GetPositionAndSizeInternal(int32_t *aPosInSet,
                                          int32_t *aSetSize);

  /**
   * Get the relation of the given type.
   */
  virtual Relation RelationByType(RelationType aType);

  //////////////////////////////////////////////////////////////////////////////
  // Initializing methods

  /**
   * Shutdown this accessible object.
   */
  virtual void Shutdown();

  /**
   * Set the ARIA role map entry for a new accessible.
   */
  void SetRoleMapEntry(const nsRoleMapEntry* aRoleMapEntry);

  /**
   * Append/insert/remove a child. Return true if operation was successful.
   */
  bool AppendChild(Accessible* aChild)
    { return InsertChildAt(mChildren.Length(), aChild); }
  virtual bool InsertChildAt(uint32_t aIndex, Accessible* aChild);

  /**
   * Inserts a child after given sibling. If the child cannot be inserted,
   * then the child is unbound from the document, and false is returned. Make
   * sure to null out any references on the child object as it may be destroyed.
   */
  bool InsertAfter(Accessible* aNewChild, Accessible* aRefChild);

  virtual bool RemoveChild(Accessible* aChild);

  /**
   * Reallocates the child withing its parent.
   */
  void MoveChild(uint32_t aNewIndex, Accessible* aChild);

  //////////////////////////////////////////////////////////////////////////////
  // Accessible tree traverse methods

  /**
   * Return parent accessible.
   */
  Accessible* Parent() const { return mParent; }

  /**
   * Return child accessible at the given index.
   */
  virtual Accessible* GetChildAt(uint32_t aIndex) const;

  /**
   * Return child accessible count.
   */
  virtual uint32_t ChildCount() const;

  /**
   * Return index of the given child accessible.
   */
  int32_t GetIndexOf(const Accessible* aChild) const
    { return (aChild->mParent != this) ? -1 : aChild->IndexInParent(); }

  /**
   * Return index in parent accessible.
   */
  virtual int32_t IndexInParent() const;

  /**
   * Return true if accessible has children;
   */
  bool HasChildren() { return !!GetChildAt(0); }

  /**
   * Return first/last/next/previous sibling of the accessible.
   */
  inline Accessible* NextSibling() const
    {  return GetSiblingAtOffset(1); }
  inline Accessible* PrevSibling() const
    { return GetSiblingAtOffset(-1); }
  inline Accessible* FirstChild()
    { return GetChildAt(0); }
  inline Accessible* LastChild()
  {
    uint32_t childCount = ChildCount();
    return childCount != 0 ? GetChildAt(childCount - 1) : nullptr;
  }

  /**
   * Return embedded accessible children count.
   */
  uint32_t EmbeddedChildCount();

  /**
   * Return embedded accessible child at the given index.
   */
  Accessible* GetEmbeddedChildAt(uint32_t aIndex);

  /**
   * Return index of the given embedded accessible child.
   */
  int32_t GetIndexOfEmbeddedChild(Accessible* aChild);

  /**
   * Return number of content children/content child at index. The content
   * child is created from markup in contrast to it's never constructed by its
   * parent accessible (like treeitem accessibles for XUL trees).
   */
  uint32_t ContentChildCount() const { return mChildren.Length(); }
  Accessible* ContentChildAt(uint32_t aIndex) const
    { return mChildren.ElementAt(aIndex); }

  /**
   * Return true if the accessible is attached to tree.
   */
  bool IsBoundToParent() const { return !!mParent; }

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous methods

  /**
   * Handle accessible event, i.e. process it, notifies observers and fires
   * platform specific event.
   */
  virtual nsresult HandleAccEvent(AccEvent* aAccEvent);

  /**
   * Return true if the accessible is an acceptable child.
   */
  virtual bool IsAcceptableChild(nsIContent* aEl) const
    { return true; }

  /**
   * Returns text of accessible if accessible has text role otherwise empty
   * string.
   *
   * @param aText         [in] returned text of the accessible
   * @param aStartOffset  [in, optional] start offset inside of the accessible,
   *                        if missed entire text is appended
   * @param aLength       [in, optional] required length of text, if missed
   *                        then text form start offset till the end is appended
   */
  virtual void AppendTextTo(nsAString& aText, uint32_t aStartOffset = 0,
                            uint32_t aLength = UINT32_MAX);

  /**
   * Return boundaries in screen coordinates.
   */
  virtual nsIntRect Bounds() const;

  /**
   * Return boundaries rect relative the bounding frame.
   */
  virtual nsRect RelativeBounds(nsIFrame** aRelativeFrame) const;

  /**
   * Selects the accessible within its container if applicable.
   */
  virtual void SetSelected(bool aSelect);

  /**
   * Select the accessible within its container.
   */
  void TakeSelection();

  /**
   * Focus the accessible.
   */
  virtual void TakeFocus();

  /**
   * Scroll the accessible into view.
   */
  void ScrollTo(uint32_t aHow) const;

  /**
   * Scroll the accessible to the given point.
   */
  void ScrollToPoint(uint32_t aCoordinateType, int32_t aX, int32_t aY);

  /**
   * Get a pointer to accessibility interface for this node, which is specific
   * to the OS/accessibility toolkit we're running on.
   */
  virtual void GetNativeInterface(void** aNativeAccessible);

  //////////////////////////////////////////////////////////////////////////////
  // Downcasting and types

  inline bool IsAbbreviation() const
  {
    return mContent->IsAnyOfHTMLElements(nsGkAtoms::abbr, nsGkAtoms::acronym);
  }

  bool IsAlert() const { return HasGenericType(eAlert); }

  bool IsApplication() const { return mType == eApplicationType; }
  ApplicationAccessible* AsApplication();

  bool IsAutoComplete() const { return HasGenericType(eAutoComplete); }

  bool IsAutoCompletePopup() const
    { return HasGenericType(eAutoCompletePopup); }

  bool IsButton() const { return HasGenericType(eButton); }

  bool IsCombobox() const { return HasGenericType(eCombobox); }

  bool IsDoc() const { return HasGenericType(eDocument); }
  DocAccessible* AsDoc();

  bool IsGenericHyperText() const { return mType == eHyperTextType; }
  bool IsHyperText() const { return HasGenericType(eHyperText); }
  HyperTextAccessible* AsHyperText();

  bool IsHTMLBr() const { return mType == eHTMLBRType; }
  bool IsHTMLCaption() const { return mType == eHTMLCaptionType; }
  bool IsHTMLCombobox() const { return mType == eHTMLComboboxType; }
  bool IsHTMLFileInput() const { return mType == eHTMLFileInputType; }

  bool IsHTMLListItem() const { return mType == eHTMLLiType; }
  HTMLLIAccessible* AsHTMLListItem();

  bool IsHTMLOptGroup() const { return mType == eHTMLOptGroupType; }

  bool IsHTMLTable() const { return mType == eHTMLTableType; }
  bool IsHTMLTableRow() const { return mType == eHTMLTableRowType; }

  bool IsImage() const { return mType == eImageType; }
  ImageAccessible* AsImage();

  bool IsImageMap() const { return mType == eImageMapType; }
  HTMLImageMapAccessible* AsImageMap();

  bool IsList() const { return HasGenericType(eList); }

  bool IsListControl() const { return HasGenericType(eListControl); }

  bool IsMenuButton() const { return HasGenericType(eMenuButton); }

  bool IsMenuPopup() const { return mType == eMenuPopupType; }

  bool IsProxy() const { return mType == eProxyType; }
  ProxyAccessible* Proxy() const
  {
    MOZ_ASSERT(IsProxy());
    return mBits.proxy;
  }
  uint32_t ProxyInterfaces() const
  {
    MOZ_ASSERT(IsProxy());
    return mInt.mProxyInterfaces;
  }
  void SetProxyInterfaces(uint32_t aInterfaces)
  {
    MOZ_ASSERT(IsProxy());
    mInt.mProxyInterfaces = aInterfaces;
  }

  bool IsOuterDoc() const { return mType == eOuterDocType; }
  OuterDocAccessible* AsOuterDoc();

  bool IsProgress() const { return mType == eProgressType; }

  bool IsRoot() const { return mType == eRootType; }
  a11y::RootAccessible* AsRoot();

  bool IsSearchbox() const;

  bool IsSelect() const { return HasGenericType(eSelect); }

  bool IsTable() const { return HasGenericType(eTable); }
  virtual TableAccessible* AsTable() { return nullptr; }

  bool IsTableCell() const { return HasGenericType(eTableCell); }
  virtual TableCellAccessible* AsTableCell() { return nullptr; }
  const TableCellAccessible* AsTableCell() const
    { return const_cast<Accessible*>(this)->AsTableCell(); }

  bool IsTableRow() const { return HasGenericType(eTableRow); }

  bool IsTextField() const { return mType == eHTMLTextFieldType; }

  bool IsText() const { return mGenericTypes & eText; }

  bool IsTextLeaf() const { return mType == eTextLeafType; }
  TextLeafAccessible* AsTextLeaf();

  bool IsXULLabel() const { return mType == eXULLabelType; }
  XULLabelAccessible* AsXULLabel();

  bool IsXULListItem() const { return mType == eXULListItemType; }

  bool IsXULTabpanels() const { return mType == eXULTabpanelsType; }

  bool IsXULTree() const { return mType == eXULTreeType; }
  XULTreeAccessible* AsXULTree();

  /**
   * Return true if the accessible belongs to the given accessible type.
   */
  bool HasGenericType(AccGenericType aType) const;

  //////////////////////////////////////////////////////////////////////////////
  // ActionAccessible

  /**
   * Return the number of actions that can be performed on this accessible.
   */
  virtual uint8_t ActionCount();

  /**
   * Return action name at given index.
   */
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName);

  /**
   * Default to localized action name.
   */
  void ActionDescriptionAt(uint8_t aIndex, nsAString& aDescription)
  {
    nsAutoString name;
    ActionNameAt(aIndex, name);
    TranslateString(name, aDescription);
  }

  /**
   * Invoke the accessible action.
   */
  virtual bool DoAction(uint8_t aIndex);

  /**
   * Return access key, such as Alt+D.
   */
  virtual KeyBinding AccessKey() const;

  /**
   * Return global keyboard shortcut for default action, such as Ctrl+O for
   * Open file menuitem.
   */
  virtual KeyBinding KeyboardShortcut() const;

  //////////////////////////////////////////////////////////////////////////////
  // HyperLinkAccessible (any embedded object in text can implement HyperLink,
  // which helps determine where it is located within containing text).

  /**
   * Return true if the accessible is hyper link accessible.
   */
  virtual bool IsLink();

  /**
   * Return the start offset of the link within the parent accessible.
   */
  virtual uint32_t StartOffset();

  /**
   * Return the end offset of the link within the parent accessible.
   */
  virtual uint32_t EndOffset();

  /**
   * Return true if the link is valid (e. g. points to a valid URL).
   */
  inline bool IsLinkValid()
  {
    NS_PRECONDITION(IsLink(), "IsLinkValid is called on not hyper link!");

    // XXX In order to implement this we would need to follow every link
    // Perhaps we can get information about invalid links from the cache
    // In the mean time authors can use role="link" aria-invalid="true"
    // to force it for links they internally know to be invalid
    return (0 == (State() & mozilla::a11y::states::INVALID));
  }

  /**
   * Return the number of anchors within the link.
   */
  virtual uint32_t AnchorCount();

  /**
   * Returns an anchor accessible at the given index.
   */
  virtual Accessible* AnchorAt(uint32_t aAnchorIndex);

  /**
   * Returns an anchor URI at the given index.
   */
  virtual already_AddRefed<nsIURI> AnchorURIAt(uint32_t aAnchorIndex);

  /**
   * Returns a text point for the accessible element.
   */
  void ToTextPoint(HyperTextAccessible** aContainer, int32_t* aOffset,
                   bool aIsBefore = true) const;

  //////////////////////////////////////////////////////////////////////////////
  // SelectAccessible

  /**
   * Return an array of selected items.
   */
  virtual void SelectedItems(nsTArray<Accessible*>* aItems);

  /**
   * Return the number of selected items.
   */
  virtual uint32_t SelectedItemCount();

  /**
   * Return selected item at the given index.
   */
  virtual Accessible* GetSelectedItem(uint32_t aIndex);

  /**
   * Determine if item at the given index is selected.
   */
  virtual bool IsItemSelected(uint32_t aIndex);

  /**
   * Add item at the given index the selection. Return true if success.
   */
  virtual bool AddItemToSelection(uint32_t aIndex);

  /**
   * Remove item at the given index from the selection. Return if success.
   */
  virtual bool RemoveItemFromSelection(uint32_t aIndex);

  /**
   * Select all items. Return true if success.
   */
  virtual bool SelectAll();

  /**
   * Unselect all items. Return true if success.
   */
  virtual bool UnselectAll();

  //////////////////////////////////////////////////////////////////////////////
  // Value (numeric value interface)

  virtual double MaxValue() const;
  virtual double MinValue() const;
  virtual double CurValue() const;
  virtual double Step() const;
  virtual bool SetCurValue(double aValue);

  //////////////////////////////////////////////////////////////////////////////
  // Widgets

  /**
   * Return true if accessible is a widget, i.e. control or accessible that
   * manages its items. Note, being a widget the accessible may be a part of
   * composite widget.
   */
  virtual bool IsWidget() const;

  /**
   * Return true if the widget is active, i.e. has a focus within it.
   */
  virtual bool IsActiveWidget() const;

  /**
   * Return true if the widget has items and items are operable by user and
   * can be activated.
   */
  virtual bool AreItemsOperable() const;

  /**
   * Return the current item of the widget, i.e. an item that has or will have
   * keyboard focus when widget gets active.
   */
  virtual Accessible* CurrentItem();

  /**
   * Set the current item of the widget.
   */
  virtual void SetCurrentItem(Accessible* aItem);

  /**
   * Return container widget this accessible belongs to.
   */
  virtual Accessible* ContainerWidget() const;

  /**
   * Return the localized string for the given key.
   */
  static void TranslateString(const nsString& aKey, nsAString& aStringOut);

  /**
   * Return true if the accessible is defunct.
   */
  bool IsDefunct() const { return mStateFlags & eIsDefunct; }

  /**
   * Return false if the accessible is no longer in the document.
   */
  bool IsInDocument() const { return !(mStateFlags & eIsNotInDocument); }

  /**
   * Return true if the accessible should be contained by document node map.
   */
  bool IsNodeMapEntry() const
    { return HasOwnContent() && !(mStateFlags & eNotNodeMapEntry); }

  /**
   * Return true if the accessible's group info needs to be updated.
   */
  inline bool HasDirtyGroupInfo() const { return mStateFlags & eGroupInfoDirty; }

  /**
   * Return true if the accessible has associated DOM content.
   */
  bool HasOwnContent() const
    { return mContent && !(mStateFlags & eSharedNode); }

  /**
  * Return true if the accessible has a numeric value.
  */
  bool HasNumericValue() const;

  /**
   * Return true if the accessible state change is processed by handling proper
   * DOM UI event, if otherwise then false. For example, HTMLCheckboxAccessible
   * process nsIDocumentObserver::ContentStateChanged instead
   * 'CheckboxStateChange' event.
   */
  bool NeedsDOMUIEvent() const
    { return !(mStateFlags & eIgnoreDOMUIEvent); }

  /**
   * Get/set survivingInUpdate bit on child indicating that parent recollects
   * its children.
   */
  bool IsSurvivingInUpdate() const { return mStateFlags & eSurvivingInUpdate; }
  void SetSurvivingInUpdate(bool aIsSurviving)
  {
    if (aIsSurviving)
      mStateFlags |= eSurvivingInUpdate;
    else
      mStateFlags &= ~eSurvivingInUpdate;
  }

  /**
   * Get/set repositioned bit indicating that the accessible was moved in
   * the accessible tree, i.e. the accessible tree structure differs from DOM.
   */
  bool IsRelocated() const { return mStateFlags & eRelocated; }
  void SetRelocated(bool aRelocated)
  {
    if (aRelocated)
      mStateFlags |= eRelocated;
    else
      mStateFlags &= ~eRelocated;
  }

  /**
   * Return true if the accessible doesn't allow accessible children from XBL
   * anonymous subtree.
   */
  bool NoXBLKids() const { return mStateFlags & eNoXBLKids; }

  /**
   * Return true if the accessible allows accessible children from subtree of
   * a DOM element of this accessible.
   */
  bool KidsFromDOM() const { return !(mStateFlags & eNoKidsFromDOM); }

  /**
   * Return true if this accessible has a parent whose name depends on this
   * accessible.
   */
  bool HasNameDependentParent() const
    { return mContextFlags & eHasNameDependentParent; }

  /**
   * Return true if aria-hidden="true" is applied to the accessible or inherited
   * from the parent.
   */
  bool IsARIAHidden() const { return mContextFlags & eARIAHidden; }
  void SetARIAHidden(bool aIsDefined);

  /**
   * Return true if the element is inside an alert.
   */
  bool IsInsideAlert() const { return mContextFlags & eInsideAlert; }

  /**
   * Return true if there is a pending reorder event for this accessible.
   */
  bool ReorderEventTarget() const { return mReorderEventTarget; }

  /**
   * Return true if there is a pending show event for this accessible.
   */
  bool ShowEventTarget() const { return mShowEventTarget; }

  /**
   * Return true if there is a pending hide event for this accessible.
   */
  bool HideEventTarget() const { return mHideEventTarget; }

  /**
   * Set if there is a pending reorder event for this accessible.
   */
  void SetReorderEventTarget(bool aTarget) { mReorderEventTarget = aTarget; }

  /**
   * Set if this accessible is a show event target.
   */
  void SetShowEventTarget(bool aTarget) { mShowEventTarget = aTarget; }

  /**
   * Set if this accessible is a hide event target.
   */
  void SetHideEventTarget(bool aTarget) { mHideEventTarget = aTarget; }

protected:
  virtual ~Accessible();

  /**
   * Return the accessible name provided by native markup. It doesn't take
   * into account ARIA markup used to specify the name.
   */
  virtual mozilla::a11y::ENameValueFlag NativeName(nsString& aName);

  /**
   * Return the accessible description provided by native markup. It doesn't take
   * into account ARIA markup used to specify the description.
   */
  virtual void NativeDescription(nsString& aDescription);

  /**
   * Return object attributes provided by native markup. It doesn't take into
   * account ARIA.
   */
  virtual already_AddRefed<nsIPersistentProperties> NativeAttributes();

  //////////////////////////////////////////////////////////////////////////////
  // Initializing, cache and tree traverse methods

  /**
   * Destroy the object.
   */
  void LastRelease();

  /**
   * Set accessible parent and index in parent.
   */
  void BindToParent(Accessible* aParent, uint32_t aIndexInParent);
  void UnbindFromParent();

  /**
   * Return sibling accessible at the given offset.
   */
  virtual Accessible* GetSiblingAtOffset(int32_t aOffset,
                                         nsresult *aError = nullptr) const;

  /**
   * Flags used to describe the state of this accessible.
   */
  enum StateFlags {
    eIsDefunct = 1 << 0, // accessible is defunct
    eIsNotInDocument = 1 << 1, // accessible is not in document
    eSharedNode = 1 << 2, // accessible shares DOM node from another accessible
    eNotNodeMapEntry = 1 << 3, // accessible shouldn't be in document node map
    eHasNumericValue = 1 << 4, // accessible has a numeric value
    eGroupInfoDirty = 1 << 5, // accessible needs to update group info
    eKidsMutating = 1 << 6, // subtree is being mutated
    eIgnoreDOMUIEvent = 1 << 7, // don't process DOM UI events for a11y events
    eSurvivingInUpdate = 1 << 8, // parent drops children to recollect them
    eRelocated = 1 << 9, // accessible was moved in tree
    eNoXBLKids = 1 << 10, // accessible don't allows XBL children
    eNoKidsFromDOM = 1 << 11, // accessible doesn't allow children from DOM
    eHasTextKids = 1 << 12, // accessible have a text leaf in children

    eLastStateFlag = eNoKidsFromDOM
  };

  /**
   * Flags used for contextual information about the accessible.
   */
  enum ContextFlags {
    eHasNameDependentParent = 1 << 0, // Parent's name depends on this accessible.
    eARIAHidden = 1 << 1,
    eInsideAlert = 1 << 2,

    eLastContextFlag = eInsideAlert
  };

protected:

  //////////////////////////////////////////////////////////////////////////////
  // Miscellaneous helpers

  /**
   * Return ARIA role (helper method).
   */
  mozilla::a11y::role ARIATransformRole(mozilla::a11y::role aRole);

  //////////////////////////////////////////////////////////////////////////////
  // Name helpers

  /**
   * Returns the accessible name specified by ARIA.
   */
  void ARIAName(nsString& aName);

  /**
   * Return the name for XUL element.
   */
  static void XULElmName(DocAccessible* aDocument,
                         nsIContent* aElm, nsString& aName);

  // helper method to verify frames
  static nsresult GetFullKeyName(const nsAString& aModifierName, const nsAString& aKeyName, nsAString& aStringOut);

  //////////////////////////////////////////////////////////////////////////////
  // Action helpers

  /**
   * Prepares click action that will be invoked in timeout.
   *
   * @note  DoCommand() prepares an action in timeout because when action
   *  command opens a modal dialog/window, it won't return until the
   *  dialog/window is closed. If executing action command directly in
   *  nsIAccessible::DoAction() method, it will block AT tools (e.g. GOK) that
   *  invoke action of mozilla accessibles direclty (see bug 277888 for details).
   *
   * @param  aContent      [in, optional] element to click
   * @param  aActionIndex  [in, optional] index of accessible action
   */
  void DoCommand(nsIContent *aContent = nullptr, uint32_t aActionIndex = 0);

  /**
   * Dispatch click event.
   */
  virtual void DispatchClickEvent(nsIContent *aContent, uint32_t aActionIndex);

  //////////////////////////////////////////////////////////////////////////////
  // Helpers

  /**
   *  Get the container node for an atomic region, defined by aria-atomic="true"
   *  @return the container node
   */
  nsIContent* GetAtomicRegion() const;

  /**
   * Return numeric value of the given ARIA attribute, NaN if not applicable.
   *
   * @param aARIAProperty  [in] the ARIA property we're using
   * @return  a numeric value
   */
  double AttrNumericValue(nsIAtom* aARIAAttr) const;

  /**
   * Return the action rule based on ARIA enum constants EActionRule
   * (see ARIAMap.h). Used by ActionCount() and ActionNameAt().
   */
  uint32_t GetActionRule() const;

  /**
   * Return group info.
   */
  AccGroupInfo* GetGroupInfo();

  // Data Members
  nsCOMPtr<nsIContent> mContent;
  DocAccessible* mDoc;

  Accessible* mParent;
  nsTArray<Accessible*> mChildren;
  int32_t mIndexInParent;

  static const uint8_t kStateFlagsBits = 13;
  static const uint8_t kContextFlagsBits = 3;
  static const uint8_t kTypeBits = 6;
  static const uint8_t kGenericTypesBits = 16;

  /**
   * Non-NO_ROLE_MAP_ENTRY_INDEX indicates author-supplied role;
   * possibly state & value as well
   */
  uint8_t mRoleMapEntryIndex;

  /**
   * Keep in sync with StateFlags, ContextFlags, and AccTypes.
   */
  uint32_t mStateFlags : kStateFlagsBits;
  uint32_t mContextFlags : kContextFlagsBits;
  uint32_t mType : kTypeBits;
  uint32_t mGenericTypes : kGenericTypesBits;
  uint32_t mReorderEventTarget : 1;
  uint32_t mShowEventTarget : 1;
  uint32_t mHideEventTarget : 1;

  void StaticAsserts() const;

#ifdef A11Y_LOG
  friend void logging::Tree(const char* aTitle, const char* aMsgText,
                            Accessible* aRoot,
                            logging::GetTreePrefix aPrefixFunc,
                            void* aGetTreePrefixData);
#endif
  friend class DocAccessible;
  friend class xpcAccessible;
  friend class TreeMutation;

  UniquePtr<mozilla::a11y::EmbeddedObjCollector> mEmbeddedObjCollector;
  union {
    int32_t mIndexOfEmbeddedChild;
    uint32_t mProxyInterfaces;
  } mInt;

  friend class EmbeddedObjCollector;

  union
  {
    AccGroupInfo* groupInfo;
    ProxyAccessible* proxy;
  } mBits;
  friend class AccGroupInfo;

private:
  Accessible() = delete;
  Accessible(const Accessible&) = delete;
  Accessible& operator =(const Accessible&) = delete;

};

NS_DEFINE_STATIC_IID_ACCESSOR(Accessible,
                              NS_ACCESSIBLE_IMPL_IID)


/**
 * Represent key binding associated with accessible (such as access key and
 * global keyboard shortcuts).
 */
class KeyBinding
{
public:
  /**
   * Modifier mask values.
   */
  static const uint32_t kShift = 1;
  static const uint32_t kControl = 2;
  static const uint32_t kAlt = 4;
  static const uint32_t kMeta = 8;
  static const uint32_t kOS = 16;

  static uint32_t AccelModifier();

  KeyBinding() : mKey(0), mModifierMask(0) {}
  KeyBinding(uint32_t aKey, uint32_t aModifierMask) :
    mKey(aKey), mModifierMask(aModifierMask) {}

  inline bool IsEmpty() const { return !mKey; }
  inline uint32_t Key() const { return mKey; }
  inline uint32_t ModifierMask() const { return mModifierMask; }

  enum Format {
    ePlatformFormat,
    eAtkFormat
  };

  /**
   * Return formatted string for this key binding depending on the given format.
   */
  inline void ToString(nsAString& aValue,
                       Format aFormat = ePlatformFormat) const
  {
    aValue.Truncate();
    AppendToString(aValue, aFormat);
  }
  inline void AppendToString(nsAString& aValue,
                             Format aFormat = ePlatformFormat) const
  {
    if (mKey) {
      if (aFormat == ePlatformFormat)
        ToPlatformFormat(aValue);
      else
        ToAtkFormat(aValue);
    }
  }

private:
  void ToPlatformFormat(nsAString& aValue) const;
  void ToAtkFormat(nsAString& aValue) const;

  uint32_t mKey;
  uint32_t mModifierMask;
};

} // namespace a11y
} // namespace mozilla

#endif
