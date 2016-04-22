/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/UbiNodeShortestPaths.h"

#include "mozilla/Maybe.h"
#include "mozilla/Move.h"

#include "jsstr.h"

namespace JS {
namespace ubi {

JS_PUBLIC_API(BackEdge::Ptr)
BackEdge::clone() const
{
    BackEdge::Ptr clone(js_new<BackEdge>());
    if (!clone)
        return nullptr;

    clone->predecessor_ = predecessor();
    if (name()) {
        clone->name_ = js::DuplicateString(name().get());
        if (!clone->name_)
            return nullptr;
    }
    return mozilla::Move(clone);
}

#ifdef DEBUG
JS_PUBLIC_API(void)
dumpPaths(JSRuntime* rt, Node node, uint32_t maxNumPaths /* = 10 */)
{
    mozilla::Maybe<AutoCheckCannotGC> nogc;

    JS::ubi::RootList rootList(rt, nogc);
    MOZ_ASSERT(rootList.init());
    rootList.wantNames = true;

    NodeSet targets;
    bool ok = targets.init() && targets.putNew(node);
    MOZ_ASSERT(ok);

    auto paths = ShortestPaths::Create(rt, nogc.ref(), maxNumPaths, &rootList, mozilla::Move(targets));
    MOZ_ASSERT(paths.isSome());

    int i = 0;
    ok = paths->forEachPath(node, [&](Path& path) {
        fprintf(stderr, "Path %d:\n", i++);
        for (auto backEdge : path) {
            fprintf(stderr, "    %p ", (void*) backEdge->predecessor().identifier());

            js_fputs(backEdge->predecessor().typeName(), stderr);
            fprintf(stderr, "\n");

            fprintf(stderr, "            |\n");
            fprintf(stderr, "            |\n");
            fprintf(stderr, "        '");

            const char16_t* name = backEdge->name().get();
            if (!name)
                name = (const char16_t*) MOZ_UTF16("<no edge name>");
            js_fputs(name, stderr);
            fprintf(stderr, "'\n");

            fprintf(stderr, "            |\n");
            fprintf(stderr, "            V\n");
        }

        fprintf(stderr, "    %p ", (void*) node.identifier());
        js_fputs(node.typeName(), stderr);
        fprintf(stderr, "\n\n");

        return true;
    });
    MOZ_ASSERT(ok);

    if (i == 0)
        fprintf(stderr, "No retaining paths found.\n");
}
#endif

} // namespace ubi
} // namespace JS
