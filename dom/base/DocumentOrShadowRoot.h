/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentOrShadowRoot_h__
#define mozilla_dom_DocumentOrShadowRoot_h__

#include "mozilla/dom/NameSpaceConstants.h"
#include "mozilla/IdentifierMapEntry.h"
#include "mozilla/RelativeTo.h"
#include "mozilla/ReverseIterator.h"
#include "nsClassHashtable.h"
#include "nsContentListDeclarations.h"
#include "nsTArray.h"

class nsContentList;
class nsCycleCollectionTraversalCallback;
class nsINode;
class nsINodeList;
class nsIRadioVisitor;
class nsWindowSizes;

namespace mozilla {
class ErrorResult;
class StyleSheet;
class ErrorResult;

namespace dom {

class Animation;
class Element;
class Document;
class DocumentOrShadowRoot;
class HTMLInputElement;
struct nsRadioGroupStruct;
class StyleSheetList;
class ShadowRoot;
template <typename T>
class Sequence;

/**
 * A class meant to be shared by ShadowRoot and Document, that holds a list of
 * stylesheets.
 *
 * TODO(emilio, bug 1418159): In the future this should hold most of the
 * relevant style state, this should allow us to fix bug 548397.
 */
class DocumentOrShadowRoot {
  enum class Kind {
    Document,
    ShadowRoot,
  };

 public:
  // These should always be non-null, but can't use a reference because
  // dereferencing `this` on initializer lists is UB, apparently, see
  // bug 1596499.
  explicit DocumentOrShadowRoot(Document*);
  explicit DocumentOrShadowRoot(ShadowRoot*);

  // Unusual argument naming is because of cycle collection macros.
  static void Traverse(DocumentOrShadowRoot* tmp,
                       nsCycleCollectionTraversalCallback& cb);
  static void Unlink(DocumentOrShadowRoot* tmp);

  nsINode& AsNode() { return *mAsNode; }

  const nsINode& AsNode() const { return *mAsNode; }

  StyleSheet* SheetAt(size_t aIndex) const {
    return mStyleSheets.SafeElementAt(aIndex);
  }

  size_t SheetCount() const { return mStyleSheets.Length(); }

  size_t AdoptedSheetCount() const { return mAdoptedStyleSheets.Length(); }

  /**
   * Returns an index for the sheet in relative style order.
   * If there are non-applicable sheets, then this index may
   * not match 1:1 with the sheet's actual index in the style set.
   *
   * Handles sheets from both mStyleSheets and mAdoptedStyleSheets
   */
  int32_t StyleOrderIndexOfSheet(const StyleSheet& aSheet) const;

  StyleSheetList* StyleSheets();

  void GetAdoptedStyleSheets(nsTArray<RefPtr<StyleSheet>>&) const;

  void RemoveStyleSheet(StyleSheet&);

  Element* GetElementById(const nsAString& aElementId);

  /**
   * This method returns _all_ the elements in this scope which have id
   * aElementId, if there are any.  Otherwise it returns null.
   *
   * This is useful for stuff like QuerySelector optimization and such.
   */
  inline const nsTArray<Element*>* GetAllElementsForId(
      const nsAString& aElementId) const;

  already_AddRefed<nsContentList> GetElementsByTagName(
      const nsAString& aTagName) {
    return NS_GetContentList(&AsNode(), kNameSpaceID_Unknown, aTagName);
  }

  already_AddRefed<nsContentList> GetElementsByTagNameNS(
      const nsAString& aNamespaceURI, const nsAString& aLocalName);

  already_AddRefed<nsContentList> GetElementsByTagNameNS(
      const nsAString& aNamespaceURI, const nsAString& aLocalName,
      mozilla::ErrorResult&);

  already_AddRefed<nsContentList> GetElementsByClassName(
      const nsAString& aClasses);

  ~DocumentOrShadowRoot();

  Element* GetPointerLockElement();
  Element* GetFullscreenElement() const;

  Element* ElementFromPoint(float aX, float aY);
  nsINode* NodeFromPoint(float aX, float aY);

