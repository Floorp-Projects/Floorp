/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * June 12, 2009.
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Chris Leary <cdleary@mozilla.com>
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

#ifndef jsarenainlines_h___
#define jsarenainlines_h___

#include "jsarena.h"

const jsuword JSArenaPool::POINTER_MASK = jsuword(JS_ALIGN_OF_POINTER - 1);

jsuword
JSArenaPool::headerBaseMask() const
{
    return mask | POINTER_MASK;
}

jsuword
JSArenaPool::headerSize() const
{
    return sizeof(JSArena **) + ((mask < POINTER_MASK) ? POINTER_MASK - mask : 0);
}

void *
JSArenaPool::grow(jsuword p, size_t size, size_t incr)
{
    countGrowth(size, incr);
    if (current->avail == p + align(size)) {
        size_t nb = align(size + incr);
        if (current->limit >= nb && p <= current->limit - nb) {
            current->avail = p + nb;
            countInplaceGrowth(size, incr);
        } else if (p == current->base) {
            p = jsuword(reallocInternal((void *) p, size, incr));
        } else {
            p = jsuword(growInternal((void *) p, size, incr));
        }
    } else {
        p = jsuword(growInternal((void *) p, size, incr));
    }
    return (void *) p;
}

#endif
