/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HyperTextAccessibleBase_H_
#define _HyperTextAccessibleBase_H_

namespace mozilla::a11y {

// This character marks where in the text returned via Text interface,
// that embedded object characters exist
const char16_t kEmbeddedObjectChar = 0xfffc;
const char16_t kImaginaryEmbeddedObjectChar = ' ';
const char16_t kForcedNewLineChar = '\n';

class HyperTextAccessibleBase {};

}  // namespace mozilla::a11y

#endif
