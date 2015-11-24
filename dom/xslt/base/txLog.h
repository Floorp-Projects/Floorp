/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txLog_h__
#define txLog_h__

#include "mozilla/Logging.h"

class txLog
{
public:
    static mozilla::LazyLogModule xpath;
    static mozilla::LazyLogModule xslt;
};

#define TX_LG_IMPL \
    mozilla::LazyLogModule txLog::xpath("xpath"); \
    mozilla::LazyLogModule txLog::xslt("xslt");

#define TX_LG_CREATE

#endif
