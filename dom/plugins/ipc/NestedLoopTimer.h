/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NESTEDLOOPTIMER_H
#define NESTEDLOOPTIMER_H

#include <QtCore/QObject>

class QTimer;

namespace mozilla {
namespace plugins {

class PluginModuleChild;

class NestedLoopTimer: public QObject
{
    Q_OBJECT
public:
    NestedLoopTimer(PluginModuleChild *pmc);

    virtual ~NestedLoopTimer();

public Q_SLOTS:
    virtual void timeOut();
    virtual void processSomeEvents();
   
private:
    PluginModuleChild *mModule;
    QTimer *mQTimer;
};

} /* namespace plugins */
} /* namespace mozilla */

#undef slots

#endif
