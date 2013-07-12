/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* PropertyKey implementation details. */

#include "js/PropertyKey.h"

#include "jsatom.h"
#include "jscntxt.h"

#include "js/RootingAPI.h"
#include "js/Value.h"
#include "vm/String.h"

using namespace js;

bool
JS::detail::ToPropertyKeySlow(JSContext *cx, HandleValue v, PropertyKey *key)
{
    MOZ_ASSERT_IF(v.isInt32(), v.toInt32() < 0);

    RootedAtom atom(cx);
    if (!ToAtom<CanGC>(cx, v))
        return false;

    uint32_t index;
    if (atom->isIndex(&index)) {
        *key = PropertyKey(index);
        return true;
    }

    key->v.setString(atom->asPropertyName());
    return true;
}
