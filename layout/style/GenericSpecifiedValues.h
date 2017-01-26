/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Generic representation of a container of specified CSS values, which
 * could potentially be Servo- or Gecko- format. Used to make attribute mapping
 * code generic over style backends.
 */

#ifndef mozilla_GenericSpecifiedValues_h
#define mozilla_GenericSpecifiedValues_h

#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsPresContext.h"

struct nsRuleData;

// This provides a common interface for attribute mappers (MapAttributesIntoRule)
// to use regardless of the style backend. If the style backend is Gecko,
// this will contain an nsRuleData. If it is Servo, it will be a PropertyDeclarationBlock.
class GenericSpecifiedValues {
public:
    // Check if we already contain a certain longhand
    virtual bool PropertyIsSet(nsCSSPropertyID aId) = 0;

    virtual nsRuleData* AsRuleData() = 0;
};

#endif // mozilla_GenericSpecifiedValues_h