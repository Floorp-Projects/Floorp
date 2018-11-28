/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JSAPI functions and callbacks related to WHATWG Stream objects.
 *
 * Much of the API here mirrors the standard algorithms and standard JS methods
 * of the objects defined in the Streams standard. One difference is that the
 * functionality of the JS controller object is exposed to C++ as functions
 * taking ReadableStream instances instead, for convenience.
 *
 * ## External streams
 *
 * Embeddings can create ReadableStreams that read from custom C++ data
 * sources. Such streams are always byte streams: the chunks they produce are
 * typed arrays (and they will support ReadableStreamBYOBReader once we have
 * it).
 *
 * When creating an "external readable stream" using
 * JS::NewReadableExternalSourceStreamObject, an underlying source can be
 * passed to be stored on the stream. The underlying source is treated as an
 * opaque void* pointer by the JS engine: it's purely meant as a reference to
 * be used by the embedding to identify whatever actual source it uses to
 * supply data for the stream.
 *
 * External readable streams are optimized to allow the embedding to interact
 * with them with a minimum of overhead: chunks aren't enqueued as individual
 * typed arrays; instead, the embedding only updates the amount of data
 * available using ReadableStreamUpdateDataAvailableFromSource. When JS
 * requests data from a reader, WriteIntoReadRequestBufferCallback is invoked,
 * asking the embedding to write data directly into the buffer we're about to
 * hand to JS.
 *
 * Additionally, ReadableStreamGetExternalUnderlyingSource can be used to get
 * the void* pointer to the underlying source. This locks the stream until it
 * is released again using JS::ReadableStreamReleaseExternalUnderlyingSource.
 *
 * Embeddings can use this to optimize away the JS `ReadableStream` overhead
 * when an embedding-defined C++ stream is passed to an embedding-defined C++
 * consumer. For example, consider a ServiceWorker piping a `fetch` Response
 * body to a TextDecoder. Instead of copying chunks of data into JS typed array
 * buffers and creating a Promise per chunk, only to immediately resolve the
 * Promises and read the data out again, the embedding can directly feed the
 * incoming data to the TextDecoder.
 */

#ifndef js_Stream_h
#define js_Stream_h

#include <stddef.h>

#include "jstypes.h"

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"

