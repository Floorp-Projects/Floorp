/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParallelArray_h__
#define ParallelArray_h__

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"

namespace js {

class ParallelArrayObject;
typedef Rooted<ParallelArrayObject *> RootedParallelArrayObject;
typedef Handle<ParallelArrayObject *> HandleParallelArrayObject;

//
// ParallelArray Overview
//
// Parallel arrays are immutable, possibly multi-dimensional arrays which
// enable parallel computation based on a few base operations. The execution
// model is one of fallback: try to execute operations in parallel, falling
// back to sequential implementation if (for whatever reason) the operation
// could not be executed in paralle. The API allows leeway to implementers to
// decide both representation and what is considered parallelizable.
//
// Currently ParallelArray objects are backed by dense arrays for ease of
// GC. For the higher-dimensional case, data is stored in a packed, row-major
// order representation in the backing dense array. See notes below about
// IndexInfo in how to convert between scalar offsets into the backing array
// and a vector of indices.
//
// ParallelArray objects are always dense. That is, all holes are eagerly
// filled in with undefined instead of being JS_ARRAY_HOLE. This results in a
// break from the behavior of normal JavaScript arrays: if a ParallelArray p
// is missing an indexed property i, p[i] is _always_ undefined and will never
// walk up the prototype chain in search of i.
//
// Except for the comprehension form, all operations (e.g. map, filter,
// reduce) operate on the outermost dimension only. That is, those operations
// only operate on the "rows" of the array. "Element" is used in context of
// ParallelArray objects to mean any indexable value of a ParallelArray
// object. For a one dimensional array, elements are always scalar values. For
// a higher dimensional array, elements could either be scalar values
// (i.e. leaves) or ParallelArray objects of lesser dimensions
// (i.e. subarrays).
//

class ParallelArrayObject : public JSObject {
  public:
    typedef Vector<uint32_t, 4> IndexVector;

    //
    // Helper structure to help index higher-dimensional arrays to convert
    // between a vector of indices and scalar offsets for use in the flat
    // backing dense array.
    //
    // IndexInfo instances _must_ be initialized using one of the initialize
    // methods before use.
    //
    // Typical usage is stack allocating an IndexInfo, initializing it with a
    // particular source ParallelArray object's dimensionality, and mutating
    // the indices member vector. For instance, to iterate through everything
    // in the first 2 dimensions of an array of > 2 dimensions:
    //
    //   IndexInfo iv(cx);
    //   if (!iv.initialize(cx, source, 2))
    //       return false;
    //   for (uint32_t i = 0; i < iv.dimensions[0]; i++) {
    //       for (uint32_t j = 0; j < iv.dimensions[1]; j++) {
    //           iv.indices[0] = i;
    //           iv.indices[1] = j;
    //           if (source->getParallelArrayElement(cx, iv, &elem))
    //               ...
    //       }
    //   }
    //
    // Note from the above that it is not required to fill out the indices
    // vector up to the full dimensionality. For an N-dimensional array,
    // having an indices vector of length D < N indexes a subarray.
    //

    struct IndexInfo {
        // Vector of indices. Should be large enough to hold up to
        // dimensions.length indices.
        IndexVector indices;

        // Vector of dimensions of the ParallelArray object that the indices
        // are meant to index into.
        IndexVector dimensions;

        // Cached partial products of the dimensions defined by the following
        // recurrence:
        //
        //   partialProducts[n] =
        //     1                                      if n == |dimensions|
        //     dimensions[n+1] * partialProducts[n+1] otherwise
        //
        // These are used for computing scalar offsets.
        IndexVector partialProducts;

        IndexInfo(JSContext *cx)
            : indices(cx), dimensions(cx), partialProducts(cx)
        {}

        // Prepares indices and computes partial products. The space argument
        // is the index space. The indices vector is resized to be of length
        // space.
        //
        // The dimensions vector must be filled already, and space must be <=
        // dimensions.length().
        inline bool initialize(uint32_t space);

        // Load dimensions from a source, then initialize as above.
        inline bool initialize(JSContext *cx, HandleParallelArrayObject source,
                               uint32_t space);

        // Get the scalar length according to the dimensions vector, i.e. the
        // product of the dimensions vector.
        inline uint32_t scalarLengthOfDimensions();

        // Compute the scalar index from the current index vector.
        inline uint32_t toScalar();

        // Set the index vector according to a scalar index.
        inline bool fromScalar(uint32_t index);

        inline bool inBounds() const;
        bool isInitialized() const;
    };

    static JSObject *initClass(JSContext *cx, JSObject *obj);
    static Class class_;

    static inline bool is(const Value &v);
    static inline bool is(JSObject *obj);
    static inline ParallelArrayObject *as(JSObject *obj);

    inline JSObject *dimensionArray();
    inline JSObject *buffer();
    inline uint32_t bufferOffset();
    inline uint32_t outermostDimension();
    inline bool isOneDimensional();
    inline bool getDimensions(JSContext *cx, IndexVector &dims);

