/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_AccIterator_h__
#define mozilla_a11y_AccIterator_h__

#include "DocAccessible.h"
#include "Filters.h"

#include <memory>

class nsITreeView;

namespace mozilla {
namespace a11y {

/**
 * AccIterable is a basic interface for iterators over accessibles.
 */
class AccIterable {
 public:
  virtual ~AccIterable() {}
  virtual LocalAccessible* Next() = 0;

 private:
  friend class Relation;
  std::unique_ptr<AccIterable> mNextIter;
};

/**
 * Allows to iterate through accessible children or subtree complying with
 * filter function.
 */
class AccIterator : public AccIterable {
 public:
  AccIterator(const LocalAccessible* aRoot, filters::FilterFuncPtr aFilterFunc);
  virtual ~AccIterator();

  /**
   * Return next accessible complying with filter function. Return the first
   * accessible for the first time.
   */
  virtual LocalAccessible* Next() override;

 private:
  AccIterator();
  AccIterator(const AccIterator&);
  AccIterator& operator=(const AccIterator&);

  struct IteratorState {
    explicit IteratorState(const LocalAccessible* aParent,
                           IteratorState* mParentState = nullptr);

    const LocalAccessible* mParent;
    int32_t mIndex;
    IteratorState* mParentState;
  };

  filters::FilterFuncPtr mFilterFunc;
  IteratorState* mState;
};

/**
 * Allows to traverse through related accessibles that are pointing to the given
 * dependent accessible by relation attribute.
 */
class RelatedAccIterator : public AccIterable {
 public:
  /**
   * Constructor.
   *
   * @param aDocument         [in] the document accessible the related
   * &                         accessibles belong to.
   * @param aDependentContent [in] the content of dependent accessible that
   *                           relations were requested for
   * @param aRelAttr          [in] relation attribute that relations are
   *                           pointed by
   */
  RelatedAccIterator(DocAccessible* aDocument, nsIContent* aDependentContent,
                     nsAtom* aRelAttr);

  virtual ~RelatedAccIterator() {}

  /**
   * Return next related accessible for the given dependent accessible.
   */
  virtual LocalAccessible* Next() override;

 private:
  RelatedAccIterator();
  RelatedAccIterator(const RelatedAccIterator&);
  RelatedAccIterator& operator=(const RelatedAccIterator&);

  DocAccessible* mDocument;
  nsAtom* mRelAttr;
  DocAccessible::AttrRelProviders* mProviders;
  uint32_t mIndex;
};

/**
 * Used to iterate through HTML labels associated with the given accessible.
 */
class HTMLLabelIterator : public AccIterable {
 public:
  enum LabelFilter { eAllLabels, eSkipAncestorLabel };

  HTMLLabelIterator(DocAccessible* aDocument,
                    const LocalAccessible* aAccessible,
                    LabelFilter aFilter = eAllLabels);

  virtual ~HTMLLabelIterator() {}

  /**
   * Return next label accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  HTMLLabelIterator();
  HTMLLabelIterator(const HTMLLabelIterator&);
  HTMLLabelIterator& operator=(const HTMLLabelIterator&);

  bool IsLabel(LocalAccessible* aLabel);

  RelatedAccIterator mRelIter;
  // XXX: replace it on weak reference (bug 678429), it's safe to use raw
  // pointer now because iterators life cycle is short.
  const LocalAccessible* mAcc;
  LabelFilter mLabelFilter;
};

/**
 * Used to iterate through HTML outputs associated with the given element.
 */
class HTMLOutputIterator : public AccIterable {
 public:
  HTMLOutputIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~HTMLOutputIterator() {}

  /**
   * Return next output accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  HTMLOutputIterator();
  HTMLOutputIterator(const HTMLOutputIterator&);
  HTMLOutputIterator& operator=(const HTMLOutputIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through XUL labels associated with the given element.
 */
class XULLabelIterator : public AccIterable {
 public:
  XULLabelIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULLabelIterator() {}

  /**
   * Return next label accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  XULLabelIterator();
  XULLabelIterator(const XULLabelIterator&);
  XULLabelIterator& operator=(const XULLabelIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through XUL descriptions associated with the given element.
 */
class XULDescriptionIterator : public AccIterable {
 public:
  XULDescriptionIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULDescriptionIterator() {}

  /**
   * Return next description accessible associated with the given element.
   */
  virtual LocalAccessible* Next() override;

 private:
  XULDescriptionIterator();
  XULDescriptionIterator(const XULDescriptionIterator&);
  XULDescriptionIterator& operator=(const XULDescriptionIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through IDs, elements or accessibles pointed by IDRefs
 * attribute. Note, any method used to iterate through IDs, elements, or
 * accessibles moves iterator to next position.
 */
class IDRefsIterator : public AccIterable {
 public:
  IDRefsIterator(DocAccessible* aDoc, nsIContent* aContent,
                 nsAtom* aIDRefsAttr);
  virtual ~IDRefsIterator() {}

  /**
   * Return next ID.
   */
  const nsDependentSubstring NextID();

  /**
   * Return next element.
   */
  nsIContent* NextElem();

  /**
   * Return the element with the given ID.
   */
  static dom::Element* GetElem(nsIContent* aContent, const nsAString& aID);
  dom::Element* GetElem(const nsDependentSubstring& aID);

  // AccIterable
  virtual LocalAccessible* Next() override;

 private:
  IDRefsIterator();
  IDRefsIterator(const IDRefsIterator&);
  IDRefsIterator operator=(const IDRefsIterator&);

  nsString mIDs;
  nsIContent* mContent;
  DocAccessible* mDoc;
  nsAString::index_type mCurrIdx;
};

/**
 * Iterator that points to a single accessible returning it on the first call
 * to Next().
 */
class SingleAccIterator : public AccIterable {
 public:
  explicit SingleAccIterator(LocalAccessible* aTarget) : mAcc(aTarget) {}
  virtual ~SingleAccIterator() {}

  virtual LocalAccessible* Next() override;

 private:
  SingleAccIterator();
  SingleAccIterator(const SingleAccIterator&);
  SingleAccIterator& operator=(const SingleAccIterator&);

  RefPtr<LocalAccessible> mAcc;
};

/**
 * Used to iterate items of the given item container.
 */
class ItemIterator : public AccIterable {
 public:
  explicit ItemIterator(const LocalAccessible* aItemContainer)
      : mContainer(aItemContainer), mAnchor(nullptr) {}
  virtual ~ItemIterator() {}

  virtual LocalAccessible* Next() override;

 private:
  ItemIterator() = delete;
  ItemIterator(const ItemIterator&) = delete;
  ItemIterator& operator=(const ItemIterator&) = delete;

  const LocalAccessible* mContainer;
  LocalAccessible* mAnchor;
};

/**
 * Used to iterate through XUL tree items of the same level.
 */
class XULTreeItemIterator : public AccIterable {
 public:
  XULTreeItemIterator(const XULTreeAccessible* aXULTree, nsITreeView* aTreeView,
                      int32_t aRowIdx);
  virtual ~XULTreeItemIterator() {}

  virtual LocalAccessible* Next() override;

 private:
  XULTreeItemIterator() = delete;
  XULTreeItemIterator(const XULTreeItemIterator&) = delete;
  XULTreeItemIterator& operator=(const XULTreeItemIterator&) = delete;

  const XULTreeAccessible* mXULTree;
  nsITreeView* mTreeView;
  int32_t mRowCount;
  int32_t mContainerLevel;
  int32_t mCurrRowIdx;
};

}  // namespace a11y
}  // namespace mozilla

#endif
