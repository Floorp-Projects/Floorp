/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTextToSubURI_h__
#define nsTextToSubURI_h__

#include "nsITextToSubURI.h"
#include "nsString.h"

//==============================================================
class nsTextToSubURI: public nsITextToSubURI {
  NS_DECL_ISUPPORTS
  NS_DECL_NSITEXTTOSUBURI

public:
  nsTextToSubURI();

private:
  virtual ~nsTextToSubURI();

  // IRI is "Internationalized Resource Identifiers"
  // http://www.ietf.org/internet-drafts/draft-duerst-iri-01.txt
  // 
  // if the IRI option is true then we assume that the URI is encoded as UTF-8
  // note: there is no definite way to distinguish between IRI and a URI encoded 
  // with a non-UTF-8 charset
  // Use this option carefully -- it may cause dataloss
  // (recommended to set to true for UI purpose only)
  //
  nsresult convertURItoUnicode(const nsAFlatCString &aCharset,
                               const nsAFlatCString &aURI, 
                               bool aIRI, 
                               nsAString &_retval);
};

#endif // nsTextToSubURI_h__

