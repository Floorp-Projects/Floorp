/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_embedding_PrintingParent_h
#define mozilla_embedding_PrintingParent_h

#include "mozilla/embedding/PPrintingParent.h"
#include "mozilla/dom/PBrowserParent.h"

namespace mozilla {
namespace embedding {
class PrintingParent : public PPrintingParent
{
public:
    virtual bool
    RecvShowProgress(PBrowserParent* parent,
                     const bool& isForPrinting);
    virtual bool
    RecvShowPrintDialog(PBrowserParent* parent,
                        const PrintData& initSettings,
                        PrintData* retVal,
                        bool* success);

    virtual void
    ActorDestroy(ActorDestroyReason aWhy);

    MOZ_IMPLICIT PrintingParent();
    virtual ~PrintingParent();
};
} // namespace embedding
} // namespace mozilla

#endif

