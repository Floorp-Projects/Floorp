/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginHelperQt.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>

static const int kMaxtimeToProcessEvents = 30;

bool
PluginHelperQt::AnswerProcessSomeEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, kMaxtimeToProcessEvents);
    return true;
}
