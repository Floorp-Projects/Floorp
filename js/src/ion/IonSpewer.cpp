/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <adrake@adrake.org>
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

#ifdef DEBUG

#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

// IonSpewer singleton.
static IonSpewer ionspewer;

static bool LoggingChecked = false;
static uint32 LoggingBits = 0;

static const char *ChannelNames[] =
{
#define IONSPEW_CHANNEL(name) #name,
    IONSPEW_CHANNEL_LIST(IONSPEW_CHANNEL)
#undef IONSPEW_CHANNEL
};


void
ion::IonSpewNewFunction(MIRGraph *graph, JSScript *function)
{
    ionspewer.init();
    ionspewer.beginFunction(graph, function);
}

void
ion::IonSpewPass(const char *pass)
{
    ionspewer.spewPass(pass);
}

void
ion::IonSpewPass(const char *pass, LinearScanAllocator *ra)
{
    ionspewer.spewPass(pass, ra);
}

void
ion::IonSpewEndFunction()
{
    ionspewer.endFunction();
}


IonSpewer::~IonSpewer() {
    if (!inited_)
        return;
    c1Spewer.finish();
    jsonSpewer.finish();
}

bool
IonSpewer::init()
{
    if (inited_)
        return true;

    if (!c1Spewer.init("/tmp/ion.cfg"))
        return false;
    if (!jsonSpewer.init("/tmp/ion.json"))
        return false;

    inited_ = true;
    return true;
}

void
IonSpewer::beginFunction(MIRGraph *graph, JSScript *function)
{
    JS_ASSERT(inited_);

    this->graph = graph;
    this->function = function;

    c1Spewer.beginFunction(graph, function);
    jsonSpewer.beginFunction(function);
}

void
IonSpewer::spewPass(const char *pass)
{
    JS_ASSERT(inited_);
    c1Spewer.spewPass(pass);
    jsonSpewer.beginPass(pass);
    jsonSpewer.spewMIR(graph);
    jsonSpewer.spewLIR(graph);
    jsonSpewer.endPass();
}

void
IonSpewer::spewPass(const char *pass, LinearScanAllocator *ra)
{
    JS_ASSERT(inited_);
    c1Spewer.spewPass(pass);
    c1Spewer.spewIntervals(pass, ra);
    jsonSpewer.beginPass(pass);
    jsonSpewer.spewMIR(graph);
    jsonSpewer.spewLIR(graph);
    jsonSpewer.spewIntervals(ra);
    jsonSpewer.endPass();
}

void
IonSpewer::endFunction()
{
    JS_ASSERT(inited_);
    c1Spewer.endFunction();
    jsonSpewer.endFunction();
}


FILE *ion::IonSpewFile = NULL;

static bool
ContainsFlag(const char *str, const char *flag)
{
    size_t flaglen = strlen(flag);
    const char *index = strstr(str, flag);
    while (index) {
        if ((index == str || index[-1] == ' ') && (index[flaglen] == 0 || index[flaglen] == ' '))
            return true;
        index = strstr(index + flaglen, flag);
    }
    return false;
}

void
ion::CheckLogging()
{
    if (LoggingChecked)
        return;
    LoggingChecked = true;
    const char *env = getenv("IONFLAGS");
    if (!env)
        return;
    if (strstr(env, "help")) {
        fflush(NULL);
        printf(
            "\n"
            "usage: IONFLAGS=option,option,option,... where options can be:\n"
            "\n"
            "  aborts   Compilation abort messages\n"
            "  mir      MIR information\n"
            "  gvn      Global Value Numbering\n"
            "  licm     Loop invariant code motion\n"
            "  lsra     Linear scan register allocation\n"

            "  all      Everything\n"
            "\n"
        );
        exit(0);
        /*NOTREACHED*/
    }
    if (ContainsFlag(env, "aborts"))
        LoggingBits |= (1 << uint32(IonSpew_Abort));
    if (ContainsFlag(env, "mir"))
        LoggingBits |= (1 << uint32(IonSpew_MIR));
    if (ContainsFlag(env, "gvn"))
        LoggingBits |= (1 << uint32(IonSpew_GVN));
    if (ContainsFlag(env, "licm"))
        LoggingBits |= (1 << uint32(IonSpew_LICM));
    if (ContainsFlag(env, "lsra"))
        LoggingBits |= (1 << uint32(IonSpew_LSRA));
    if (ContainsFlag(env, "snapshots"))
        LoggingBits |= (1 << uint32(IonSpew_Snapshots));
    if (ContainsFlag(env, "all"))
        LoggingBits = uint32(-1);

    IonSpewFile = stderr;
}

void
ion::IonSpewVA(IonSpewChannel channel, const char *fmt, va_list ap)
{
    if (!IonSpewEnabled(channel))
        return;

    IonSpewHeader(channel);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void
ion::IonSpew(IonSpewChannel channel, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    IonSpewVA(channel, fmt, ap);
    va_end(ap);
}

void
ion::IonSpewHeader(IonSpewChannel channel)
{
    if (!IonSpewEnabled(channel))
        return;

    fprintf(stderr, "[%s] ", ChannelNames[channel]);
}

bool
ion::IonSpewEnabled(IonSpewChannel channel)
{
    JS_ASSERT(LoggingChecked);

    return LoggingBits & (1 << uint32(channel));
}

#endif /* DEBUG */

