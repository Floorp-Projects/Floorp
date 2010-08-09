/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXMLBinding_h__
#define nsXMLBinding_h__

#include "nsAutoPtr.h"
#include "nsIAtom.h"
#include "nsCycleCollectionParticipant.h"

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
class nsXMLBindingSet
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
