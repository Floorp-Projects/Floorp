/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubImageDecoderObserver is an implementation of the imgIDecoderObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#ifndef nsStubImageDecoderObserver_h_
#define nsStubImageDecoderObserver_h_

#include "imgIDecoderObserver.h"

/**
 * There are two advantages to inheriting from nsStubImageDecoderObserver
 * rather than directly from imgIDecoderObserver:
 *  1. smaller compiled code size (since there's no need for the code
 *     for the empty virtual function implementations for every
 *     imgIDecoderObserver implementation)
 *  2. the performance of document's loop over observers benefits from
 *     the fact that more of the functions called are the same (which
 *     can reduce instruction cache misses and perhaps improve branch
 *     prediction)
 */
class nsStubImageDecoderObserver : public imgIDecoderObserver {
public:
    NS_DECL_IMGICONTAINEROBSERVER
    NS_DECL_IMGIDECODEROBSERVER
};

#endif /* !defined(nsStubImageDecoderObserver_h_) */
