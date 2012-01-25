/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is about:memory glue.
 *
 * The Initial Developer of the Original Code is
 * Ms2ger <ms2ger@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "js/MemoryMetrics.h"

#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"

#include "jsobjinlines.h"

#ifdef JS_THREADSAFE

namespace JS {

using namespace js;

static void
CompartmentMemoryCallback(JSContext *cx, void *vdata, JSCompartment *compartment)
{
    // Append a new CompartmentStats to the vector.
    IterateData *data = static_cast<IterateData *>(vdata);

    // CollectCompartmentStatsForRuntime reserves enough space.
    MOZ_ALWAYS_TRUE(data->compartmentStatsVector.growBy(1));
    CompartmentStats &curr = data->compartmentStatsVector.back();
    curr.init(data->getNameCb(cx, compartment), data->destroyNameCb);
    data->currCompartmentStats = &curr;

    // Get the compartment-level numbers.
#ifdef JS_METHODJIT
    curr.mjitCode = compartment->sizeOfMjitCode();
#endif
    SizeOfCompartmentTypeInferenceData(cx, compartment,
                                       &curr.typeInferenceMemory,
                                       data->mallocSizeOf);
    curr.shapesCompartmentTables =
        SizeOfCompartmentShapeTable(compartment, data->mallocSizeOf);
}

static void
ExplicitNonHeapCompartmentCallback(JSContext *cx, void *data, JSCompartment *compartment)
{
#ifdef JS_METHODJIT
    size_t *n = static_cast<size_t *>(data);
    *n += compartment->sizeOfMjitCode();
#endif
}

static void
ChunkCallback(JSContext *cx, void *vdata, gc::Chunk *chunk)
{
    // Nb: This function is only called for dirty chunks, which is why we
    // increment gcHeapChunkDirtyDecommitted.
    IterateData *data = static_cast<IterateData *>(vdata);
    for (size_t i = 0; i < gc::ArenasPerChunk; i++)
        if (chunk->decommittedArenas.get(i))
            data->gcHeapChunkDirtyDecommitted += gc::ArenaSize;
}

static void
ArenaCallback(JSContext *cx, void *vdata, gc::Arena *arena,
              JSGCTraceKind traceKind, size_t thingSize)
{
    IterateData *data = static_cast<IterateData *>(vdata);

    data->currCompartmentStats->gcHeapArenaHeaders +=
        sizeof(gc::ArenaHeader);
    size_t allocationSpace = arena->thingsSpan(thingSize);
    data->currCompartmentStats->gcHeapArenaPadding +=
        gc::ArenaSize - allocationSpace - sizeof(gc::ArenaHeader);
    // We don't call the callback on unused things.  So we compute the
    // unused space like this:  arenaUnused = maxArenaUnused - arenaUsed.
    // We do this by setting arenaUnused to maxArenaUnused here, and then
    // subtracting thingSize for every used cell, in CellCallback().
    data->currCompartmentStats->gcHeapArenaUnused += allocationSpace;
}

static void
CellCallback(JSContext *cx, void *vdata, void *thing, JSGCTraceKind traceKind,
             size_t thingSize)
{
    IterateData *data = static_cast<IterateData *>(vdata);
    CompartmentStats *curr = data->currCompartmentStats;
    switch (traceKind) {
    case JSTRACE_OBJECT:
    {
        JSObject *obj = static_cast<JSObject *>(thing);
        if (obj->isFunction()) {
            curr->gcHeapObjectsFunction += thingSize;
        } else {
            curr->gcHeapObjectsNonFunction += thingSize;
        }
        size_t slotsSize, elementsSize;
        obj->sizeOfExcludingThis(data->mallocSizeOf, &slotsSize, &elementsSize);
        curr->objectSlots += slotsSize;
        curr->objectElements += elementsSize;
        break;
    }
    case JSTRACE_STRING:
    {
        JSString *str = static_cast<JSString *>(thing);
        curr->gcHeapStrings += thingSize;
        curr->stringChars += str->sizeOfExcludingThis(data->mallocSizeOf);
        break;
    }
    case JSTRACE_SHAPE:
    {
        Shape *shape = static_cast<Shape*>(thing);
        size_t propTableSize, kidsSize;
        shape->sizeOfExcludingThis(data->mallocSizeOf, &propTableSize, &kidsSize);
        if (shape->inDictionary()) {
            curr->gcHeapShapesDict += thingSize;
            curr->shapesExtraDictTables += propTableSize;
            JS_ASSERT(kidsSize == 0);
        } else {
            curr->gcHeapShapesTree += thingSize;
            curr->shapesExtraTreeTables += propTableSize;
            curr->shapesExtraTreeShapeKids += kidsSize;
        }
        break;
    }
    case JSTRACE_BASE_SHAPE:
    {
        curr->gcHeapShapesBase += thingSize;
        break;
    }
    case JSTRACE_SCRIPT:
    {
        JSScript *script = static_cast<JSScript *>(thing);
        curr->gcHeapScripts += thingSize;
        curr->scriptData += script->sizeOfData(data->mallocSizeOf);
#ifdef JS_METHODJIT
        curr->mjitData += script->sizeOfJitScripts(data->mallocSizeOf);
#endif
        break;
    }
    case JSTRACE_TYPE_OBJECT:
    {
        types::TypeObject *obj = static_cast<types::TypeObject *>(thing);
        curr->gcHeapTypeObjects += thingSize;
        SizeOfTypeObjectExcludingThis(obj, &curr->typeInferenceMemory,
                                      data->mallocSizeOf);
        break;
    }
    case JSTRACE_XML:
    {
        curr->gcHeapXML += thingSize;
        break;
    }
    }
    // Yes, this is a subtraction:  see ArenaCallback() for details.
    curr->gcHeapArenaUnused -= thingSize;
}

JS_PUBLIC_API(bool)
CollectCompartmentStatsForRuntime(JSRuntime *rt, IterateData *data)
{
    JSContext *cx = JS_NewContext(rt, 0);
    if (!cx)
        return false;

    {
        JSAutoRequest ar(cx);

        if (!data->compartmentStatsVector.reserve(rt->compartments.length()))
            return false;

        data->gcHeapChunkCleanDecommitted =
            rt->gcChunkPool.countCleanDecommittedArenas(rt) *
            gc::ArenaSize;
        data->gcHeapChunkCleanUnused =
            int64_t(JS_GetGCParameter(rt, JSGC_UNUSED_CHUNKS)) *
            gc::ChunkSize -
            data->gcHeapChunkCleanDecommitted;
        data->gcHeapChunkTotal =
            int64_t(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) *
            gc::ChunkSize;

        IterateCompartmentsArenasCells(cx, data, CompartmentMemoryCallback,
                                       ArenaCallback, CellCallback);
        IterateChunks(cx, data, ChunkCallback);

        data->runtimeObject = data->mallocSizeOf(rt);

        size_t normal, temporary, regexpCode, stackCommitted;
        rt->sizeOfExcludingThis(data->mallocSizeOf,
                                &normal,
                                &temporary,
                                &regexpCode,
                                &stackCommitted);

        data->runtimeNormal = normal;
        data->runtimeTemporary = temporary;
        data->runtimeRegexpCode = regexpCode;
        data->runtimeStackCommitted = stackCommitted;

        // Nb: we use sizeOfExcludingThis() because atomState.atoms is within
        // JSRuntime, and so counted when JSRuntime is counted.
        data->runtimeAtomsTable =
            rt->atomState.atoms.sizeOfExcludingThis(data->mallocSizeOf);

        JSContext *acx, *iter = NULL;
        while ((acx = JS_ContextIteratorUnlocked(rt, &iter)) != NULL)
            data->runtimeContexts += acx->sizeOfIncludingThis(data->mallocSizeOf);
    }

    JS_DestroyContextNoGC(cx);

    // This is initialized to all bytes stored in used chunks, and then we
    // subtract used space from it each time around the loop.
    data->gcHeapChunkDirtyUnused = data->gcHeapChunkTotal -
                                   data->gcHeapChunkCleanUnused -
                                   data->gcHeapChunkCleanDecommitted -
                                   data->gcHeapChunkDirtyDecommitted;

    for (size_t index = 0;
         index < data->compartmentStatsVector.length();
         index++) {
        CompartmentStats &stats = data->compartmentStatsVector[index];

        int64_t used = stats.gcHeapArenaHeaders +
                       stats.gcHeapArenaPadding +
                       stats.gcHeapArenaUnused +
                       stats.gcHeapObjectsNonFunction +
                       stats.gcHeapObjectsFunction +
                       stats.gcHeapStrings +
                       stats.gcHeapShapesTree +
                       stats.gcHeapShapesDict +
                       stats.gcHeapShapesBase +
                       stats.gcHeapScripts +
                       stats.gcHeapTypeObjects +
                       stats.gcHeapXML;

        data->gcHeapChunkDirtyUnused -= used;
        data->gcHeapArenaUnused += stats.gcHeapArenaUnused;
        data->totalObjects += stats.gcHeapObjectsNonFunction +
                              stats.gcHeapObjectsFunction +
                              stats.objectSlots +
                              stats.objectElements;
        data->totalShapes  += stats.gcHeapShapesTree +
                              stats.gcHeapShapesDict +
                              stats.gcHeapShapesBase +
                              stats.shapesExtraTreeTables +
                              stats.shapesExtraDictTables +
                              stats.shapesCompartmentTables;
        data->totalScripts += stats.gcHeapScripts +
                              stats.scriptData;
        data->totalStrings += stats.gcHeapStrings +
                              stats.stringChars;
#ifdef JS_METHODJIT
        data->totalMjit    += stats.mjitCode +
                              stats.mjitData;
#endif
        data->totalTypeInference += stats.gcHeapTypeObjects +
                                    stats.typeInferenceMemory.objects +
                                    stats.typeInferenceMemory.scripts +
                                    stats.typeInferenceMemory.tables;
        data->totalAnalysisTemp  += stats.typeInferenceMemory.temporary;
    }

    size_t numDirtyChunks = (data->gcHeapChunkTotal -
                             data->gcHeapChunkCleanUnused) /
                            gc::ChunkSize;
    int64_t perChunkAdmin =
        sizeof(gc::Chunk) - (sizeof(gc::Arena) * gc::ArenasPerChunk);
    data->gcHeapChunkAdmin = numDirtyChunks * perChunkAdmin;
    data->gcHeapChunkDirtyUnused -= data->gcHeapChunkAdmin;

    // Why 10000x?  100x because it's a percentage, and another 100x
    // because nsIMemoryReporter requires that for percentage amounts so
    // they can be fractional.
    data->gcHeapUnusedPercentage = (data->gcHeapChunkCleanUnused +
                                    data->gcHeapChunkDirtyUnused +
                                    data->gcHeapChunkCleanDecommitted +
                                    data->gcHeapChunkDirtyDecommitted +
                                    data->gcHeapArenaUnused) * 10000 /
                                   data->gcHeapChunkTotal;

    return true;
}

JS_PUBLIC_API(bool)
GetExplicitNonHeapForRuntime(JSRuntime *rt, int64_t *amount,
                             JSMallocSizeOfFun mallocSizeOf)
{
    JSContext *cx = JS_NewContext(rt, 0);
    if (!cx)
        return false;

    // explicit/<compartment>/gc-heap/*
    *amount = int64_t(JS_GetGCParameter(rt, JSGC_TOTAL_CHUNKS)) *
              gc::ChunkSize;

    {
        JSAutoRequest ar(cx);

        // explicit/<compartment>/mjit-code
        size_t n = 0;
        IterateCompartments(cx, &n, ExplicitNonHeapCompartmentCallback);
        *amount += n;

        // explicit/runtime/regexp-code
        // explicit/runtime/stack-committed
        size_t regexpCode, stackCommitted;
        rt->sizeOfExcludingThis(mallocSizeOf,
                                NULL,
                                NULL,
                                &regexpCode,
                                &stackCommitted);

        *amount += regexpCode;
        *amount += stackCommitted;
    }

    JS_DestroyContextNoGC(cx);

    return true;
}

} // namespace JS

#endif // JS_THREADSAFE
