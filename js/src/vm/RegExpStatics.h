/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RegExpStatics_h__
#define RegExpStatics_h__

#include "mozilla/GuardObjects.h"

#include "jscntxt.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/Vector.h"

#include "vm/MatchPairs.h"
#include "vm/RegExpObject.h"

namespace js {

class PreserveRegExpStatics;
class RegExpStatics;

size_t SizeOfRegExpStaticsData(const JSObject *obj, JSMallocSizeOfFun mallocSizeOf);

} /* namespace js */

#endif /* RegExpStatics_h__ */
