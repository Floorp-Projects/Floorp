/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu> (original author)
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

#ifndef jscntxtinlines_h___
#define jscntxtinlines_h___

#include "jscntxt.h"
#include "jsxml.h"

#include "jsobjinlines.h"

namespace js {

void
AutoIdArray::trace(JSTracer *trc) {
    JS_ASSERT(tag == IDARRAY);
    js::TraceValues(trc, idArray->length, idArray->vector, "JSAutoIdArray.idArray");
}

class AutoNamespaces : protected AutoGCRooter {
  protected:
    AutoNamespaces(JSContext *cx) : AutoGCRooter(cx, NAMESPACES) {
    }

    friend void AutoGCRooter::trace(JSTracer *trc);

  public:
    JSXMLArray array;
};

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        JS_SET_TRACING_NAME(trc, "js::AutoValueRooter.val");
        js_CallValueTracerIfGCThing(trc, static_cast<AutoValueRooter *>(this)->val);
        return;

      case SPROP:
        static_cast<AutoScopePropertyRooter *>(this)->sprop->trace(trc);
        return;

      case WEAKROOTS:
        static_cast<AutoSaveWeakRoots *>(this)->savedRoots.mark(trc);
        return;

      case COMPILER:
        static_cast<JSCompiler *>(this)->trace(trc);
        return;

      case SCRIPT:
        if (JSScript *script = static_cast<AutoScriptRooter *>(this)->script)
            js_TraceScript(trc, script);
        return;

      case ENUMERATOR:
        static_cast<AutoEnumStateRooter *>(this)->trace(trc);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        TraceValues(trc, ida->length, ida->vector, "js::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropertyDescriptorArray &descriptors =
            static_cast<AutoDescriptorArray *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropertyDescriptor &desc = descriptors[i];

            JS_CALL_VALUE_TRACER(trc, desc.value, "PropertyDescriptor::value");
            JS_CALL_VALUE_TRACER(trc, desc.get, "PropertyDescriptor::get");
            JS_CALL_VALUE_TRACER(trc, desc.set, "PropertyDescriptor::set");
            js_TraceId(trc, desc.id);
        }
        return;
      }

      case NAMESPACES: {
        JSXMLArray &array = static_cast<AutoNamespaces *>(this)->array;
        TraceObjectVector(trc, reinterpret_cast<JSObject **>(array.vector), array.length);
        array.cursors->trace(trc);
        return;
      }

      case XML:
        js_TraceXML(trc, static_cast<AutoXMLRooter *>(this)->xml);
        return;

      case OBJECT:
        if (JSObject *obj = static_cast<AutoObjectRooter *>(this)->obj) {
            JS_SET_TRACING_NAME(trc, "js::AutoObjectRooter.obj");
            js_CallGCMarker(trc, obj, JSTRACE_OBJECT);
        }
        return;

      case ID:
        JS_SET_TRACING_NAME(trc, "js::AutoIdRooter.val");
        js_CallValueTracerIfGCThing(trc, static_cast<AutoIdRooter *>(this)->idval);
        return;

      case VECTOR: {
        js::Vector<jsval, 8> &vector = static_cast<js::AutoValueVector *>(this)->vector;
        js::TraceValues(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    TraceValues(trc, tag, static_cast<AutoArrayRooter *>(this)->array, "js::AutoArrayRooter.array");
}

}

#endif /* jscntxtinlines_h___ */
