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
 *   Andrew Drake <drakedevel@gmail.com>
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

#include "Ion.h"
#include "IonAnalysis.h"
#include "IonBuilder.h"
#include "IonSpew.h"
#include "IonLIR.h"

#if defined(JS_CPU_X86)
# include "x86/Lowering-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Lowering-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Lowering-arm.h"
#endif

using namespace js;
using namespace js::ion;

#ifdef JS_THREADSAFE
static bool IonTLSInitialized = false;
static PRUintn IonTLSIndex;
#else
static IonContext *GlobalIonContext;
#endif

IonContext::IonContext(JSContext *cx, TempAllocator *temp)
  : cx(cx),
    temp(temp)
{
    SetIonContext(this);
}

IonContext::~IonContext()
{
    SetIonContext(NULL);
}

bool ion::InitializeIon()
{
#ifdef JS_THREADSAFE
    if (!IonTLSInitialized) {
        PRStatus status = PR_NewThreadPrivateIndex(&IonTLSIndex, NULL);
        if (status != PR_SUCCESS)
            return false;
        IonTLSInitialized = true;
    }
#endif
    IonBuilder::SetupOpcodeFlags();
    return true;
}

#ifdef JS_THREADSAFE
IonContext *ion::GetIonContext()
{
    return (IonContext *)PR_GetThreadPrivate(IonTLSIndex);
}

bool ion::SetIonContext(IonContext *ctx)
{
    return PR_SetThreadPrivate(IonTLSIndex, ctx) == PR_SUCCESS;
}
#else
IonContext *ion::GetIonContext()
{
    JS_ASSERT(GlobalIonContext);
    return GlobalIonContext;
}

bool ion::SetIonContext(IonContext *ctx)
{
    GlobalIonContext = ctx;
    return true;
}
#endif

bool
ion::Go(JSContext *cx, JSScript *script, StackFrame *fp)
{
    TempAllocator temp(&cx->tempPool);
    IonContext ictx(cx, &temp);

    JSFunction *fun = fp->isFunctionFrame() ? fp->fun() : NULL;

    MIRGraph graph;

    DummyOracle oracle;
    C1Spewer spew(graph, script);
    IonBuilder builder(cx, script, fun, temp, graph, &oracle);
    spew.enable("/tmp/ion.cfg");

    if (!builder.build())
        return false;
    spew.spew("Build SSA");

    if (!ReorderBlocks(graph))
        return false;
    spew.spew("Reorder Blocks");

    if (!BuildPhiReverseMapping(graph))
        return false;
    // No spew, graph not changed.

    if (!ApplyTypeInformation(graph))
        return false;
    spew.spew("Apply types");

    LIRBuilder lirgen(&builder, graph);
    if (!lirgen.generate())
        return false;
    spew.spew("Generate LIR");

    return false;
}

