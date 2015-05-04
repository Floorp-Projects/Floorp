/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubMutationObserver is an implementation of the nsIMutationObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#ifndef nsStubMutationObserver_h_
#define nsStubMutationObserver_h_

#include "nsIMutationObserver.h"

/**
 * There are two advantages to inheriting from nsStubMutationObserver
 * rather than directly from nsIMutationObserver:
 *  1. smaller compiled code size (since there's no need for the code
 *     for the empty virtual function implementations for every
 *     nsIMutationObserver implementation)
 *  2. the performance of document's loop over observers benefits from
 *     the fact that more of the functions called are the same (which
 *     can reduce instruction cache misses and perhaps improve branch
 *     prediction)
 */
class nsStubMutationObserver : public nsIMutationObserver {
public:
  NS_DECL_NSIMUTATIONOBSERVER
};

#endif /* !defined(nsStubMutationObserver_h_) */