    // The general case; requires an initialized IndexInfo.
    bool getParallelArrayElement(JSContext *cx, IndexInfo &iv, MutableHandleValue vp);

    // Get the element at index in the outermost dimension. This is a
    // convenience function designed to require an IndexInfo only if it is
    // actually needed.
    //
    // If the parallel array is multidimensional, then the caller must provide
    // an IndexInfo initialized to length 1, which is used to access the
    // array. This argument is modified. If the parallel array is
    // one-dimensional, then maybeIV may be null.
    bool getParallelArrayElement(JSContext *cx, uint32_t index, IndexInfo *maybeIV,
                                 MutableHandleValue vp);

    // Get the element at index in the outermost dimension. This is a
    // convenience function that initializes a temporary
    // IndexInfo if the parallel array is multidimensional.
    bool getParallelArrayElement(JSContext *cx, uint32_t index, MutableHandleValue vp);

    bool toStringBuffer(JSContext *cx, bool useLocale, StringBuffer &sb);

    // Note that this is not an object op but called directly from the
    // iteration code, as we enumerate prototypes ourselves.
    static bool enumerate(JSContext *cx, HandleObject obj, unsigned flags,
                          AutoIdVector *props);

  private:
    enum {
        // The ParallelArray API refers to dimensions as "shape", but to avoid
        // confusion with the internal engine notion of a shape we call it
        // "dimensions" here.
        SLOT_DIMENSIONS = 0,

        // Underlying dense array.
        SLOT_BUFFER,

        // First index of the underlying buffer to be considered in bounds.
        SLOT_BUFFER_OFFSET,

        RESERVED_SLOTS
    };

    enum ExecutionStatus {
        ExecutionFailed = 0,
        ExecutionCompiled,
        ExecutionSucceeded
    };

