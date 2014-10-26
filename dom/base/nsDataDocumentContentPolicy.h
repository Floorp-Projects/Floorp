/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Content policy implementation that prevents all loads of images,
 * subframes, etc from documents loaded as data (eg documents loaded
 * via XMLHttpRequest).
 */

#ifndef nsDataDocumentContentPolicy_h__
#define nsDataDocumentContentPolicy_h__

/* 1147d32c-215b-4014-b180-07fe7aedf915 */
#define NS_DATADOCUMENTCONTENTPOLICY_CID \
 {0x1147d32c, 0x215b, 0x4014, {0xb1, 0x80, 0x07, 0xfe, 0x7a, 0xed, 0xf9, 0x15}}
#define NS_DATADOCUMENTCONTENTPOLICY_CONTRACTID \
 "@mozilla.org/data-document-content-policy;1"


#include "nsIContentPolicy.h"
#include "mozilla/Attributes.h"

class nsDataDocumentContentPolicy MOZ_FINAL : public nsIContentPolicy
{
  ~nsDataDocumentContentPolicy()
  {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY

  nsDataDocumentContentPolicy()
  {}
};


#endif /* nsDataDocumentContentPolicy_h__ */
