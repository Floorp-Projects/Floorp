/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* internal interface for implementing complex pseudo-classes */

#ifndef nsICSSPseudoComparator_h___
#define nsICSSPseudoComparator_h___

struct nsCSSSelector;

class nsICSSPseudoComparator
{
public:
  virtual bool PseudoMatches(nsCSSSelector* aSelector)=0;
};

#endif /* nsICSSPseudoComparator_h___ */
