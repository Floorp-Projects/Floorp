/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TEXT_HANDLER_H
#define TRANSFRMX_TEXT_HANDLER_H

#include "txXMLEventHandler.h"
#include "nsString.h"

class txTextHandler : public txAXMLEventHandler
{
public:
    txTextHandler(bool aOnlyText);

    TX_DECL_TXAXMLEVENTHANDLER

    nsString mValue;

private:
    PRUint32 mLevel;
    bool mOnlyText;
};

#endif
