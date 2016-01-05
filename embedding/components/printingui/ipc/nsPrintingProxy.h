/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsPrintingProxy_h
#define __nsPrintingProxy_h

#include "nsIPrintingPromptService.h"
#include "mozilla/embedding/PPrintingChild.h"

namespace mozilla {
namespace layout {
class PRemotePrintJobChild;
}
}

class nsPrintingProxy: public nsIPrintingPromptService,
                       public mozilla::embedding::PPrintingChild
{
    virtual ~nsPrintingProxy();

public:
    nsPrintingProxy();

    static already_AddRefed<nsPrintingProxy> GetInstance();

    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINTINGPROMPTSERVICE

    nsresult SavePrintSettings(nsIPrintSettings* aPS,
                               bool aUsePrinterNamePrefix,
                               uint32_t aFlags);

    virtual PPrintProgressDialogChild*
    AllocPPrintProgressDialogChild() override;

    virtual bool
    DeallocPPrintProgressDialogChild(PPrintProgressDialogChild* aActor) override;

    virtual PPrintSettingsDialogChild*
    AllocPPrintSettingsDialogChild() override;

    virtual bool
    DeallocPPrintSettingsDialogChild(PPrintSettingsDialogChild* aActor) override;

    virtual PRemotePrintJobChild*
    AllocPRemotePrintJobChild() override;

    virtual bool
    DeallocPRemotePrintJobChild(PRemotePrintJobChild* aActor) override;
};

#endif

