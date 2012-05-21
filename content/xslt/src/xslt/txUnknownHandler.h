/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef txUnknownHandler_h___
#define txUnknownHandler_h___

#include "txBufferingHandler.h"
#include "txOutputFormat.h"

class txExecutionState;

class txUnknownHandler : public txBufferingHandler
{
public:
    txUnknownHandler(txExecutionState* aEs);
    virtual ~txUnknownHandler();

    TX_DECL_TXAXMLEVENTHANDLER

private:
    nsresult createHandlerAndFlush(bool aHTMLRoot,
                                   const nsSubstring& aName,
                                   const PRInt32 aNsID);

    /*
     * XXX we shouldn't hold to the txExecutionState, as we're supposed
     * to live without it. But as a standalone handler, we don't.
     * The right fix may need a txOutputFormat here.
     */
    txExecutionState* mEs;

    // If mFlushed is true then we've replaced mEs->mResultHandler with a
    // different handler and we should forward to that handler.
    bool mFlushed;
};

#endif /* txUnknownHandler_h___ */