  void ElementsFromPoint(float aX, float aY, nsTArray<RefPtr<Element>>&);
  void NodesFromPoint(float aX, float aY, nsTArray<RefPtr<nsINode>>&);

  /**
   * Helper for elementFromPoint implementation that allows
   * ignoring the scroll frame and/or avoiding layout flushes.
   *
   * @see nsIDOMWindowUtils::elementFromPoint
   */
  Element* ElementFromPointHelper(float aX, float aY,
                                  bool aIgnoreRootScrollFrame,
                                  bool aFlushLayout,
                                  ViewportType aViewportType);

  void NodesFromRect(float aX, float aY, float aTopSize, float aRightSize,
                     float aBottomSize, float aLeftSize,
                     bool aIgnoreRootScrollFrame, bool aFlushLayout,
                     bool aOnlyVisible, float aVisibleThreshold,
                     nsTArray<RefPtr<nsINode>>&);

  /**
   * This gets fired when the element that an id refers to changes.
   * This fires at difficult times. It is generally not safe to do anything
   * which could modify the DOM in any way. Use
   * nsContentUtils::AddScriptRunner.
   * @return true to keep the callback in the callback set, false
   * to remove it.
   */
  typedef bool (*IDTargetObserver)(Element* aOldElement, Element* aNewelement,
                                   void* aData);

  /**
   * Add an IDTargetObserver for a specific ID. The IDTargetObserver
   * will be fired whenever the content associated with the ID changes
   * in the future. If aForImage is true, mozSetImageElement can override
   * what content is associated with the ID. In that case the IDTargetObserver
   * will be notified at those times when the result of LookupImageElement
   * changes.
   * At most one (aObserver, aData, aForImage) triple can be
   * registered for each ID.
   * @return the content currently associated with the ID.
   */
  Element* AddIDTargetObserver(nsAtom* aID, IDTargetObserver aObserver,
                               void* aData, bool aForImage);

  /**
   * Remove the (aObserver, aData, aForImage) triple for a specific ID, if
   * registered.
   */
  void RemoveIDTargetObserver(nsAtom* aID, IDTargetObserver aObserver,
                              void* aData, bool aForImage);

  /**
   * Lookup an image element using its associated ID, which is usually provided
   * by |-moz-element()|. Similar to GetElementById, with the difference that
   * elements set using mozSetImageElement have higher priority.
   * @param aId the ID associated the element we want to lookup
   * @return the element associated with |aId|
   */
  Element* LookupImageElement(const nsAString& aElementId);

  /**
   * Check that aId is not empty and log a message to the console
   * service if it is.
   * @returns true if aId looks correct, false otherwise.
   */
  inline bool CheckGetElementByIdArg(const nsAString& aId) {
    if (aId.IsEmpty()) {
      ReportEmptyGetElementByIdArg();
      return false;
    }
    return true;
  }

  void ReportEmptyGetElementByIdArg();

  // Web Animations
  MOZ_CAN_RUN_SCRIPT
  void GetAnimations(nsTArray<RefPtr<Animation>>& aAnimations);

  // nsIRadioGroupContainer
  NS_IMETHOD WalkRadioGroup(const nsAString& aName, nsIRadioVisitor* aVisitor,
                            bool aFlushContent);
  void SetCurrentRadioButton(const nsAString& aName, HTMLInputElement* aRadio);
  HTMLInputElement* GetCurrentRadioButton(const nsAString& aName);
  nsresult GetNextRadioButton(const nsAString& aName, const bool aPrevious,
                              HTMLInputElement* aFocusedRadio,
                              HTMLInputElement** aRadioOut);
  void AddToRadioGroup(const nsAString& aName, HTMLInputElement* aRadio);
  void RemoveFromRadioGroup(const nsAString& aName, HTMLInputElement* aRadio);
  uint32_t GetRequiredRadioCount(const nsAString& aName) const;
  void RadioRequiredWillChange(const nsAString& aName, bool aRequiredAdded);
  bool GetValueMissingState(const nsAString& aName) const;
  void SetValueMissingState(const nsAString& aName, bool aValue);

