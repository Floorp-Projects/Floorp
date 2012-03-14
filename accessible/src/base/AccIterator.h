/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef nsAccIterator_h_
#define nsAccIterator_h_

#include "nsAccessibilityService.h"
#include "filters.h"
#include "nscore.h"
#include "nsDocAccessible.h"

#include "nsIDOMDocumentXBL.h"

/**
 * AccIterable is a basic interface for iterators over accessibles.
 */
class AccIterable
{
public:
  virtual ~AccIterable() { }
  virtual nsAccessible* Next() = 0;

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

  AccIterator(nsAccessible* aRoot, filters::FilterFuncPtr aFilterFunc,
              IterationType aIterationType = eFlatNav);
  virtual ~AccIterator();

  /**
   * Return next accessible complying with filter function. Return the first
   * accessible for the first time.
   */
  virtual nsAccessible *Next();

private:
  AccIterator();
  AccIterator(const AccIterator&);
  AccIterator& operator =(const AccIterator&);

  struct IteratorState
  {
    IteratorState(nsAccessible *aParent, IteratorState *mParentState = nsnull);

    nsAccessible *mParent;
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
  RelatedAccIterator(nsDocAccessible* aDocument, nsIContent* aDependentContent,
                     nsIAtom* aRelAttr);

  virtual ~RelatedAccIterator() { }

  /**
   * Return next related accessible for the given dependent accessible.
   */
  virtual nsAccessible* Next();

private:
  RelatedAccIterator();
  RelatedAccIterator(const RelatedAccIterator&);
  RelatedAccIterator& operator = (const RelatedAccIterator&);

  nsDocAccessible* mDocument;
  nsIAtom* mRelAttr;
  nsDocAccessible::AttrRelProviderArray* mProviders;
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

  HTMLLabelIterator(nsDocAccessible* aDocument, const nsAccessible* aAccessible,
                    LabelFilter aFilter = eAllLabels);

  virtual ~HTMLLabelIterator() { }

  /**
   * Return next label accessible associated with the given element.
   */
  virtual nsAccessible* Next();

private:
  HTMLLabelIterator();
  HTMLLabelIterator(const HTMLLabelIterator&);
  HTMLLabelIterator& operator = (const HTMLLabelIterator&);

  RelatedAccIterator mRelIter;
  // XXX: replace it on weak reference (bug 678429), it's safe to use raw
  // pointer now because iterators life cycle is short.
  const nsAccessible* mAcc;
  LabelFilter mLabelFilter;
};


/**
 * Used to iterate through HTML outputs associated with the given element.
 */
class HTMLOutputIterator : public AccIterable
{
public:
  HTMLOutputIterator(nsDocAccessible* aDocument, nsIContent* aElement);
  virtual ~HTMLOutputIterator() { }

  /**
   * Return next output accessible associated with the given element.
   */
  virtual nsAccessible* Next();

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
  XULLabelIterator(nsDocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULLabelIterator() { }

  /**
   * Return next label accessible associated with the given element.
   */
  virtual nsAccessible* Next();

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
  XULDescriptionIterator(nsDocAccessible* aDocument, nsIContent* aElement);
  virtual ~XULDescriptionIterator() { }

  /**
   * Return next description accessible associated with the given element.
   */
  virtual nsAccessible* Next();

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
  IDRefsIterator(nsIContent* aContent, nsIAtom* aIDRefsAttr);
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
  virtual nsAccessible* Next();

private:
  IDRefsIterator();
  IDRefsIterator(const IDRefsIterator&);
  IDRefsIterator operator = (const IDRefsIterator&);

  nsString mIDs;
  nsIContent* mContent;
  nsAString::index_type mCurrIdx;
};

/**
 * Iterator that points to a single accessible returning it on the first call
 * to Next().
 */
class SingleAccIterator : public AccIterable
{
public:
  SingleAccIterator(nsAccessible* aTarget): mAcc(aTarget) { }
  virtual ~SingleAccIterator() { }

  virtual nsAccessible* Next();

private:
  SingleAccIterator();
  SingleAccIterator(const SingleAccIterator&);
  SingleAccIterator& operator = (const SingleAccIterator&);

  nsRefPtr<nsAccessible> mAcc;
};

#endif
