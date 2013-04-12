/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaselineCompiler-x86-shared.h"

using namespace js;
using namespace js::ion;

BaselineCompilerX86Shared::BaselineCompilerX86Shared(JSContext *cx, HandleScript script)
  : BaselineCompilerShared(cx, script)
{
}
