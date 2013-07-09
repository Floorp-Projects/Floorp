/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SharedContext_inl_h
#define frontend_SharedContext_inl_h

#include "frontend/Parser.h"
#include "frontend/SharedContext.h"

namespace js {
namespace frontend {

inline bool
SharedContext::needStrictChecks()
{
    return context->hasExtraWarningsOption() || strict;
}

} /* namespace frontend */
} // namespace js

#endif /* frontend_SharedContext_inl_h */
