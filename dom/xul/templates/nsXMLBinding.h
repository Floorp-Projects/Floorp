/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLBinding_h__
#define nsXMLBinding_h__

#include "nsAutoPtr.h"
#include "nsIAtom.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/XPathExpression.h"

class nsINode;
class nsXULTemplateResultXML;
class nsXMLBindingValues;
namespace mozilla {
namespace dom {
class XPathResult;
} // namespace dom
} // namespace mozilla

/**
 * Classes related to storing bindings for XML handling.
 */

/**
 * a <binding> description
 */
struct nsXMLBinding {
  nsCOMPtr<nsIAtom> mVar;
  nsAutoPtr<mozilla::dom::XPathExpression> mExpr;

  nsAutoPtr<nsXMLBinding> mNext;

  nsXMLBinding(nsIAtom* aVar, nsAutoPtr<mozilla::dom::XPathExpression>&& aExpr)
    : mVar(aVar), mExpr(aExpr), mNext(nullptr)
  {
    MOZ_COUNT_CTOR(nsXMLBinding);
  }

  ~nsXMLBinding()
  {
    MOZ_COUNT_DTOR(nsXMLBinding);
  }
};

/**
 * a collection of <binding> descriptors. This object is refcounted by
 * nsXMLBindingValues objects and the query processor.
 */
class nsXMLBindingSet final
{
  ~nsXMLBindingSet();

public:
  // pointer to the first binding in a linked list
  nsAutoPtr<nsXMLBinding> mFirst;

  NS_INLINE_DECL_REFCOUNTING(nsXMLBindingSet);

  /**
   * Add a binding to the set
   */
  void
  AddBinding(nsIAtom* aVar, nsAutoPtr<mozilla::dom::XPathExpression>&& aExpr);

  /**
   * The nsXMLBindingValues class stores an array of values, one for each
   * target symbol that could be set by the bindings in the set.
   * LookupTargetIndex determines the index into the array for a given
   * target symbol.
   */
  int32_t
  LookupTargetIndex(nsIAtom* aTargetVariable, nsXMLBinding** aBinding);
};

/**
 * a set of values of bindings. This object is used once per result.
 */
class nsXMLBindingValues
{
protected:

  // the binding set
  RefPtr<nsXMLBindingSet> mBindings;

  /**
   * A set of values for variable bindings. To look up a binding value,
   * scan through the binding set in mBindings for the right target atom.
   * Its index will correspond to the index in this array.
   */
  nsTArray<RefPtr<mozilla::dom::XPathResult> > mValues;

public:

  nsXMLBindingValues() { MOZ_COUNT_CTOR(nsXMLBindingValues); }
  ~nsXMLBindingValues() { MOZ_COUNT_DTOR(nsXMLBindingValues); }

  nsXMLBindingSet* GetBindingSet() { return mBindings; }

  void SetBindingSet(nsXMLBindingSet* aBindings) { mBindings = aBindings; }

  int32_t
  LookupTargetIndex(nsIAtom* aTargetVariable, nsXMLBinding** aBinding)
  {
    return mBindings ?
           mBindings->LookupTargetIndex(aTargetVariable, aBinding) : -1;
  }

  /**
   * Retrieve the assignment for a particular variable
   *
   * aResult the result generated from the template
   * aBinding the binding looked up using LookupTargetIndex
   * aIndex the index of the assignment to retrieve
   * aType the type of result expected
   */
  mozilla::dom::XPathResult*
  GetAssignmentFor(nsXULTemplateResultXML* aResult,
                   nsXMLBinding* aBinding,
                   int32_t idx,
                   uint16_t type);

  nsINode*
  GetNodeAssignmentFor(nsXULTemplateResultXML* aResult,
                       nsXMLBinding* aBinding,
                       int32_t idx);

  void
  GetStringAssignmentFor(nsXULTemplateResultXML* aResult,
                         nsXMLBinding* aBinding,
                         int32_t idx,
                         nsAString& aValue);
};

#endif // nsXMLBinding_h__
