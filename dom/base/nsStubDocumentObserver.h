/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubDocumentObserver is an implementation of the nsIDocumentObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#ifndef nsStubDocumentObserver_h_
#define nsStubDocumentObserver_h_

#include "nsIDocumentObserver.h"

/**
 * There are two advantages to inheriting from nsStubDocumentObserver
 * rather than directly from nsIDocumentObserver:
 *  1. smaller compiled code size (since there's no need for the code
 *     for the empty virtual function implementations for every
 *     nsIDocumentObserver implementation)
 *  2. the performance of document's loop over observers benefits from
 *     the fact that more of the functions called are the same (which
 *     can reduce instruction cache misses and perhaps improve branch
 *     prediction)
 */
class nsStubDocumentObserver : public nsIDocumentObserver {
public:
  NS_DECL_NSIDOCUMENTOBSERVER
};

#endif /* !defined(nsStubDocumentObserver_h_) */
