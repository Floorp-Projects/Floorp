/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLBinding_h__
#define nsXMLBinding_h__

#include "nsAutoPtr.h"
#include "nsIAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

class nsXULTemplateResultXML;
class nsXMLBindingValues;

/**
 * Classes related to storing bindings for XML handling.
 */

/**
 * a <binding> description
 */
struct nsXMLBinding {
  nsCOMPtr<nsIAtom> mVar;
  nsCOMPtr<nsIDOMXPathExpression> mExpr;

  nsAutoPtr<nsXMLBinding> mNext;

  nsXMLBinding(nsIAtom* aVar, nsIDOMXPathExpression* aExpr)
    : mVar(aVar), mExpr(aExpr), mNext(nsnull)
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
class nsXMLBindingSet MOZ_FINAL
{
public:

  // results hold a reference to a binding set in their
  // nsXMLBindingValues fields
  nsAutoRefCnt mRefCnt;

  // pointer to the first binding in a linked list
  nsAutoPtr<nsXMLBinding> mFirst;

public:

  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  NS_DECL_OWNINGTHREAD
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsXMLBindingSet)

  /**
   * Add a binding to the set
   */
  nsresult
  AddBinding(nsIAtom* aVar, nsIDOMXPathExpression* aExpr);

  /**
   * The nsXMLBindingValues class stores an array of values, one for each
   * target symbol that could be set by the bindings in the set.
   * LookupTargetIndex determines the index into the array for a given
   * target symbol.
   */
  PRInt32
  LookupTargetIndex(nsIAtom* aTargetVariable, nsXMLBinding** aBinding);
};

/**
 * a set of values of bindings. This object is used once per result.
 */
class nsXMLBindingValues
{
protected:

  // the binding set
  nsRefPtr<nsXMLBindingSet> mBindings;

  /**
   * A set of values for variable bindings. To look up a binding value,
   * scan through the binding set in mBindings for the right target atom.
   * Its index will correspond to the index in this array.
   */
  nsCOMArray<nsIDOMXPathResult> mValues;

public:

  nsXMLBindingValues() { MOZ_COUNT_CTOR(nsXMLBindingValues); }
  ~nsXMLBindingValues() { MOZ_COUNT_DTOR(nsXMLBindingValues); }

  nsXMLBindingSet* GetBindingSet() { return mBindings; }

  void SetBindingSet(nsXMLBindingSet* aBindings) { mBindings = aBindings; }

  PRInt32
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
   * aValue the value of the assignment
   */
  void
  GetAssignmentFor(nsXULTemplateResultXML* aResult,
                   nsXMLBinding* aBinding,
                   PRInt32 idx,
                   PRUint16 type,
                   nsIDOMXPathResult** aValue);

  void
  GetNodeAssignmentFor(nsXULTemplateResultXML* aResult,
                       nsXMLBinding* aBinding,
                       PRInt32 idx,
                       nsIDOMNode** aValue);

  void
  GetStringAssignmentFor(nsXULTemplateResultXML* aResult,
                         nsXMLBinding* aBinding,
                         PRInt32 idx,
                         nsAString& aValue);
};

#endif // nsXMLBinding_h__
