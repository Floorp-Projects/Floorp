/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsJISx4501LineBreaker_h__
#define nsJISx4501LineBreaker_h__


#include "nsILineBreaker.h"

class nsJISx4051LineBreaker : public nsILineBreaker
{
  NS_DECL_ISUPPORTS

public:
  nsJISx4051LineBreaker();
  virtual ~nsJISx4051LineBreaker();

  PRInt32 Next( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos);

  PRInt32 Prev( const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos);

  virtual void GetJISx4051Breaks(const PRUnichar* aText, PRUint32 aLength,
                                 PRUint8 aBreakMode,
                                 PRUint8* aBreakBefore);
  virtual void GetJISx4051Breaks(const PRUint8* aText, PRUint32 aLength,
                                 PRUint8 aBreakMode,
                                 PRUint8* aBreakBefore);

private:
  PRInt32 WordMove(const PRUnichar* aText, PRUint32 aLen, PRUint32 aPos,
                   PRInt8 aDirection);
};

#endif  /* nsJISx4501LineBreaker_h__ */
