/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jason Orendorff <jorendorff@mozilla.com>
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

#ifndef MapObject_h__
#define MapObject_h__

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "js/HashTable.h"

namespace js {

/*
 * Comparing two ropes for equality can fail. The js::HashTable template
 * requires infallible hash() and match() operations. Therefore we require
 * all values to be converted to hashable form before being used as a key
 * in a Map or Set object.
 *
 * All values except ropes are hashable as-is.
 */
class HashableValue {
    HeapValue value;

  public:
    struct Hasher {
        typedef HashableValue Lookup;
        static HashNumber hash(const Lookup &v) { return v.hash(); }
        static bool match(const HashableValue &k, const Lookup &l) { return k.equals(l); }
    };

    HashableValue() : value(UndefinedValue()) {}

    operator const HeapValue &() const { return value; }
    bool setValue(JSContext *cx, const Value &v);
    HashNumber hash() const;
    bool equals(const HashableValue &other) const;
};

typedef HashMap<HashableValue, HeapValue, HashableValue::Hasher, RuntimeAllocPolicy> ValueMap;
typedef HashSet<HashableValue, HashableValue::Hasher, RuntimeAllocPolicy> ValueSet;

class MapObject : public JSObject {
  public:
    static JSObject *initClass(JSContext *cx, JSObject *obj);
    static Class class_;
  private:
    typedef ValueMap Data;
    static JSFunctionSpec methods[];
    ValueMap *getData() { return static_cast<ValueMap *>(getPrivate()); }
    static void mark(JSTracer *trc, JSObject *obj);
    static void finalize(FreeOp *fop, JSObject *obj);
    static JSBool construct(JSContext *cx, unsigned argc, Value *vp);
    static JSBool size(JSContext *cx, unsigned argc, Value *vp);
    static JSBool get(JSContext *cx, unsigned argc, Value *vp);
    static JSBool has(JSContext *cx, unsigned argc, Value *vp);
    static JSBool set(JSContext *cx, unsigned argc, Value *vp);
    static JSBool delete_(JSContext *cx, unsigned argc, Value *vp);
};

class SetObject : public JSObject {
  public:
    static JSObject *initClass(JSContext *cx, JSObject *obj);
    static Class class_;
  private:
    typedef ValueSet Data;
    static JSFunctionSpec methods[];
    ValueSet *getData() { return static_cast<ValueSet *>(getPrivate()); }
    static void mark(JSTracer *trc, JSObject *obj);
    static void finalize(FreeOp *fop, JSObject *obj);
    static JSBool construct(JSContext *cx, unsigned argc, Value *vp);
    static JSBool size(JSContext *cx, unsigned argc, Value *vp);
    static JSBool has(JSContext *cx, unsigned argc, Value *vp);
    static JSBool add(JSContext *cx, unsigned argc, Value *vp);
    static JSBool delete_(JSContext *cx, unsigned argc, Value *vp);
};

} /* namespace js */

extern JSObject *
js_InitMapClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitSetClass(JSContext *cx, JSObject *obj);

#endif  /* MapObject_h__ */
