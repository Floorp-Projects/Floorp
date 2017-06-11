/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inlined methods for nsStyleContext. Will just redirect to
 * GeckoStyleContext methods when compiled without stylo, but will do
 * virtual dispatch (by checking which kind of container it is)
 * in stylo mode.
 */

#ifndef nsStyleContextInlines_h
#define nsStyleContextInlines_h

#include "nsStyleContext.h"
#include "mozilla/ServoStyleContext.h"
#include "mozilla/GeckoStyleContext.h"
#include "mozilla/ServoUtils.h"

using namespace mozilla;

MOZ_DEFINE_STYLO_METHODS(nsStyleContext, GeckoStyleContext, ServoStyleContext);

nsRuleNode*
nsStyleContext::RuleNode()
{
    MOZ_RELEASE_ASSERT(IsGecko());
    return AsGecko()->RuleNode();
}

#endif // nsStyleContextInlines_h
