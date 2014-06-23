/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsEditProperty_h__
#define __nsEditProperty_h__

#include "nsISupports.h"

class nsIAtom;
class nsString;

/** simple interface for describing a single property as it relates to a range of content.
  *
  */

class nsEditProperty
{
public:

    static void RegisterAtoms();

#define EDITOR_ATOM(name_, value_) static nsIAtom* name_;
#include "nsEditPropertyAtomList.h"
#undef EDITOR_ATOM

};



#endif
