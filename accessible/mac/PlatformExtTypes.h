/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_PlatformExtTypes_h__
#define mozilla_a11y_PlatformExtTypes_h__

namespace mozilla {
namespace a11y {

enum class EWhichRange {
  eLeftWord,
  eRightWord,
  eLine,
  eLeftLine,
  eRightLine,
  eParagraph,
  eStyle
};

enum class EWhichPostFilter { eContainsText };

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_PlatformExtTypes_h__
