/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Static helper class for implementing atom lists.
 */

#ifndef nsAtomListUtils_h__
#define nsAtomListUtils_h__

#include <stdint.h>

class nsAtom;
struct nsStaticAtomSetup;

class nsAtomListUtils {
public:
  static bool IsMember(nsAtom *aAtom,
                       const nsStaticAtomSetup* aSetup,
                       uint32_t aCount);
};

#endif /* !defined(nsAtomListUtils_h__) */
