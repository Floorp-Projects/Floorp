/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXXSLTPROCESSOR_H
#define TRANSFRMX_TXXSLTPROCESSOR_H

#include "txExecutionState.h"

class txXSLTProcessor
{
public:
    /**
     * Initialisation and shutdown routines. Initilizes and cleansup all
     * dependant classes
     */
    static bool init();
    static void shutdown();


    static nsresult execute(txExecutionState& aEs);

    // once we want to have interuption we should probably have functions for
    // running X number of steps or running until a condition is true.
};

#endif
