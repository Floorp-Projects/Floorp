/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef __nsScriptableUConv_h_
#define __nsScriptableUConv_h_

#include "nsIScriptableUConv.h"
#include "nsCOMPtr.h"
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"

class nsScriptableUnicodeConverter : public nsIScriptableUnicodeConverter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTABLEUNICODECONVERTER

  nsScriptableUnicodeConverter();

protected:
  virtual ~nsScriptableUnicodeConverter();

  nsCString mCharset;
  nsCOMPtr<nsIUnicodeEncoder> mEncoder;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
  bool mIsInternal;

  nsresult FinishWithLength(char **_retval, int32_t* aLength);
  nsresult ConvertFromUnicodeWithLength(const nsAString& aSrc,
                                        int32_t* aOutLen,
                                        char **_retval);


  nsresult InitConverter();
};

#endif
