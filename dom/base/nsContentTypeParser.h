/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentTypeParser_h
#define nsContentTypeParser_h

#include "nsString.h"

class nsContentTypeParser {
 public:
  explicit nsContentTypeParser(const nsAString& aString);

  nsresult GetParameter(const char* aParameterName, nsAString& aResult) const;
  nsresult GetType(nsAString& aResult) const;

 private:
  NS_ConvertUTF16toUTF8 mString;
};

#endif
