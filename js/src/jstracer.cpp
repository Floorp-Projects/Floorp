/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
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
 *   Brendan Eich <brendan@mozilla.org
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

#define jstracer_cpp___

#include "jsinterp.cpp"

uint32
js_AllocateLoopTableSlots(JSContext* cx, uint32 nloops)
{
    jsword* cursorp = &cx->runtime->loopTableCursor;
    jsword cursor, fencepost;

    do {
        cursor = *cursorp;
        fencepost = cursor + nloops;
        if (fencepost > LOOP_TABLE_LIMIT) {
            // try slow path
            return LOOP_TABLE_NO_SLOT;
        }
    } while (!js_CompareAndSwap(cursorp, cursor, fencepost));
    return (uint32) cursor;
}

void
js_FreeLoopTableSlots(JSContext* cx, uint32 base, uint32 nloops)
{
    jsword* cursorp = &cx->runtime->loopTableCursor;
    jsword cursor;

    cursor = *cursorp;
    JS_ASSERT(cursor >= (jsword) (base + nloops));

    if ((uint32) cursor == base + nloops &&
        js_CompareAndSwap(cursorp, cursor, base)) {
        return;
    }

    // slow path
}

/*
 * To grow the loop table that we take the traceMonitor lock, double check that
 * no other thread grew the table while we were deciding to grow the table, and
 * only then double the size of the loop table.
 *
 * The initial size of the table is 2^8 and grows to at most 2^24 entries. It
 * is extended at most a constant number of times (C=16) by doubling its size
 * every time. When extending the table, each slot is initially filled with
 * JSVAL_ZERO.
 */
bool
js_GrowLoopTable(JSContext* cx, uint32 slot)
{
    JSTraceMonitor* tm = &JS_TRACE_MONITOR(cx);
    uint32 oldSize = tm->loopTableSize;

    if (slot >= oldSize) {
        uint32 newSize = oldSize << 1;
        jsval* t = tm->loopTable;
        if (t) {
            t = (jsval*) JS_realloc(cx, t, newSize * sizeof(jsval));
        } else {
            JS_ASSERT(oldSize == 0);
            newSize = 256;
            t = (jsval*) JS_malloc(cx, newSize * sizeof(jsval));
        }
        if (!t)
            return false;
        for (uint32 n = oldSize; n < newSize; ++n)
            t[n] = JSVAL_ZERO;
        tm->loopTable = t;
        tm->loopTableSize = newSize;
    }
    return true;
}
