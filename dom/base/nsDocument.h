/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base class for all our document implementations.
 */

#ifndef nsDocument_h___
#define nsDocument_h___

#include "nsIDocument.h"

#define XML_DECLARATION_BITS_DECLARATION_EXISTS (1 << 0)
#define XML_DECLARATION_BITS_ENCODING_EXISTS (1 << 1)
#define XML_DECLARATION_BITS_STANDALONE_EXISTS (1 << 2)
#define XML_DECLARATION_BITS_STANDALONE_YES (1 << 3)

// Base class for our document implementations.
class nsDocument : public nsIDocument {
 protected:

  explicit nsDocument(const char* aContentType);
  virtual ~nsDocument();

  // Don't add stuff here, add to nsIDocument instead.

 private:
  nsDocument(const nsDocument& aOther) = delete;
  nsDocument& operator=(const nsDocument& aOther) = delete;
};

#endif /* nsDocument_h___ */
