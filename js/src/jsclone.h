/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
