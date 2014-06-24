/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsUTF8ConverterService_h__
#define nsUTF8ConverterService_h__

#include "nsIUTF8ConverterService.h"

//==============================================================

class nsUTF8ConverterService;

namespace mozilla {
template<>
struct HasDangerousPublicDestructor<nsUTF8ConverterService>
{
  static const bool value = true;
};
}

class nsUTF8ConverterService: public nsIUTF8ConverterService {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUTF8CONVERTERSERVICE

public:
  nsUTF8ConverterService() {}
  virtual ~nsUTF8ConverterService() {}
};

#endif // nsUTF8ConverterService_h__

