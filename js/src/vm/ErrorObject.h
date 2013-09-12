/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ErrorObject_h_
#define vm_ErrorObject_h_

#include "jsobj.h"

struct JSExnPrivate;

namespace js {

class ErrorObject : public JSObject
{
  public:
    static const Class class_;

    JSExnPrivate *getExnPrivate() { return static_cast<JSExnPrivate*>(getPrivate()); }
};

} // namespace js

#endif // vm_ErrorObject_h_
