/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozEnglishWordUtils_h__
#define mozEnglishWordUtils_h__

#include "nsCOMPtr.h"
#include "nsString.h"

#include "mozITXTToHTMLConv.h"
#include "nsCycleCollectionParticipant.h"

class mozEnglishWordUtils final
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(mozEnglishWordUtils)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(mozEnglishWordUtils)

  mozEnglishWordUtils();

  /**
   * Given a unicode string and an offset, find the beginning and end of the
   * next word. begin and end are -1 if there are no words remaining in the
   * string. This should really be folded into the Line/WordBreaker.
   */
  nsresult FindNextWord(const char16_t* word, uint32_t length,
                        uint32_t offset, int32_t* begin, int32_t* end);

protected:
  virtual ~mozEnglishWordUtils();

  static bool ucIsAlpha(char16_t aChar);

  nsCOMPtr<mozITXTToHTMLConv> mURLDetector; // used to detect urls so the spell checker can skip them.
};

#endif
