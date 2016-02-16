/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/UbiNodeShortestPaths.h"

#include "jsstr.h"

namespace JS {
namespace ubi {

JS_PUBLIC_API(BackEdge::Ptr)
BackEdge::clone() const
{
    BackEdge::Ptr clone(js_new<BackEdge>());
    if (!clone)
        return nullptr;

    clone->predecessor_ = predecessor();
    if (name()) {
        clone->name_ = js::DuplicateString(name().get());
        if (!clone->name_)
            return nullptr;
    }
    return mozilla::Move(clone);
}

} // namespace ubi
} // namespace JS
