/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPrintingPromptServiceProxy_h
#define __nsPrintingPromptServiceProxy_h

#include "nsIPrintingPromptService.h"
#include "mozilla/embedding/PPrintingChild.h"

class nsPrintingPromptServiceProxy: public nsIPrintingPromptService,
                                    public mozilla::embedding::PPrintingChild
{
    virtual ~nsPrintingPromptServiceProxy();

public:
    nsPrintingPromptServiceProxy();

    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINTINGPROMPTSERVICE
};

#endif

