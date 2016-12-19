/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MozChecker.h"
#include "CustomTypeAnnotation.h"
#include "Utils.h"

bool MozChecker::hasCustomAnnotation(const Decl *D, const char *Spelling) {
  iterator_range<specific_attr_iterator<AnnotateAttr>> Attrs =
      D->specific_attrs<AnnotateAttr>();

  for (AnnotateAttr *Attr : Attrs) {
    if (Attr->getAnnotation() == Spelling) {
      return true;
    }
  }

  return false;
}
