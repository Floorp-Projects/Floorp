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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nicolas B. Pierron <nicolas.b.pierron@mozilla.com>
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

#ifndef jscache_h___
#define jscache_h___

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "gc/Marking.h"

namespace js {

// A WeakCache is used to map a key to a value similar to an HashMap except
// that its entries are garbage collected.  An entry is kept as long as
// both the key and value are marked.
//
// No mark function is provided with this weak container.  However, this weak
// container should take part in the sweep phase.
template <class Key, class Value,
          class HashPolicy = DefaultHasher<Key>,
          class AllocPolicy = RuntimeAllocPolicy>
class WeakCache : public HashMap<Key, Value, HashPolicy, AllocPolicy> {
  private:
    typedef HashMap<Key, Value, HashPolicy, AllocPolicy> Base;
    typedef typename Base::Range Range;
    typedef typename Base::Enum Enum;

  public:
    explicit WeakCache(JSRuntime *rt) : Base(rt) { }
    explicit WeakCache(JSContext *cx) : Base(cx) { }

  public:
    // Sweep all entries which have unmarked key or value.
    void sweep(FreeOp *fop) {
        // Remove all entries whose keys/values remain unmarked.
        for (Enum e(*this); !e.empty(); e.popFront()) {
            if (!gc::IsMarked(e.front().key) || !gc::IsMarked(e.front().value))
                e.removeFront();
        }

#if DEBUG
        // Once we've swept, all remaining edges should stay within the
        // known-live part of the graph.
        for (Range r = Base::all(); !r.empty(); r.popFront()) {
            JS_ASSERT(gc::IsMarked(r.front().key));
            JS_ASSERT(gc::IsMarked(r.front().value));
        }
#endif
    }
};

}

#endif
