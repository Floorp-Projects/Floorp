/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsComplexBreaker_h__
#define nsComplexBreaker_h__

#include <stdint.h>
/**
 * Find line break opportunities in aText[] of aLength characters,
 * filling boolean values indicating line break opportunities for
 * corresponding charactersin aBreakBefore[] on return.
 */
void NS_GetComplexLineBreaks(const char16_t* aText, uint32_t aLength,
                             uint8_t* aBreakBefore);

class ComplexBreaker {
 public:
  static void Initialize();

  static void Shutdown();

  /**
   * A wrapper around the platform specific NS_GetComplexLineBreaks, which adds
   * caching of the results to mitigate sometimes expensive implementation.
   * @param aText - pointer to the text to process for possible line breaks
   * @param aLength - the length to process
   * @param aBreakBefore - result array correlated to aText, where element is
   *                       set to true if line can be broken before
   *                       corresponding character in aText and false otherwise
   */
  static void GetBreaks(const char16_t* aText, uint32_t aLength,
                        uint8_t* aBreakBefore);
};

#endif /* nsComplexBreaker_h__ */
