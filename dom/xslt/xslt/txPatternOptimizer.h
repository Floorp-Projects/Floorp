/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txPatternOptimizer_h__
#define txPatternOptimizer_h__

#include "txXPathOptimizer.h"

class txPattern;

class txPatternOptimizer
{
public:
    /**
     * Optimize the given pattern.
     * @param aInPattern    Pattern to optimize.
     * @param aOutPattern   Resulting pattern, null if optimization didn't
     *                      result in a new pattern.
     */
    nsresult optimize(txPattern* aInPattern, txPattern** aOutPattern);

private:

    // Helper methods for optimizing specific classes
    nsresult optimizeStep(txPattern* aInPattern, txPattern** aOutPattern);

    txXPathOptimizer mXPathOptimizer;
};

#endif //txPatternOptimizer_h__
