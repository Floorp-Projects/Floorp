/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jscrashreport_h___
#define jscrashreport_h___

#include "jstypes.h"
#include "jsutil.h"

namespace js {
namespace crash {

void
SnapshotGCStack();

void
SnapshotErrorStack();

void
SaveCrashData(uint64_t tag, void *ptr, size_t size);

template<size_t size, unsigned char marker>
class StackBuffer {
  private:
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    volatile unsigned char buffer[size + 4];

  public:
    StackBuffer(void *data JS_GUARD_OBJECT_NOTIFIER_PARAM) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;

        buffer[0] = marker;
        buffer[1] = '[';

        for (size_t i = 0; i < size; i++) {
            if (data)
                buffer[i + 2] = ((unsigned char *)data)[i];
            else
                buffer[i + 2] = 0;
        }

        buffer[size - 2] = ']';
        buffer[size - 1] = marker;
    }
};

} /* namespace crash */
} /* namespace js */

#endif /* jscrashreport_h___ */
