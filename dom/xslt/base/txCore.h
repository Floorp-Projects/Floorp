/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __txCore_h__
#define __txCore_h__

#include "nscore.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsStringFwd.h"

class txObject {
 public:
  MOZ_COUNTED_DEFAULT_CTOR(txObject)

  /**
   * Deletes this txObject
   */
  MOZ_COUNTED_DTOR_VIRTUAL(txObject)
};

/**
 * Utility class for doubles
 */
class txDouble {
 public:
  /**
   * Converts the value of the given double to a string, and appends
   * the result to the destination string.
   */
  static void toString(double aValue, nsAString& aDest);

  /**
   * Converts the given String to a double, if the string value does not
   * represent a double, NaN will be returned.
   */
  static double toDouble(const nsAString& aStr);
};

#endif
