/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_VARIABLEMAP_H
#define TRANSFRMX_VARIABLEMAP_H

#include "nsError.h"
#include "txXMLUtils.h"
#include "txExprResult.h"
#include "txExpandedNameMap.h"

/**
 * Map that maps from expanded name to an expression result value. This is just
 * a base class, use txVariableMap or txParameterMap instead.
 */
class txVariableMapBase {
 public:
  nsresult bindVariable(const txExpandedName& aName, txAExprResult* aValue);

  void getVariable(const txExpandedName& aName, txAExprResult** aResult);

  void removeVariable(const txExpandedName& aName);

 protected:
  txVariableMapBase() = default;
  ~txVariableMapBase();

  txExpandedNameMap<txAExprResult> mMap;
};

/**
 * Map for mapping from expanded name to variable values. This is not
 * refcounted, so owners need to be careful to clean this up.
 */
class txVariableMap : public txVariableMapBase {
 public:
  txVariableMap() { MOZ_COUNT_CTOR(txVariableMap); }
  MOZ_COUNTED_DTOR(txVariableMap)
};

/**
 * Map for mapping from expanded name to parameter values. This is refcounted,
 * so multiple owners can hold a reference.
 */
class txParameterMap : public txVariableMapBase {
 public:
  NS_INLINE_DECL_REFCOUNTING(txParameterMap)

 private:
  ~txParameterMap() = default;
};

inline txVariableMapBase::~txVariableMapBase() {
  txExpandedNameMap<txAExprResult>::iterator iter(mMap);
  while (iter.next()) {
    txAExprResult* res = iter.value();
    NS_RELEASE(res);
  }
}

inline nsresult txVariableMapBase::bindVariable(const txExpandedName& aName,
                                                txAExprResult* aValue) {
  NS_ASSERTION(aValue, "can't add null-variables to a txVariableMap");
  nsresult rv = mMap.add(aName, aValue);
  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(aValue);
  } else if (rv == NS_ERROR_XSLT_ALREADY_SET) {
    rv = NS_ERROR_XSLT_VAR_ALREADY_SET;
  }
  return rv;
}

inline void txVariableMapBase::getVariable(const txExpandedName& aName,
                                           txAExprResult** aResult) {
  *aResult = mMap.get(aName);
  if (*aResult) {
    NS_ADDREF(*aResult);
  }
}

inline void txVariableMapBase::removeVariable(const txExpandedName& aName) {
  txAExprResult* var = mMap.remove(aName);
  NS_IF_RELEASE(var);
}

#endif  // TRANSFRMX_VARIABLEMAP_H
