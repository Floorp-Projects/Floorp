/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
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
 * The Original Code is the Mozilla SpiderMonkey property tree implementation
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2002-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org>
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

#include <new>

#include "jstypes.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jspropertytree.h"
#include "jsscope.h"

#include "jsobjinlines.h"
#include "jsscopeinlines.h"

using namespace js;

inline HashNumber
ShapeHasher::hash(const Lookup l)
{
    return l->hash();
}

inline bool
ShapeHasher::match(const Key k, const Lookup l)
{
    return l->matches(k);
}

Shape *
PropertyTree::newShape(JSContext *cx)
{
    Shape *shape = js_NewGCShape(cx);
    if (!shape) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }
    JS_COMPARTMENT_METER(compartment->livePropTreeNodes++);
    JS_COMPARTMENT_METER(compartment->totalPropTreeNodes++);
    return shape;
}

static KidsHash *
HashChildren(Shape *kid1, Shape *kid2)
{
    KidsHash *hash = OffTheBooks::new_<KidsHash>();
    if (!hash || !hash->init(2)) {
        Foreground::delete_(hash);
        return NULL;
    }

    JS_ALWAYS_TRUE(hash->putNew(kid1));
    JS_ALWAYS_TRUE(hash->putNew(kid2));
    return hash;
}

