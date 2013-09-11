/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_GeneratorObject_h
#define vm_GeneratorObject_h

#include "jsobj.h"

namespace js {

class LegacyGeneratorObject : public JSObject
{
  public:
    static const Class class_;

    JSGenerator *getGenerator() { return static_cast<JSGenerator*>(getPrivate()); }
};

class StarGeneratorObject : public JSObject
{
  public:
    static const Class class_;

    JSGenerator *getGenerator() { return static_cast<JSGenerator*>(getPrivate()); }
};

} // namespace js

#endif /* vm_GeneratorObject_h */