namespace JS {

/**
 * ## Readable stream callbacks
 *
 * Compartment safety: All callbacks (except Finalize) receive `cx` and
 * `stream` arguments. SpiderMonkey enters the realm of the stream object
 * before invoking these callbacks, so `stream` is never a wrapper. Other
 * arguments may be wrappers.
 */

/**
 * Invoked whenever a reader desires more data from a ReadableStream's
 * embedding-provided underlying source.
 *
 * The given |desiredSize| is the absolute size, not a delta from the previous
 * desired size.
 */
typedef void
(* RequestReadableStreamDataCallback)(JSContext* cx, HandleObject stream,
                                      void* underlyingSource, size_t desiredSize);

/**
 * Invoked to cause the embedding to fill the given |buffer| with data from
 * the given embedding-provided underlying source.
 *
 * This can only happen after the embedding has updated the amount of data
 * available using JS::ReadableStreamUpdateDataAvailableFromSource. If at
 * least one read request is pending when
 * JS::ReadableStreamUpdateDataAvailableFromSource is called,
 * the WriteIntoReadRequestBufferCallback is invoked immediately from under
 * the call to JS::WriteIntoReadRequestBufferCallback. If not, it is invoked
 * if and when a new read request is made.
 *
 * Note: This callback *must not cause GC*, because that could potentially
 * invalidate the |buffer| pointer.
 */
typedef void
(* WriteIntoReadRequestBufferCallback)(JSContext* cx, HandleObject stream,
                                       void* underlyingSource, void* buffer, size_t length,
                                       size_t* bytesWritten);

/**
 * Invoked in reaction to the ReadableStream being canceled to allow the
 * embedding to free the underlying source.
 *
 * This is equivalent to calling |cancel| on non-external underlying sources
 * provided to the ReadableStream constructor in JavaScript.
 *
 * The given |reason| is the JS::Value that was passed as an argument to
 * ReadableStream#cancel().
 *
 * The returned JS::Value will be used to resolve the Promise returned by
 * ReadableStream#cancel().
 */
typedef Value
(* CancelReadableStreamCallback)(JSContext* cx, HandleObject stream,
                                 void* underlyingSource, HandleValue reason);

/**
 * Invoked in reaction to a ReadableStream with an embedding-provided
 * underlying source being closed.
 */
typedef void
(* ReadableStreamClosedCallback)(JSContext* cx, HandleObject stream, void* underlyingSource);

/**
 * Invoked in reaction to a ReadableStream with an embedding-provided
 * underlying source being errored with the
 * given reason.
 */
typedef void
(* ReadableStreamErroredCallback)(JSContext* cx, HandleObject stream, void* underlyingSource,
                                  HandleValue reason);

/**
 * Invoked in reaction to a ReadableStream with an embedding-provided
 * underlying source being finalized. Only the underlying source is passed
 * as an argument, while the ReadableStream itself is not to prevent the
 * embedding from operating on a JSObject that might not be in a valid state
 * anymore.
 *
 * Note: the ReadableStream might be finalized on a background thread. That
 * means this callback might be invoked from an arbitrary thread, which the
 * embedding must be able to handle.
 */
typedef void
(* ReadableStreamFinalizeCallback)(void* underlyingSource);

/**
 * Sets runtime-wide callbacks to use for interacting with embedding-provided
 * hooks for operating on ReadableStream instances.
 *
 * See the documentation for the individual callback types for details.
 */
extern JS_PUBLIC_API void
SetReadableStreamCallbacks(JSContext* cx,
                           RequestReadableStreamDataCallback dataRequestCallback,
                           WriteIntoReadRequestBufferCallback writeIntoReadRequestCallback,
                           CancelReadableStreamCallback cancelCallback,
                           ReadableStreamClosedCallback closedCallback,
                           ReadableStreamErroredCallback erroredCallback,
                           ReadableStreamFinalizeCallback finalizeCallback);

extern JS_PUBLIC_API bool
HasReadableStreamCallbacks(JSContext* cx);

/**
 * Returns a new instance of the ReadableStream builtin class in the current
 * compartment, configured as a default stream.
 * If a |proto| is passed, that gets set as the instance's [[Prototype]]
 * instead of the original value of |ReadableStream.prototype|.
 */
extern JS_PUBLIC_API JSObject*
NewReadableDefaultStreamObject(JSContext* cx, HandleObject underlyingSource = nullptr,
                               HandleFunction size = nullptr, double highWaterMark = 1,
                               HandleObject proto = nullptr);

/**
 * Returns a new instance of the ReadableStream builtin class in the current
 * compartment, with the right slot layout. If a |proto| is passed, that gets
 * set as the instance's [[Prototype]] instead of the original value of
 * |ReadableStream.prototype|.
 *
 * The instance is optimized for operating as a byte stream backed by an
 * embedding-provided underlying source, using the callbacks set via
 * |JS::SetReadableStreamCallbacks|.
 *
 * Note: the embedding is responsible for ensuring that the pointer to the
 * underlying source stays valid as long as the stream can be read from.
 * The underlying source can be freed if the tree is canceled or errored.
 * It can also be freed if the stream is destroyed. The embedding is notified
 * of that using ReadableStreamFinalizeCallback.
 *
 * Note: |underlyingSource| must have an even address.
 */
extern JS_PUBLIC_API JSObject*
NewReadableExternalSourceStreamObject(JSContext* cx, void* underlyingSource,
                                      HandleObject proto = nullptr);

/**
 * Returns the embedding-provided underlying source of the given |stream|.
 *
 * Can be used to optimize operations if both the underlying source and the
 * intended sink are embedding-provided. In that case it might be
 * preferrable to pipe data directly from source to sink without interacting
 * with the stream at all.
 *
 * Locks the stream until ReadableStreamReleaseExternalUnderlyingSource is
 * called.
 *
 * Throws an exception if the stream is locked, i.e. if a reader has been
 * acquired for the stream, or if ReadableStreamGetExternalUnderlyingSource
 * has been used previously without releasing the external source again.
 *
 * Throws an exception if the stream isn't readable, i.e if it is errored or
 * closed. This is different from ReadableStreamGetReader because we don't
 * have a Promise to resolve/reject, which a reader provides.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 *
 * Asserts that the stream has an embedding-provided underlying source.
 */
extern JS_PUBLIC_API bool
ReadableStreamGetExternalUnderlyingSource(JSContext* cx, HandleObject stream, void** source);

/**
 * Releases the embedding-provided underlying source of the given |stream|,
 * returning the stream into an unlocked state.
 *
 * Asserts that the stream was locked through
 * ReadableStreamGetExternalUnderlyingSource.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 *
 * Asserts that the stream has an embedding-provided underlying source.
 */
extern JS_PUBLIC_API bool
ReadableStreamReleaseExternalUnderlyingSource(JSContext* cx, HandleObject stream);

/**
 * Update the amount of data available at the underlying source of the given
 * |stream|.
 *
 * Can only be used for streams with an embedding-provided underlying source.
 * The JS engine will use the given value to satisfy read requests for the
 * stream by invoking the JS::WriteIntoReadRequestBuffer callback.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamUpdateDataAvailableFromSource(JSContext* cx, HandleObject stream,
                                            uint32_t availableData);

/**
 * Returns true if the given object is a ReadableStream object or an
 * unwrappable wrapper for one, false otherwise.
 */
extern JS_PUBLIC_API bool
IsReadableStream(JSObject* obj);

/**
 * Returns true if the given object is a ReadableStreamDefaultReader or
 * ReadableStreamBYOBReader object or an unwrappable wrapper for one, false
 * otherwise.
 */
extern JS_PUBLIC_API bool
IsReadableStreamReader(JSObject* obj);

/**
 * Returns true if the given object is a ReadableStreamDefaultReader object
 * or an unwrappable wrapper for one, false otherwise.
 */
extern JS_PUBLIC_API bool
IsReadableStreamDefaultReader(JSObject* obj);

enum class ReadableStreamMode {
    Default,
    Byte,
    ExternalSource
};

/**
 * Returns the stream's ReadableStreamMode. If the mode is |Byte| or
 * |ExternalSource|, it's possible to acquire a BYOB reader for more optimized
 * operations.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamGetMode(JSContext* cx, HandleObject stream, ReadableStreamMode* mode);

enum class ReadableStreamReaderMode {
    Default
};

/**
 * Returns true if the given ReadableStream is readable, false if not.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamIsReadable(JSContext* cx, HandleObject stream, bool* result);

/**
 * Returns true if the given ReadableStream is locked, false if not.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamIsLocked(JSContext* cx, HandleObject stream, bool* result);

/**
 * Returns true if the given ReadableStream is disturbed, false if not.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamIsDisturbed(JSContext* cx, HandleObject stream, bool* result);

/**
 * Cancels the given ReadableStream with the given reason and returns a
 * Promise resolved according to the result.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API JSObject*
ReadableStreamCancel(JSContext* cx, HandleObject stream, HandleValue reason);

/**
 * Creates a reader of the type specified by the mode option and locks the
 * stream to the new reader.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one. The returned object will always be created in the
 * current cx compartment.
 */
extern JS_PUBLIC_API JSObject*
ReadableStreamGetReader(JSContext* cx, HandleObject stream, ReadableStreamReaderMode mode);

/**
 * Tees the given ReadableStream and stores the two resulting streams in
 * outparams. Returns false if the operation fails, e.g. because the stream is
 * locked.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamTee(JSContext* cx, HandleObject stream,
                  MutableHandleObject branch1Stream, MutableHandleObject branch2Stream);

/**
 * Retrieves the desired combined size of additional chunks to fill the given
 * ReadableStream's queue. Stores the result in |value| and sets |hasValue| to
 * true on success, returns false on failure.
 *
 * If the stream is errored, the call will succeed but no value will be stored
 * in |value| and |hasValue| will be set to false.
 *
 * Note: This is semantically equivalent to the |desiredSize| getter on
 * the stream controller's prototype in JS. We expose it with the stream
 * itself as a target for simplicity.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamGetDesiredSize(JSContext* cx, JSObject* stream, bool* hasValue, double* value);

/**
 * Closes the given ReadableStream.
 *
 * Throws a TypeError and returns false if the closing operation fails.
 *
 * Note: This is semantically equivalent to the |close| method on
 * the stream controller's prototype in JS. We expose it with the stream
 * itself as a target for simplicity.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamClose(JSContext* cx, HandleObject stream);

/**
 * Returns true if the given ReadableStream reader is locked, false otherwise.
 *
 * Asserts that |reader| is a ReadableStreamDefaultReader or
 * ReadableStreamBYOBReader object or an unwrappable wrapper for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamReaderIsClosed(JSContext* cx, HandleObject reader, bool* result);

/**
 * Enqueues the given chunk in the given ReadableStream.
 *
 * Throws a TypeError and returns false if the enqueing operation fails.
 *
 * Note: This is semantically equivalent to the |enqueue| method on
 * the stream controller's prototype in JS. We expose it with the stream
 * itself as a target for simplicity.
 *
 * If the ReadableStream has an underlying byte source, the given chunk must
 * be a typed array or a DataView. Consider using
 * ReadableByteStreamEnqueueBuffer.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamEnqueue(JSContext* cx, HandleObject stream, HandleValue chunk);

/**
 * Errors the given ReadableStream, causing all future interactions to fail
 * with the given error value.
 *
 * Throws a TypeError and returns false if the erroring operation fails.
 *
 * Note: This is semantically equivalent to the |error| method on
 * the stream controller's prototype in JS. We expose it with the stream
 * itself as a target for simplicity.
 *
 * Asserts that |stream| is a ReadableStream object or an unwrappable wrapper
 * for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamError(JSContext* cx, HandleObject stream, HandleValue error);

/**
 * C++ equivalent of `reader.cancel(reason)`
 * (both <https://streams.spec.whatwg.org/#default-reader-cancel> and
 * <https://streams.spec.whatwg.org/#byob-reader-cancel>).
 *
 * `reader` must be a stream reader created using `JS::ReadableStreamGetReader`
 * or an unwrappable wrapper for one. (This function is meant to support using
 * C++ to read from streams. It's not meant to allow C++ code to operate on
 * readers created by scripts.)
 */
extern JS_PUBLIC_API bool
ReadableStreamReaderCancel(JSContext* cx, HandleObject reader, HandleValue reason);

/**
 * C++ equivalent of `reader.releaseLock()`
 * (both <https://streams.spec.whatwg.org/#default-reader-release-lock> and
 * <https://streams.spec.whatwg.org/#byob-reader-release-lock>).
 *
 * `reader` must be a stream reader created using `JS::ReadableStreamGetReader`
 * or an unwrappable wrapper for one.
 */
extern JS_PUBLIC_API bool
ReadableStreamReaderReleaseLock(JSContext* cx, HandleObject reader);

/**
 * C++ equivalent of the `reader.read()` method on default readers
 * (<https://streams.spec.whatwg.org/#default-reader-read>).
 *
 * The result is a new Promise object, or null on OOM.
 *
 * `reader` must be the result of calling `JS::ReadableStreamGetReader` with
 * `ReadableStreamReaderMode::Default` mode, or an unwrappable wrapper for such
 * a reader.
 */
extern JS_PUBLIC_API JSObject*
ReadableStreamDefaultReaderRead(JSContext* cx, HandleObject reader);

} // namespace JS

#endif // js_Stream_h
