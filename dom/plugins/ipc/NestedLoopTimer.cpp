/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QtCore/QTimer>

#include "NestedLoopTimer.h"
#include "mozilla/plugins/PluginModuleChild.h"

namespace mozilla {
namespace plugins {

NestedLoopTimer::NestedLoopTimer(PluginModuleChild *pmc):
     QObject(), mModule(pmc), mQTimer(nullptr)
{
}

NestedLoopTimer::~NestedLoopTimer()
{
    if (mQTimer) {
        mQTimer->stop();
        delete mQTimer;
        mQTimer = nullptr;
    }
}

void NestedLoopTimer::timeOut()
{
    // just detected a nested loop; start a timer that will
    // periodically rpc-call back into the browser and process some
    // events
    mQTimer = new QTimer(this);
    QObject::connect(mQTimer, SIGNAL(timeout()), this,
                     SLOT(processSomeEvents()));
    mQTimer->setInterval(kNestedLoopDetectorIntervalMs);
    mQTimer->start();
}

void NestedLoopTimer::processSomeEvents()
{
    if (mModule)
        mModule->CallProcessSomeEvents();
}

} /* namespace plugins */
} /* namespace mozilla */
