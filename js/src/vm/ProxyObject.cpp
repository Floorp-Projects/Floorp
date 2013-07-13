/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ProxyObject.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

#include "gc/Barrier-inl.h"
#include "vm/ObjectImpl-inl.h"

using namespace js;

void
ProxyObject::initCrossCompartmentPrivate(HandleValue priv)
{
    initCrossCompartmentSlot(PRIVATE_SLOT, priv);
}

void
ProxyObject::initHandler(BaseProxyHandler *handler)
{
    initSlot(HANDLER_SLOT, PrivateValue(handler));
}

static void
NukeSlot(ProxyObject *proxy, uint32_t slot)
{
    Value old = proxy->getSlot(slot);
    if (old.isMarkable()) {
        Zone *zone = ZoneOfValue(old);
        AutoMarkInDeadZone amd(zone);
        proxy->setReservedSlot(slot, NullValue());
    } else {
        proxy->setReservedSlot(slot, NullValue());
    }
}

void
ProxyObject::nuke(BaseProxyHandler *handler)
{
    NukeSlot(this, PRIVATE_SLOT);
    setHandler(handler);

    NukeSlot(this, EXTRA_SLOT + 0);
    NukeSlot(this, EXTRA_SLOT + 1);

    if (is<FunctionProxyObject>())
        as<FunctionProxyObject>().nukeExtra();
}

void
FunctionProxyObject::nukeExtra()
{
    NukeSlot(this, CALL_SLOT);
    NukeSlot(this, CONSTRUCT_SLOT);
}
