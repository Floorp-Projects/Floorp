/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * an object that stores the result of determining whether a style struct that
 * was computed can be cached in the rule tree, and if so, what the conditions
 * it relies on are
 */

#ifndef RuleNodeCacheConditions_h_
#define RuleNodeCacheConditions_h_


namespace mozilla {

// Define this dummy class so there are fewer call sites to change when the old
// style system code is compiled out.
class RuleNodeCacheConditions
{
};

} // namespace mozilla


#endif // !defined(RuleNodeCacheConditions_h_)
