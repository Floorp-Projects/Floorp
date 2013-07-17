/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscompartmentinlines_h
#define jscompartmentinlines_h

#include "jscompartment.h"

#include "jscntxtinlines.h"

inline void
JSCompartment::initGlobal(js::GlobalObject &global)
{
    JS_ASSERT(global.compartment() == this);
    JS_ASSERT(!global_);
    global_ = &global;
}

js::GlobalObject *
JSCompartment::maybeGlobal() const
{
    JS_ASSERT_IF(global_, global_->compartment() == this);
    return global_;
}

js::AutoCompartment::AutoCompartment(JSContext *cx, JSObject *target)
  : cx_(cx),
    origin_(cx->compartment())
{
    cx_->enterCompartment(target->compartment());
}

js::AutoCompartment::AutoCompartment(ExclusiveContext *cx, JSCompartment *target)
  : cx_(cx->asJSContext()),
    origin_(cx_->compartment())
{
    cx_->enterCompartment(target);
}

js::AutoCompartment::~AutoCompartment()
{
    cx_->leaveCompartment(origin_);
}

#endif /* jscompartmentinlines_h */
