/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
SaveCrashData(uint64 tag, void *ptr, size_t size);

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
