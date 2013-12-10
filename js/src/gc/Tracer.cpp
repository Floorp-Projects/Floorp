/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Tracer.h"

#include "jsapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsprf.h"
#include "jsscript.h"
#include "NamespaceImports.h"

#include "gc/Marking.h"

using namespace js;
using namespace js::gc;

JS_PUBLIC_API(void)
JS_CallValueTracer(JSTracer *trc, Value *valuep, const char *name)
{
    MarkValueUnbarriered(trc, valuep, name);
}

JS_PUBLIC_API(void)
JS_CallIdTracer(JSTracer *trc, jsid *idp, const char *name)
{
    MarkIdUnbarriered(trc, idp, name);
}

JS_PUBLIC_API(void)
JS_CallObjectTracer(JSTracer *trc, JSObject **objp, const char *name)
{
    MarkObjectUnbarriered(trc, objp, name);
}

JS_PUBLIC_API(void)
JS_CallStringTracer(JSTracer *trc, JSString **strp, const char *name)
{
    MarkStringUnbarriered(trc, strp, name);
}

JS_PUBLIC_API(void)
JS_CallScriptTracer(JSTracer *trc, JSScript **scriptp, const char *name)
{
    MarkScriptUnbarriered(trc, scriptp, name);
}

JS_PUBLIC_API(void)
JS_CallHeapValueTracer(JSTracer *trc, JS::Heap<JS::Value> *valuep, const char *name)
{
    MarkValueUnbarriered(trc, valuep->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapIdTracer(JSTracer *trc, JS::Heap<jsid> *idp, const char *name)
{
    MarkIdUnbarriered(trc, idp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapObjectTracer(JSTracer *trc, JS::Heap<JSObject *> *objp, const char *name)
{
    MarkObjectUnbarriered(trc, objp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapStringTracer(JSTracer *trc, JS::Heap<JSString *> *strp, const char *name)
{
    MarkStringUnbarriered(trc, strp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallHeapScriptTracer(JSTracer *trc, JS::Heap<JSScript *> *scriptp, const char *name)
{
    MarkScriptUnbarriered(trc, scriptp->unsafeGet(), name);
}

JS_PUBLIC_API(void)
JS_CallTenuredObjectTracer(JSTracer *trc, JS::TenuredHeap<JSObject *> *objp, const char *name)
{
    JSObject *obj = objp->getPtr();
    if (!obj)
        return;

    JS_SET_TRACING_LOCATION(trc, (void*)objp);
    MarkObjectUnbarriered(trc, &obj, name);

    objp->setPtr(obj);
}

JS_PUBLIC_API(void)
JS_TracerInit(JSTracer *trc, JSRuntime *rt, JSTraceCallback callback)
{
    InitTracer(trc, rt, callback);
}

JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    js::TraceChildren(trc, thing, kind);
}

JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer *trc)
{
    AssertHeapIsIdle(trc->runtime);
    TraceRuntime(trc);
}

static size_t
CountDecimalDigits(size_t num)
{
    size_t numDigits = 0;
    do {
        num /= 10;
        numDigits++;
    } while (num > 0);

    return numDigits;
}

JS_PUBLIC_API(void)
JS_GetTraceThingInfo(char *buf, size_t bufsize, JSTracer *trc, void *thing,
                     JSGCTraceKind kind, bool details)
{
    const char *name = nullptr; /* silence uninitialized warning */
    size_t n;

    if (bufsize == 0)
        return;

    switch (kind) {
      case JSTRACE_OBJECT:
      {
        name = static_cast<JSObject *>(thing)->getClass()->name;
        break;
      }

      case JSTRACE_STRING:
        name = ((JSString *)thing)->isDependent()
               ? "substring"
               : "string";
        break;

      case JSTRACE_SCRIPT:
        name = "script";
        break;

      case JSTRACE_LAZY_SCRIPT:
        name = "lazyscript";
        break;

      case JSTRACE_IONCODE:
        name = "ioncode";
        break;

      case JSTRACE_SHAPE:
        name = "shape";
        break;

      case JSTRACE_BASE_SHAPE:
        name = "base_shape";
        break;

      case JSTRACE_TYPE_OBJECT:
        name = "type_object";
        break;
    }

    n = strlen(name);
    if (n > bufsize - 1)
        n = bufsize - 1;
    js_memcpy(buf, name, n + 1);
    buf += n;
    bufsize -= n;
    *buf = '\0';

    if (details && bufsize > 2) {
        switch (kind) {
          case JSTRACE_OBJECT:
          {
            JSObject *obj = (JSObject *)thing;
            if (obj->is<JSFunction>()) {
                JSFunction *fun = &obj->as<JSFunction>();
                if (fun->displayAtom()) {
                    *buf++ = ' ';
                    bufsize--;
                    PutEscapedString(buf, bufsize, fun->displayAtom(), 0);
                }
            } else if (obj->getClass()->flags & JSCLASS_HAS_PRIVATE) {
                JS_snprintf(buf, bufsize, " %p", obj->getPrivate());
            } else {
                JS_snprintf(buf, bufsize, " <no private>");
            }
            break;
          }

          case JSTRACE_STRING:
          {
            *buf++ = ' ';
            bufsize--;
            JSString *str = (JSString *)thing;

            if (str->isLinear()) {
                bool willFit = str->length() + strlen("<length > ") +
                               CountDecimalDigits(str->length()) < bufsize;

                n = JS_snprintf(buf, bufsize, "<length %d%s> ",
                                (int)str->length(),
                                willFit ? "" : " (truncated)");
                buf += n;
                bufsize -= n;

                PutEscapedString(buf, bufsize, &str->asLinear(), 0);
            }
            else
                JS_snprintf(buf, bufsize, "<rope: length %d>", (int)str->length());
            break;
          }

          case JSTRACE_SCRIPT:
          {
            JSScript *script = static_cast<JSScript *>(thing);
            JS_snprintf(buf, bufsize, " %s:%u", script->filename(), unsigned(script->lineno()));
            break;
          }

          case JSTRACE_LAZY_SCRIPT:
          case JSTRACE_IONCODE:
          case JSTRACE_SHAPE:
          case JSTRACE_BASE_SHAPE:
          case JSTRACE_TYPE_OBJECT:
            break;
        }
    }
    buf[bufsize - 1] = '\0';
}

extern JS_PUBLIC_API(const char *)
JS_GetTraceEdgeName(JSTracer *trc, char *buffer, int bufferSize)
{
    if (trc->debugPrinter) {
        trc->debugPrinter(trc, buffer, bufferSize);
        return buffer;
    }
    if (trc->debugPrintIndex != (size_t) - 1) {
        JS_snprintf(buffer, bufferSize, "%s[%lu]",
                    (const char *)trc->debugPrintArg,
                    trc->debugPrintIndex);
        return buffer;
    }
    return (const char*)trc->debugPrintArg;
}