  // for radio group
  nsRadioGroupStruct* GetRadioGroup(const nsAString& aName) const;
  nsRadioGroupStruct* GetOrCreateRadioGroup(const nsAString& aName);

  nsIContent* Retarget(nsIContent* aContent) const;

  void SetAdoptedStyleSheets(
      const Sequence<OwningNonNull<StyleSheet>>& aAdoptedStyleSheets,
      ErrorResult& aRv);

  // This is needed because ServoStyleSet / ServoAuthorData don't deal with
  // duplicate stylesheets (and it's unclear we'd want to support that as it'd
  // be a bunch of duplicate work), while adopted stylesheets do need to deal
  // with them.
  template <typename Callback>
  void EnumerateUniqueAdoptedStyleSheetsBackToFront(Callback aCallback) {
    StyleSheetSet set(mAdoptedStyleSheets.Length());
    for (StyleSheet* sheet : Reversed(mAdoptedStyleSheets)) {
      if (MOZ_UNLIKELY(!set.EnsureInserted(sheet))) {
        continue;
      }
      aCallback(*sheet);
    }
  }

 protected:
  // Cycle collection helper functions
  void TraverseSheetRefInStylesIfApplicable(
      StyleSheet&, nsCycleCollectionTraversalCallback&);
  void TraverseStyleSheets(nsTArray<RefPtr<StyleSheet>>&, const char*,
                           nsCycleCollectionTraversalCallback&);
  void UnlinkStyleSheets(nsTArray<RefPtr<StyleSheet>>&);

  using StyleSheetSet = nsTHashtable<nsPtrHashKey<const StyleSheet>>;
  void RemoveSheetFromStylesIfApplicable(StyleSheet&);
  void ClearAdoptedStyleSheets();

  /**
   * Clone's the argument's adopted style sheets into this.
   * This should only be used when cloning a static document for printing.
   */
  void CloneAdoptedSheetsFrom(const DocumentOrShadowRoot&);

  void InsertSheetAt(size_t aIndex, StyleSheet& aSheet);

  void AddSizeOfExcludingThis(nsWindowSizes&) const;
  void AddSizeOfOwnedSheetArrayExcludingThis(
      nsWindowSizes&, const nsTArray<RefPtr<StyleSheet>>&) const;

  /**
   * If focused element's subtree root is this document or shadow root, return
   * focused element, otherwise, get the shadow host recursively until the
   * shadow host's subtree root is this document or shadow root.
   */
  Element* GetRetargetedFocusedElement();

  nsTArray<RefPtr<StyleSheet>> mStyleSheets;
  RefPtr<StyleSheetList> mDOMStyleSheets;

  /**
   * Style sheets that are adopted by assinging to the `adoptedStyleSheets`
   * WebIDL atribute. These can only be constructed stylesheets.
   */
  nsTArray<RefPtr<StyleSheet>> mAdoptedStyleSheets;

  /*
   * mIdentifierMap works as follows for IDs:
   * 1) Attribute changes affect the table immediately (removing and adding
   *    entries as needed).
   * 2) Removals from the DOM affect the table immediately
   * 3) Additions to the DOM always update existing entries for names, and add
   *    new ones for IDs.
   */
  nsTHashtable<IdentifierMapEntry> mIdentifierMap;

  nsClassHashtable<nsStringHashKey, nsRadioGroupStruct> mRadioGroups;

  // Always non-null, see comment in the constructor as to why a pointer instead
  // of a reference.
  nsINode* mAsNode;
  const Kind mKind;
};

inline const nsTArray<Element*>* DocumentOrShadowRoot::GetAllElementsForId(
    const nsAString& aElementId) const {
  if (aElementId.IsEmpty()) {
    return nullptr;
  }

  IdentifierMapEntry* entry = mIdentifierMap.GetEntry(aElementId);
  return entry ? &entry->GetIdElements() : nullptr;
}

}  // namespace dom

}  // namespace mozilla

#endif
