/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ion_CompileInfo_inl_h
#define ion_CompileInfo_inl_h

#include "ion/CompileInfo.h"

#include "jsscriptinlines.h"

using namespace js;
using namespace ion;

inline RegExpObject *
CompileInfo::getRegExp(jsbytecode *pc) const
{
    return script_->getRegExp(GET_UINT32_INDEX(pc));
}

inline JSFunction *
CompileInfo::getFunction(jsbytecode *pc) const
{
    return script_->getFunction(GET_UINT32_INDEX(pc));
}

#endif /* ion_CompileInfo_inl_h */
