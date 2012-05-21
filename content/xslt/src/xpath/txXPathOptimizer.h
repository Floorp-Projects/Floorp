/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txXPathOptimizer_h__
#define txXPathOptimizer_h__

#include "txCore.h"

class Expr;

class txXPathOptimizer
{
public:
    /**
     * Optimize the given expression.
     * @param aInExpr       Expression to optimize.
     * @param aOutExpr      Resulting expression, null if optimization didn't
     *                      result in a new expression.
     */
    nsresult optimize(Expr* aInExpr, Expr** aOutExpr);

private:
    // Helper methods for optimizing specific classes
    nsresult optimizeStep(Expr* aInExpr, Expr** aOutExpr);
    nsresult optimizePath(Expr* aInExpr, Expr** aOutExpr);
    nsresult optimizeUnion(Expr* aInExpr, Expr** aOutExpr);
};

#endif
