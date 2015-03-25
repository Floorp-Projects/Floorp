/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsJISx4051LineBreaker_h__
#define nsJISx4051LineBreaker_h__


#include "nsILineBreaker.h"

class nsJISx4051LineBreaker : public nsILineBreaker
{
  NS_DECL_ISUPPORTS

private:
  virtual ~nsJISx4051LineBreaker();

public:
  nsJISx4051LineBreaker();

  int32_t Next( const char16_t* aText, uint32_t aLen, uint32_t aPos) override;

  int32_t Prev( const char16_t* aText, uint32_t aLen, uint32_t aPos) override;

  virtual void GetJISx4051Breaks(const char16_t* aText, uint32_t aLength,
                                 uint8_t aBreakMode,
                                 uint8_t* aBreakBefore) override;
  virtual void GetJISx4051Breaks(const uint8_t* aText, uint32_t aLength,
                                 uint8_t aBreakMode,
                                 uint8_t* aBreakBefore) override;

private:
  int32_t WordMove(const char16_t* aText, uint32_t aLen, uint32_t aPos,
                   int8_t aDirection);
};

#endif  /* nsJISx4051LineBreaker_h__ */
