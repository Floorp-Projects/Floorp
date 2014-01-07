/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txLog_h__
#define txLog_h__

#include "prlog.h"

#ifdef PR_LOGGING
class txLog
{
public:
    static PRLogModuleInfo *xpath;
    static PRLogModuleInfo *xslt;
};

#define TX_LG_IMPL \
    PRLogModuleInfo * txLog::xpath = 0; \
    PRLogModuleInfo * txLog::xslt = 0;

#define TX_LG_CREATE \
    txLog::xpath = PR_NewLogModule("xpath"); \
    txLog::xslt  = PR_NewLogModule("xslt")

#else

#define TX_LG_IMPL
#define TX_LG_CREATE

#endif
#endif
