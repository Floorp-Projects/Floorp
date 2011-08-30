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
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   Julian Seward <jseward@acm.org>
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

#if !defined jsjaeger_logging_h__
#define jsjaeger_logging_h__

#include "assembler/wtf/Platform.h"
#include "prmjtime.h"

#if defined(JS_METHODJIT) || ENABLE_YARR_JIT

namespace js {

#define JSPEW_CHAN_MAP(_)   \
    _(Abort)                \
    _(Scripts)              \
    _(Prof)                 \
    _(JSOps)                \
    _(Insns)                \
    _(VMFrame)              \
    _(PICs)                 \
    _(SlowCalls)            \
    _(Analysis)             \
    _(Regalloc)             \
    _(Inlining)             \
    _(Recompile)

enum JaegerSpewChannel {
#define _(name) JSpew_##name,
    JSPEW_CHAN_MAP(_)
#undef  _
    JSpew_Terminator
};

#if defined(DEBUG) && !defined(JS_METHODJIT_SPEW)
# define JS_METHODJIT_SPEW
#endif

#if defined(JS_METHODJIT_SPEW)

void JMCheckLogging();

struct ConditionalLog {
    uint32 oldBits;
    bool logging;
    ConditionalLog(bool logging);
    ~ConditionalLog();
};

bool IsJaegerSpewChannelActive(JaegerSpewChannel channel);
#ifdef __GNUC__
void JaegerSpew(JaegerSpewChannel channel, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
#else
void JaegerSpew(JaegerSpewChannel channel, const char *fmt, ...);
#endif

struct Profiler {
    JSInt64 t_start;
    JSInt64 t_stop;

    static inline JSInt64 now() {
        return PRMJ_Now();
    }

    inline void start() {
        t_start = now();
    }

    inline void stop() {
        t_stop = now();
    }

    inline uint32 time_ms() {
        return uint32((t_stop - t_start) / PRMJ_USEC_PER_MSEC);
    }

    inline uint32 time_us() {
        return uint32(t_stop - t_start);
    }
};

#else

static inline void JaegerSpew(JaegerSpewChannel channel, const char *fmt, ...)
{
}

#endif

}

#endif

#endif

