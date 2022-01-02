/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_REMOVEPAREN_H_
#define DOM_QUOTA_REMOVEPAREN_H_

// See
// https://stackoverflow.com/questions/24481810/how-to-remove-the-enclosing-parentheses-with-macro
#define MOZ_REMOVE_PAREN(X) MOZ_REMOVE_PAREN_HELPER2(MOZ_REMOVE_PAREN_HELPER X)
#define MOZ_REMOVE_PAREN_HELPER(...) MOZ_REMOVE_PAREN_HELPER __VA_ARGS__
#define MOZ_REMOVE_PAREN_HELPER2(...) MOZ_REMOVE_PAREN_HELPER3(__VA_ARGS__)
#define MOZ_REMOVE_PAREN_HELPER3(...) MOZ_REMOVE_PAREN_HELPER4_##__VA_ARGS__
#define MOZ_REMOVE_PAREN_HELPER4_MOZ_REMOVE_PAREN_HELPER

#endif  // DOM_QUOTA_REMOVEPAREN_H_