    // Execution modes are kept as static instances of structs that implement
    // a signature that comprises of build, map, fold, scatter, and filter,
    // whose argument lists are defined in the macros below.
    //
    // Even though the base class |ExecutionMode| is purely abstract, we only
    // use dynamic dispatch when using the debug options. Almost always we
    // directly call the member function on one of the statics.

#define JS_PA_build_ARGS               \
    JSContext *cx,                     \
    IndexInfo &iv,                     \
    HandleObject elementalFun,         \
    HandleObject buffer

#define JS_PA_map_ARGS                 \
    JSContext *cx,                     \
    HandleParallelArrayObject source,  \
    HandleObject elementalFun,         \
    HandleObject buffer

#define JS_PA_reduce_ARGS              \
    JSContext *cx,                     \
    HandleParallelArrayObject source,  \
    HandleObject elementalFun,         \
    HandleObject buffer,               \
    MutableHandleValue vp

#define JS_PA_scatter_ARGS             \
    JSContext *cx,                     \
    HandleParallelArrayObject source,  \
    HandleObject targets,              \
    const Value &defaultValue,         \
    HandleObject conflictFun,          \
    HandleObject buffer

#define JS_PA_filter_ARGS              \
    JSContext *cx,                     \
    HandleParallelArrayObject source,  \
    HandleObject filters,              \
    HandleObject buffer

#define JS_PA_DECLARE_OP(NAME) \
    ExecutionStatus NAME(JS_PA_ ## NAME ## _ARGS)

#define JS_PA_DECLARE_ALL_OPS          \
    JS_PA_DECLARE_OP(build);           \
    JS_PA_DECLARE_OP(map);             \
    JS_PA_DECLARE_OP(reduce);          \
    JS_PA_DECLARE_OP(scatter);         \
    JS_PA_DECLARE_OP(filter);

    class ExecutionMode {
      public:
        // The comprehension form. Builds a higher-dimensional array using a
        // kernel function.
        virtual JS_PA_DECLARE_OP(build) = 0;

        // Maps a kernel function over the outermost dimension of the array.
        virtual JS_PA_DECLARE_OP(map) = 0;

        // Reduce to a value using a kernel function. Scan is like reduce, but
        // keeps the intermediate results in an array.
        virtual JS_PA_DECLARE_OP(reduce) = 0;

        // Scatter elements according to an index map.
        virtual JS_PA_DECLARE_OP(scatter) = 0;

        // Filter elements according to a truthy array.
        virtual JS_PA_DECLARE_OP(filter) = 0;

        virtual const char *toString() = 0;
    };

    // Fallback means try parallel first, and if unable to execute in
    // parallel, execute sequentially.
    class FallbackMode : public ExecutionMode {
      public:
        JS_PA_DECLARE_ALL_OPS
        const char *toString() { return "fallback"; }
    };

    class ParallelMode : public ExecutionMode {
      public:
        JS_PA_DECLARE_ALL_OPS
        const char *toString() { return "parallel"; }
    };

    class SequentialMode : public ExecutionMode {
      public:
        JS_PA_DECLARE_ALL_OPS
        const char *toString() { return "sequential"; }
    };

    static SequentialMode sequential;
    static ParallelMode parallel;
    static FallbackMode fallback;

#undef JS_PA_build_ARGS
#undef JS_PA_map_ARGS
#undef JS_PA_reduce_ARGS
#undef JS_PA_scatter_ARGS
#undef JS_PA_filter_ARGS
#undef JS_PA_DECLARE_OP
#undef JS_PA_DECLARE_ALL_OPS

#ifdef DEBUG
    // Debug options can be passed in as an extra argument to the
    // operations. The grammar is:
    //
    //   options ::= { mode: "par" | "seq",
    //                 expect: "fail" | "bail" | "success" }
    struct DebugOptions {
        ExecutionMode *mode;
        ExecutionStatus expect;
        bool init(JSContext *cx, const Value &v);
        bool check(JSContext *cx, ExecutionStatus actual);
    };

    static const char *ExecutionStatusToString(ExecutionStatus ss);
#endif

    static JSFunctionSpec methods[];
    static Class protoClass;

    static inline bool DenseArrayToIndexVector(JSContext *cx, HandleObject obj,
                                               IndexVector &indices);

    bool toStringBufferImpl(JSContext *cx, IndexInfo &iv, bool useLocale,
                            HandleObject buffer, StringBuffer &sb);

    static bool create(JSContext *cx, MutableHandleValue vp);
    static bool create(JSContext *cx, HandleObject buffer, MutableHandleValue vp);
    static bool create(JSContext *cx, HandleObject buffer, uint32_t offset,
                       const IndexVector &dims, MutableHandleValue vp);
    static JSBool construct(JSContext *cx, unsigned argc, Value *vp);

    static bool map(JSContext *cx, CallArgs args);
    static bool reduce(JSContext *cx, CallArgs args);
    static bool scan(JSContext *cx, CallArgs args);
    static bool scatter(JSContext *cx, CallArgs args);
    static bool filter(JSContext *cx, CallArgs args);
    static bool flatten(JSContext *cx, CallArgs args);
    static bool partition(JSContext *cx, CallArgs args);
    static bool get(JSContext *cx, CallArgs args);
    static bool dimensionsGetter(JSContext *cx, CallArgs args);
    static bool lengthGetter(JSContext *cx, CallArgs args);
    static bool toString(JSContext *cx, CallArgs args);
    static bool toLocaleString(JSContext *cx, CallArgs args);
    static bool toSource(JSContext *cx, CallArgs args);

    static void mark(JSTracer *trc, JSObject *obj);
    static JSBool lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                MutableHandleObject objp, MutableHandleShape propp);
    static JSBool lookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                 MutableHandleObject objp, MutableHandleShape propp);
    static JSBool lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                MutableHandleObject objp, MutableHandleShape propp);
    static JSBool lookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                MutableHandleObject objp, MutableHandleShape propp);
    static JSBool defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                                JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool defineProperty(JSContext *cx, HandleObject obj,
                                 HandlePropertyName name, HandleValue value,
                                 JSPropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool defineElement(JSContext *cx, HandleObject obj,
                                uint32_t index, HandleValue value,
                                PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool defineSpecial(JSContext *cx, HandleObject obj,
                                HandleSpecialId sid, HandleValue value,
                                PropertyOp getter, StrictPropertyOp setter, unsigned attrs);
    static JSBool getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                             HandleId id, MutableHandleValue vp);
    static JSBool getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                              HandlePropertyName name, MutableHandleValue vp);
    static JSBool getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                             uint32_t index, MutableHandleValue vp);
    static JSBool getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver,
                             HandleSpecialId sid, MutableHandleValue vp);
    static JSBool setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                             MutableHandleValue vp, JSBool strict);
    static JSBool setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                              MutableHandleValue vp, JSBool strict);
    static JSBool setElement(JSContext *cx, HandleObject obj, uint32_t index,
                             MutableHandleValue vp, JSBool strict);
    static JSBool setSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                             MutableHandleValue vp, JSBool strict);
    static JSBool getGenericAttributes(JSContext *cx, HandleObject obj, HandleId id,
                                       unsigned *attrsp);
    static JSBool getPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                        unsigned *attrsp);
    static JSBool getElementAttributes(JSContext *cx, HandleObject obj, uint32_t index,
                                       unsigned *attrsp);
    static JSBool getSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                       unsigned *attrsp);
    static JSBool setGenericAttributes(JSContext *cx, HandleObject obj, HandleId id,
                                       unsigned *attrsp);
    static JSBool setPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                        unsigned *attrsp);
    static JSBool setElementAttributes(JSContext *cx, HandleObject obj, uint32_t index,
                                       unsigned *attrsp);
    static JSBool setSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                       unsigned *attrsp);
    static JSBool deleteGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                MutableHandleValue rval, JSBool strict);
    static JSBool deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                 MutableHandleValue rval, JSBool strict);
    static JSBool deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                MutableHandleValue rval, JSBool strict);
    static JSBool deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                MutableHandleValue rval, JSBool strict);
};

} // namespace js

extern JSObject *
js_InitParallelArrayClass(JSContext *cx, JSObject *obj);

#endif // ParallelArray_h__
