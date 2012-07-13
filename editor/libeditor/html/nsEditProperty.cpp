/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsMemory.h"
#include "nsStaticAtom.h"
#include "nsEditProperty.h"

using namespace mozilla;

#define EDITOR_ATOM(name_, value_) nsIAtom* nsEditProperty::name_ = 0;
#include "nsEditPropertyAtomList.h" // IWYU pragma: keep
#undef EDITOR_ATOM

/* From the HTML 4.0 DTD, 

INLINE:
<!-- %inline; covers inline or "text-level" elements -->
 <!ENTITY % inline "#PCDATA | %fontstyle; | %phrase; | %special; | %formctrl;">
 <!ENTITY % fontstyle "TT | I | B | BIG | SMALL">
 <!ENTITY % phrase "EM | STRONG | DFN | CODE |
                    SAMP | KBD | VAR | CITE | ABBR | ACRONYM" >
 <!ENTITY % special
    "A | IMG | OBJECT | BR | SCRIPT | MAP | Q | SUB | SUP | SPAN | BDO">
 <!ENTITY % formctrl "INPUT | SELECT | TEXTAREA | LABEL | BUTTON">

BLOCK:
<!ENTITY % block
      "P | %heading (h1-h6); | %list (UL | OL); | %preformatted (PRE); | DL | DIV | NOSCRIPT |
       BLOCKQUOTE | FORM | HR | TABLE | FIELDSET | ADDRESS">

But what about BODY, TR, TD, TH, CAPTION, COL, COLGROUP, THEAD, TFOOT, LI, DT, DD, LEGEND, etc.?
 

*/

#define EDITOR_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "nsEditPropertyAtomList.h" // IWYU pragma: keep
#undef EDITOR_ATOM

void
nsEditProperty::RegisterAtoms()
{
  // inline tags
  static const nsStaticAtom property_atoms[] = {
#define EDITOR_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &name_),
#include "nsEditPropertyAtomList.h" // IWYU pragma: keep
#undef EDITOR_ATOM
  };
  
  NS_RegisterStaticAtoms(property_atoms);
}
