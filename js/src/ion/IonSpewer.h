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

#ifndef jsion_ion_spewer_h__
#define jsion_ion_spewer_h__

#include <stdarg.h>
#include "C1Spewer.h"
#include "JSONSpewer.h"
#include "LinearScan.h"
#include "MIRGraph.h"

namespace js {
namespace ion {

// New channels may be added below.
#define IONSPEW_CHANNEL_LIST(_)             \
    /* Used to abort SSA construction */    \
    _(Abort)                                \
    /* Information during MIR building */   \
    _(MIR)                                  \
    /* Information during GVN */            \
    _(GVN)                                  \
    /* Information during LICM */           \
    _(LICM)                                 \
    /* Information during LSRA */           \
    _(LSRA)                                 \
    /* Debug info about snapshots */        \
    _(Snapshots)

enum IonSpewChannel {
#define IONSPEW_CHANNEL(name) IonSpew_##name,
    IONSPEW_CHANNEL_LIST(IONSPEW_CHANNEL)
#undef IONSPEW_CHANNEL
    IonSpew_Terminator
};


// The IonSpewer is only available on debug builds.
// None of the global functions have effect on non-debug builds.

#ifdef DEBUG

class IonSpewer
{
  private:
    MIRGraph *graph;
    JSScript *function;
    C1Spewer c1Spewer;
    JSONSpewer jsonSpewer;
    bool inited_;

  public:
    IonSpewer()
      : graph(NULL), function(NULL), inited_(false)
    { }

    // File output is terminated safely upon destruction.
    ~IonSpewer();

    bool init();
    void beginFunction(MIRGraph *graph, JSScript *);
    void spewPass(const char *pass);
    void spewPass(const char *pass, LinearScanAllocator *ra);
    void endFunction();
};

void IonSpewNewFunction(MIRGraph *graph, JSScript *function);
void IonSpewPass(const char *pass);
void IonSpewPass(const char *pass, LinearScanAllocator *ra);
void IonSpewEndFunction();

void CheckLogging();
extern FILE *IonSpewFile;
void IonSpew(IonSpewChannel channel, const char *fmt, ...);
void IonSpewHeader(IonSpewChannel channel);
bool IonSpewEnabled(IonSpewChannel channel);
void IonSpewVA(IonSpewChannel channel, const char *fmt, va_list ap);

#else

static inline void IonSpewNewFunction(MIRGraph *graph, JSScript *function)
{ }
static inline void IonSpewPass(const char *pass)
{ }
static inline void IonSpewPass(const char *pass, LinearScanAllocator *ra)
{ }
static inline void IonSpewEndFunction()
{ }

static inline void CheckLogging()
{ }
static FILE *const IonSpewFile = NULL;
static inline void IonSpew(IonSpewChannel, const char *fmt, ...)
{ }
static inline void IonSpewHeader(IonSpewChannel channel)
{ }
static inline bool IonSpewEnabled(IonSpewChannel channel)
{ return false; }
static inline void IonSpewVA(IonSpewChannel, const char *fmt, va_list ap)
{ } 

#endif /* DEBUG */

} /* ion */
} /* js */

#endif /* jsion_ion_spewer_h__ */

