/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __txCore_h__
#define __txCore_h__

#include "nscore.h"
#include "nsDebug.h"
#include "nsTraceRefcnt.h"

class nsAString;

class txObject
{
public:
    txObject()
    {
        MOZ_COUNT_CTOR(txObject);
    }

    /**
     * Deletes this txObject
     */
    virtual ~txObject()
    {
        MOZ_COUNT_DTOR(txObject);
    }
};

/**
 * Utility class for doubles
 */
class txDouble
{
public:
    /**
     * Converts the value of the given double to a string, and appends
     * the result to the destination string.
     */
    static void toString(double aValue, nsAString& aDest);

    /**
     * Converts the given String to a double, if the string value does not
     * represent a double, NaN will be returned.
     */
    static double toDouble(const nsAString& aStr);
};

#endif