bool
PropertyTree::insertChild(JSContext *cx, Shape *parent, Shape *child)
{
    JS_ASSERT(!parent->inDictionary());
    JS_ASSERT(!child->parent);
    JS_ASSERT(!child->inDictionary());
    JS_ASSERT(!JSID_IS_VOID(parent->id));
    JS_ASSERT(!JSID_IS_VOID(child->id));
    JS_ASSERT(cx->compartment == compartment);
    JS_ASSERT(child->compartment() == parent->compartment());

    KidsPointer *kidp = &parent->kids;

    if (kidp->isNull()) {
        child->setParent(parent);
        kidp->setShape(child);
        return true;
    }

    if (kidp->isShape()) {
        Shape *shape = kidp->toShape();
        JS_ASSERT(shape != child);
        JS_ASSERT(!shape->matches(child));

        KidsHash *hash = HashChildren(shape, child);
        if (!hash) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        kidp->setHash(hash);
        child->setParent(parent);
        return true;
    }

    if (!kidp->toHash()->putNew(child)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    child->setParent(parent);
    return true;
}

void
Shape::removeChild(Shape *child)
{
    JS_ASSERT(!child->inDictionary());
    JS_ASSERT(!JSID_IS_VOID(id));

    KidsPointer *kidp = &kids;
    if (kidp->isShape()) {
        JS_ASSERT(kidp->toShape() == child);
        kids.setNull();
        return;
    }

    kidp->toHash()->remove(child);
}

Shape *
PropertyTree::getChild(JSContext *cx, Shape *parent, const Shape &child)
{
    Shape *shape;

    JS_ASSERT(parent);
    JS_ASSERT(!JSID_IS_VOID(parent->id));

    /*
     * The property tree has extremely low fan-out below its root in
     * popular embeddings with real-world workloads. Patterns such as
     * defining closures that capture a constructor's environment as
     * getters or setters on the new object that is passed in as
     * |this| can significantly increase fan-out below the property
     * tree root -- see bug 335700 for details.
     */
    KidsPointer *kidp = &parent->kids;
    if (kidp->isShape()) {
        shape = kidp->toShape();
        if (shape->matches(&child))
            return shape;
    } else if (kidp->isHash()) {
        shape = *kidp->toHash()->lookup(&child);
        if (shape)
            return shape;
    } else {
        /* If kidp->isNull(), we always insert. */
    }

    shape = newShape(cx);
    if (!shape)
        return NULL;

    new (shape) Shape(child.id, child.rawGetter, child.rawSetter, child.slot, child.attrs,
                      child.flags, child.shortid, js_GenerateShape(cx));

    if (!insertChild(cx, parent, shape))
        return NULL;

    return shape;
}

#ifdef DEBUG

void
KidsPointer::checkConsistency(const Shape *aKid) const
{
    if (isShape()) {
        JS_ASSERT(toShape() == aKid);
    } else {
        JS_ASSERT(isHash());
        KidsHash *hash = toHash();
        KidsHash::Ptr ptr = hash->lookup(aKid);
        JS_ASSERT(*ptr == aKid);
    }
}

void
Shape::dump(JSContext *cx, FILE *fp) const
{
    JS_ASSERT(!JSID_IS_VOID(id));

    if (JSID_IS_INT(id)) {
        fprintf(fp, "[%ld]", (long) JSID_TO_INT(id));
    } else if (JSID_IS_DEFAULT_XML_NAMESPACE(id)) {
        fprintf(fp, "<default XML namespace>");
    } else {
        JSLinearString *str;
        if (JSID_IS_ATOM(id)) {
            str = JSID_TO_ATOM(id);
        } else {
            JS_ASSERT(JSID_IS_OBJECT(id));
            JSString *s = js_ValueToString(cx, IdToValue(id));
            fputs("object ", fp);
            str = s ? s->ensureLinear(cx) : NULL;
        }
        if (!str)
            fputs("<error>", fp);
        else
            FileEscapedString(fp, str, '"');
    }

    fprintf(fp, " g/s %p/%p slot %u attrs %x ",
            JS_FUNC_TO_DATA_PTR(void *, rawGetter),
            JS_FUNC_TO_DATA_PTR(void *, rawSetter),
            slot, attrs);
    if (attrs) {
        int first = 1;
        fputs("(", fp);
#define DUMP_ATTR(name, display) if (attrs & JSPROP_##name) fputs(" " #display + first, fp), first = 0
        DUMP_ATTR(ENUMERATE, enumerate);
        DUMP_ATTR(READONLY, readonly);
        DUMP_ATTR(PERMANENT, permanent);
        DUMP_ATTR(GETTER, getter);
        DUMP_ATTR(SETTER, setter);
        DUMP_ATTR(SHARED, shared);
#undef  DUMP_ATTR
        fputs(") ", fp);
    }

    fprintf(fp, "flags %x ", flags);
    if (flags) {
        int first = 1;
        fputs("(", fp);
#define DUMP_FLAG(name, display) if (flags & name) fputs(" " #display + first, fp), first = 0
        DUMP_FLAG(ALIAS, alias);
        DUMP_FLAG(HAS_SHORTID, has_shortid);
        DUMP_FLAG(METHOD, method);
        DUMP_FLAG(IN_DICTIONARY, in_dictionary);
#undef  DUMP_FLAG
        fputs(") ", fp);
    }

    fprintf(fp, "shortid %d\n", shortid);
}

static void
MeterKidCount(JSBasicStats *bs, uintN nkids)
{
    JS_BASIC_STATS_ACCUM(bs, nkids);
}

void
js::PropertyTree::meter(JSBasicStats *bs, Shape *node)
{
    uintN nkids = 0;
    const KidsPointer &kidp = node->kids;
    if (kidp.isShape()) {
        meter(bs, kidp.toShape());
        nkids = 1;
    } else if (kidp.isHash()) {
        const KidsHash &hash = *kidp.toHash();
        for (KidsHash::Range range = hash.all(); !range.empty(); range.popFront()) {
            Shape *kid = range.front();
            
            meter(bs, kid);
            nkids++;
        }
    }

    MeterKidCount(bs, nkids);
}

void
Shape::dumpSubtree(JSContext *cx, int level, FILE *fp) const
{
    if (!parent) {
        JS_ASSERT(level == 0);
        JS_ASSERT(JSID_IS_EMPTY(id));
        fprintf(fp, "class %s emptyShape %u\n", clasp->name, shape);
    } else {
        fprintf(fp, "%*sid ", level, "");
        dump(cx, fp);
    }

    if (!kids.isNull()) {
        ++level;
        if (kids.isShape()) {
            Shape *kid = kids.toShape();
            JS_ASSERT(kid->parent == this);
            kid->dumpSubtree(cx, level, fp);
        } else {
            const KidsHash &hash = *kids.toHash();
            for (KidsHash::Range range = hash.all(); !range.empty(); range.popFront()) {
                Shape *kid = range.front();

                JS_ASSERT(kid->parent == this);
                kid->dumpSubtree(cx, level, fp);
            }
        }
    }
}

#endif /* DEBUG */

void
Shape::finalize(JSContext *cx)
{
#ifdef DEBUG
    if ((flags & SHARED_EMPTY) && cx->runtime->meterEmptyShapes())
        compartment()->emptyShapes.remove((EmptyShape *)this);
#endif

    if (inDictionary()) {
        JS_COMPARTMENT_METER(compartment()->liveDictModeNodes--);
    } else {
        if (parent && parent->isMarked())
            parent->removeChild(this);

        if (kids.isHash())
            cx->delete_(kids.toHash());
    }

    freeTable(cx);
    JS_COMPARTMENT_METER(compartment()->livePropTreeNodes--);
}

void
js::PropertyTree::dumpShapeStats()
{
#ifdef DEBUG
    JSRuntime *rt = compartment->rt;

    JSBasicStats bs;
    static FILE *logfp;
    if (!logfp) {
        if (const char *filename = rt->propTreeStatFilename)
            logfp = fopen(filename, "w");
        if (!logfp)
            return;
    }

    JS_BASIC_STATS_INIT(&bs);

    uint32 empties;
    {
        typedef JSCompartment::EmptyShapeSet HS;

        HS &h = compartment->emptyShapes;
        empties = h.count();
        MeterKidCount(&bs, empties);
        for (HS::Range r = h.all(); !r.empty(); r.popFront())
            meter(&bs, r.front());
    }

    double props = rt->liveObjectPropsPreSweep;
    double nodes = compartment->livePropTreeNodes;
    double dicts = compartment->liveDictModeNodes;

    /* Empty scope nodes are never hashed, so subtract them from nodes. */
    JS_ASSERT(nodes - dicts == bs.sum);
    nodes -= empties;

    double sigma;
    double mean = JS_MeanAndStdDevBS(&bs, &sigma);

    fprintf(logfp,
            "props %g nodes %g (dicts %g) beta %g meankids %g sigma %g max %u\n",
            props, nodes, dicts, nodes / props, mean, sigma, bs.max);

    JS_DumpHistogram(&bs, logfp);

    /* This data is global, so only print it once per GC. */
    if (compartment == rt->atomsCompartment) {
        fprintf(logfp,
                "\nProperty tree stats for gcNumber %lu\n",
                (unsigned long) rt->gcNumber);

#define RATE(f1, f2) (((double)js_scope_stats.f1 / js_scope_stats.f2) * 100.0)

        fprintf(logfp,
                "Scope search stats:\n"
                "  searches:        %6u\n"
                "  hits:            %6u %5.2f%% of searches\n"
                "  misses:          %6u %5.2f%%\n"
                "  hashes:          %6u %5.2f%%\n"
                "  hashHits:        %6u %5.2f%% (%5.2f%% of hashes)\n"
                "  hashMisses:      %6u %5.2f%% (%5.2f%%)\n"
                "  steps:           %6u %5.2f%% (%5.2f%%)\n"
                "  stepHits:        %6u %5.2f%% (%5.2f%%)\n"
                "  stepMisses:      %6u %5.2f%% (%5.2f%%)\n"
                "  initSearches:    %6u\n"
                "  changeSearches:  %6u\n"
                "  tableAllocFails: %6u\n"
                "  toDictFails:     %6u\n"
                "  wrapWatchFails:  %6u\n"
                "  adds:            %6u\n"
                "  addFails:        %6u\n"
                "  puts:            %6u\n"
                "  redundantPuts:   %6u\n"
                "  putFails:        %6u\n"
                "  changes:         %6u\n"
                "  changeFails:     %6u\n"
                "  compresses:      %6u\n"
                "  grows:           %6u\n"
                "  removes:         %6u\n"
                "  removeFrees:     %6u\n"
                "  uselessRemoves:  %6u\n"
                "  shrinks:         %6u\n",
                js_scope_stats.searches,
                js_scope_stats.hits, RATE(hits, searches),
                js_scope_stats.misses, RATE(misses, searches),
                js_scope_stats.hashes, RATE(hashes, searches),
                js_scope_stats.hashHits, RATE(hashHits, searches), RATE(hashHits, hashes),
                js_scope_stats.hashMisses, RATE(hashMisses, searches), RATE(hashMisses, hashes),
                js_scope_stats.steps, RATE(steps, searches), RATE(steps, hashes),
                js_scope_stats.stepHits, RATE(stepHits, searches), RATE(stepHits, hashes),
                js_scope_stats.stepMisses, RATE(stepMisses, searches), RATE(stepMisses, hashes),
                js_scope_stats.initSearches,
                js_scope_stats.changeSearches,
                js_scope_stats.tableAllocFails,
                js_scope_stats.toDictFails,
                js_scope_stats.wrapWatchFails,
                js_scope_stats.adds,
                js_scope_stats.addFails,
                js_scope_stats.puts,
                js_scope_stats.redundantPuts,
                js_scope_stats.putFails,
                js_scope_stats.changes,
                js_scope_stats.changeFails,
                js_scope_stats.compresses,
                js_scope_stats.grows,
                js_scope_stats.removes,
                js_scope_stats.removeFrees,
                js_scope_stats.uselessRemoves,
                js_scope_stats.shrinks);
    }

#undef RATE

    fflush(logfp);
#endif /* DEBUG */
}

void
js::PropertyTree::dumpShapes(JSContext *cx)
{
#ifdef DEBUG
    JSRuntime *rt = cx->runtime;

    if (const char *filename = rt->propTreeDumpFilename) {
        char pathname[1024];
        JS_snprintf(pathname, sizeof pathname, "%s.%lu",
                    filename, (unsigned long)rt->gcNumber);
        FILE *dumpfp = fopen(pathname, "w");
        if (dumpfp) {
            typedef JSCompartment::EmptyShapeSet HS;

            for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c) {
                if (rt->gcCurrentCompartment != NULL && rt->gcCurrentCompartment != *c)
                    continue;

                fprintf(dumpfp, "*** Compartment %p ***\n", (void *)*c);

                HS &h = (*c)->emptyShapes;
                for (HS::Range r = h.all(); !r.empty(); r.popFront()) {
                    Shape *empty = r.front();
                    empty->dumpSubtree(cx, 0, dumpfp);
                    putc('\n', dumpfp);
                }
            }

            fclose(dumpfp);
        }
    }
#endif
}
