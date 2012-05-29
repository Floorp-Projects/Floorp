/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAccIterator_h_
#define nsAccIterator_h_

#include "nsAccessibilityService.h"
#include "filters.h"
#include "nscore.h"
#include "DocAccessible.h"

/**
 * AccIterable is a basic interface for iterators over accessibles.
 */
class AccIterable
{
public:
  virtual ~AccIterable() { }
  virtual Accessible* Next() = 0;

private:
  friend class Relation;
  nsAutoPtr<AccIterable> mNextIter;
};

/**
 * Allows to iterate through accessible children or subtree complying with
 * filter function.
 */
class AccIterator : public AccIterable
{
public:
  /**
   * Used to define iteration type.
   */
  enum IterationType {
    /**
     * Navigation happens through direct children.
     */
    eFlatNav,

    /**
     * Navigation through subtree excluding iterator root; if the accessible
     * complies with filter, iterator ignores its children.
     */
    eTreeNav
  };

  AccIterator(Accessible* aRoot, filters::FilterFuncPtr aFilterFunc,
              IterationType aIterationType = eFlatNav);
  virtual ~AccIterator();

  /**
   * Return next accessible complying with filter function. Return the first
   * accessible for the first time.
   */
  virtual Accessible* Next();

private:
  AccIterator();
  AccIterator(const AccIterator&);
  AccIterator& operator =(const AccIterator&);

  struct IteratorState
  {
    IteratorState(Accessible* aParent, IteratorState* mParentState = nsnull);

    Accessible* mParent;
    PRInt32 mIndex;
    IteratorState *mParentState;
  };

  filters::FilterFuncPtr mFilterFunc;
  bool mIsDeep;
  IteratorState *mState;
};


/**
 * Allows to traverse through related accessibles that are pointing to the given
 * dependent accessible by relation attribute.
 */
class RelatedAccIterator : public AccIterable
{
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
                     nsIAtom* aRelAttr);

  virtual ~RelatedAccIterator() { }

  /**
   * Return next related accessible for the given dependent accessible.
   */
  virtual Accessible* Next();

private:
  RelatedAccIterator();
  RelatedAccIterator(const RelatedAccIterator&);
  RelatedAccIterator& operator = (const RelatedAccIterator&);

  DocAccessible* mDocument;
  nsIAtom* mRelAttr;
  DocAccessible::AttrRelProviderArray* mProviders;
  nsIContent* mBindingParent;
  PRUint32 mIndex;
};


/**
 * Used to iterate through HTML labels associated with the given accessible.
 */
class HTMLLabelIterator : public AccIterable
{
public:
  enum LabelFilter {
    eAllLabels,
    eSkipAncestorLabel
  };

  HTMLLabelIterator(DocAccessible* aDocument, const Accessible* aAccessible,
                    LabelFilter aFilter = eAllLabels);

  virtual ~HTMLLabelIterator() { }

  /**
   * Return next label accessible associated with the given element.
   */
  virtual Accessible* Next();

private:
  HTMLLabelIterator();
  HTMLLabelIterator(const HTMLLabelIterator&);
  HTMLLabelIterator& operator = (const HTMLLabelIterator&);

  RelatedAccIterator mRelIter;
  // XXX: replace it on weak reference (bug 678429), it's safe to use raw
  // pointer now because iterators life cycle is short.
  const Accessible* mAcc;
  LabelFilter mLabelFilter;
};


/**
 * Used to iterate through HTML outputs associated with the given element.
 */
class HTMLOutputIterator : public AccIterable
{
public:
  HTMLOutputIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~HTMLOutputIterator() { }

  /**
   * Return next output accessible associated with the given element.
   */
  virtual Accessible* Next();

private:
  HTMLOutputIterator();
  HTMLOutputIterator(const HTMLOutputIterator&);
  HTMLOutputIterator& operator = (const HTMLOutputIterator&);

  RelatedAccIterator mRelIter;
};


/**
 * Used to iterate through XUL labels associated with the given element.
 */
class XULLabelIterator : public AccIterable
{
public:
  XULLabelIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULLabelIterator() { }

  /**
   * Return next label accessible associated with the given element.
   */
  virtual Accessible* Next();

private:
  XULLabelIterator();
  XULLabelIterator(const XULLabelIterator&);
  XULLabelIterator& operator = (const XULLabelIterator&);

  RelatedAccIterator mRelIter;
};


/**
 * Used to iterate through XUL descriptions associated with the given element.
 */
class XULDescriptionIterator : public AccIterable
{
public:
  XULDescriptionIterator(DocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULDescriptionIterator() { }

  /**
   * Return next description accessible associated with the given element.
   */
  virtual Accessible* Next();

private:
  XULDescriptionIterator();
  XULDescriptionIterator(const XULDescriptionIterator&);
  XULDescriptionIterator& operator = (const XULDescriptionIterator&);

  RelatedAccIterator mRelIter;
};

/**
 * Used to iterate through IDs, elements or accessibles pointed by IDRefs
 * attribute. Note, any method used to iterate through IDs, elements, or
 * accessibles moves iterator to next position.
 */
class IDRefsIterator : public AccIterable
{
public:
  IDRefsIterator(DocAccessible* aDoc, nsIContent* aContent,
                 nsIAtom* aIDRefsAttr);
  virtual ~IDRefsIterator() { }

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
  nsIContent* GetElem(const nsDependentSubstring& aID);

  // AccIterable
  virtual Accessible* Next();

private:
  IDRefsIterator();
  IDRefsIterator(const IDRefsIterator&);
  IDRefsIterator operator = (const IDRefsIterator&);

  nsString mIDs;
  nsIContent* mContent;
  DocAccessible* mDoc;
  nsAString::index_type mCurrIdx;
};

/**
 * Iterator that points to a single accessible returning it on the first call
 * to Next().
 */
class SingleAccIterator : public AccIterable
{
public:
  SingleAccIterator(Accessible* aTarget): mAcc(aTarget) { }
  virtual ~SingleAccIterator() { }

  virtual Accessible* Next();

private:
  SingleAccIterator();
  SingleAccIterator(const SingleAccIterator&);
  SingleAccIterator& operator = (const SingleAccIterator&);

  nsRefPtr<Accessible> mAcc;
};

#endif
