/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: Rod Spears (rods@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsSVGAtoms.h"
#include "nsStaticAtom.h"
#include "nsMemory.h"

// define storage for all atoms
#define SVG_ATOM(_name, _value) nsIAtom* nsSVGAtoms::_name;
#include "nsSVGAtomList.h"
#undef SVG_ATOM

static const nsStaticAtom SVGAtoms_info[] = {
#define SVG_ATOM(name_, value_) { value_, &nsSVGAtoms::name_ },
#include "nsSVGAtomList.h"
#undef SVG_ATOM
};

void nsSVGAtoms::AddRefAtoms() {

  NS_RegisterStaticAtoms(SVGAtoms_info, NS_ARRAY_LENGTH(SVGAtoms_info));
}

