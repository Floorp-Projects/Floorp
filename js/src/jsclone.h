/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is JavaScript structured data serialization.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
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

#ifndef jsclone_h___
#define jsclone_h___

#include "jsapi.h"
#include "jscntxt.h"

#include "js/HashTable.h"
#include "js/Vector.h"

namespace js {

bool
WriteStructuredClone(JSContext *cx, const Value &v, uint64_t **bufp, size_t *nbytesp,
                     const JSStructuredCloneCallbacks *cb, void *cbClosure);

bool
ReadStructuredClone(JSContext *cx, const uint64_t *data, size_t nbytes, Value *vp,
                    const JSStructuredCloneCallbacks *cb, void *cbClosure);

struct SCOutput {
  public:
    explicit SCOutput(JSContext *cx);

    JSContext *context() const { return cx; }

    bool write(uint64_t u);
    bool writePair(uint32_t tag, uint32_t data);
    bool writeDouble(double d);
    bool writeBytes(const void *p, size_t nbytes);
    bool writeChars(const jschar *p, size_t nchars);

    template <class T>
    bool writeArray(const T *p, size_t nbytes);

    bool extractBuffer(uint64_t **datap, size_t *sizep);

    uint64_t count() { return buf.length(); }

  private:
    JSContext *cx;
    js::Vector<uint64_t> buf;
};

struct SCInput {
  public:
    SCInput(JSContext *cx, const uint64_t *data, size_t nbytes);

    JSContext *context() const { return cx; }

    bool read(uint64_t *p);
    bool readPair(uint32_t *tagp, uint32_t *datap);
    bool readDouble(double *p);
    bool readBytes(void *p, size_t nbytes);
    bool readChars(jschar *p, size_t nchars);

    template <class T>
    bool readArray(T *p, size_t nelems);

  private:
    bool eof();

    void staticAssertions() {
        JS_STATIC_ASSERT(sizeof(jschar) == 2);
        JS_STATIC_ASSERT(sizeof(uint32_t) == 4);
        JS_STATIC_ASSERT(sizeof(double) == 8);
    }

    JSContext *cx;
    const uint64_t *point;
    const uint64_t *end;
};

}

struct JSStructuredCloneReader {
  public:
    explicit JSStructuredCloneReader(js::SCInput &in, const JSStructuredCloneCallbacks *cb,
                                     void *cbClosure)
        : in(in), objs(in.context()), allObjs(in.context()),
          callbacks(cb), closure(cbClosure) { }

    js::SCInput &input() { return in; }
    bool read(js::Value *vp);

  private:
    JSContext *context() { return in.context(); }

    bool checkDouble(double d);
    JSString *readString(uint32_t nchars);
    bool readTypedArray(uint32_t tag, uint32_t nelems, js::Value *vp);
    bool readArrayBuffer(uint32_t nbytes, js::Value *vp);
    bool readId(jsid *idp);
    bool startRead(js::Value *vp);

    js::SCInput &in;

    // Stack of objects with properties remaining to be read.
    js::AutoValueVector objs;

    // Stack of all objects read during this deserialization
    js::AutoValueVector allObjs;

    // The user defined callbacks that will be used for cloning.
    const JSStructuredCloneCallbacks *callbacks;

    // Any value passed to JS_ReadStructuredClone.
    void *closure;

    friend JSBool JS_ReadTypedArray(JSStructuredCloneReader *r, jsval *vp);
};

struct JSStructuredCloneWriter {
  public:
    explicit JSStructuredCloneWriter(js::SCOutput &out, const JSStructuredCloneCallbacks *cb,
                                     void *cbClosure)
        : out(out), objs(out.context()), counts(out.context()), ids(out.context()),
          memory(out.context()), callbacks(cb), closure(cbClosure) { }

    bool init() { return memory.init(); }

    bool write(const js::Value &v);

    js::SCOutput &output() { return out; }

  private:
    JSContext *context() { return out.context(); }

    bool writeString(uint32_t tag, JSString *str);
    bool writeId(jsid id);
    bool writeArrayBuffer(JSObject *obj);
    bool writeTypedArray(JSObject *obj);
    bool startObject(JSObject *obj);
    bool startWrite(const js::Value &v);

    inline void checkStack();

    js::SCOutput &out;

    // Vector of objects with properties remaining to be written.
    //
    // NB: These can span multiple compartments, so the compartment must be
    // entered before any manipulation is performed.
    js::AutoValueVector objs;

    // counts[i] is the number of properties of objs[i] remaining to be written.
    // counts.length() == objs.length() and sum(counts) == ids.length().
    js::Vector<size_t> counts;

    // Ids of properties remaining to be written.
    js::AutoIdVector ids;

    // The "memory" list described in the HTML5 internal structured cloning algorithm.
    // memory is a superset of objs; items are never removed from Memory
    // until a serialization operation is finished
    typedef js::HashMap<JSObject *, uint32_t> CloneMemory;
    CloneMemory memory;

    // The user defined callbacks that will be used for cloning.
    const JSStructuredCloneCallbacks *callbacks;

    // Any value passed to JS_WriteStructuredClone.
    void *closure;

    friend JSBool JS_WriteTypedArray(JSStructuredCloneWriter *w, jsval v);
};

#endif /* jsclone_h___ */
