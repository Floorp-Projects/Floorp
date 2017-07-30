/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"

#include "jsapi-tests/tests.h"

using namespace JS;

char test_buffer_data[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static JSObject*
NewDefaultStream(JSContext* cx, HandleObject source = nullptr, HandleFunction size = nullptr,
                 double highWaterMark = 1, HandleObject proto = nullptr)
{
    RootedObject stream(cx, NewReadableDefaultStreamObject(cx, source, size, highWaterMark,
                                                           proto));
    MOZ_ASSERT_IF(stream, IsReadableStream(stream));
    return stream;
}

static JSObject*
NewByteStream(JSContext* cx, double highWaterMark = 0, HandleObject proto = nullptr)
{
    RootedObject source(cx, JS_NewPlainObject(cx));
    MOZ_ASSERT(source);

    RootedObject stream(cx, NewReadableByteStreamObject(cx, source, highWaterMark, proto));
    MOZ_ASSERT_IF(stream, IsReadableStream(stream));
    return stream;
}

static bool dataRequestCBCalled = false;
static void
DataRequestCB(JSContext* cx, HandleObject stream, void* underlyingSource, uint8_t flags,
              size_t desiredSize)
{
    MOZ_ASSERT(!dataRequestCBCalled, "Invalid test setup");
    dataRequestCBCalled = true;
}

static bool writeIntoRequestBufferCBCalled = false;
static void
WriteIntoRequestBufferCB(JSContext* cx, HandleObject stream, void* underlyingSource, uint8_t flags,
                         void* buffer, size_t length, size_t* bytesWritten)
{
    MOZ_ASSERT(!writeIntoRequestBufferCBCalled, "Invalid test setup");
    MOZ_ASSERT(length <= sizeof(test_buffer_data));
    memcpy(buffer, test_buffer_data, length);
    writeIntoRequestBufferCBCalled = true;
    *bytesWritten = length;
}

static bool cancelStreamCBCalled = false;
static Value cancelStreamReason;
static Value
CancelStreamCB(JSContext* cx, HandleObject stream, void* underlyingSource, uint8_t flags,
               HandleValue reason)
{
    MOZ_ASSERT(!cancelStreamCBCalled, "Invalid test setup");
    cancelStreamCBCalled = true;
    cancelStreamReason = reason;
    return reason;
}

static bool streamClosedCBCalled = false;
static Value streamClosedReason;
static void
StreamClosedCB(JSContext* cx, HandleObject stream, void* underlyingSource, uint8_t flags)
{
    MOZ_ASSERT(!streamClosedCBCalled, "Invalid test setup");
    streamClosedCBCalled = true;
}

static bool streamErroredCBCalled = false;
static Value streamErroredReason;
static void
StreamErroredCB(JSContext* cx, HandleObject stream, void* underlyingSource, uint8_t flags,
                HandleValue reason)
{
    MOZ_ASSERT(!streamErroredCBCalled, "Invalid test setup");
    streamErroredCBCalled = true;
    streamErroredReason = reason;
}

static bool finalizeStreamCBCalled = false;
static void* finalizedStreamUnderlyingSource;
static void
FinalizeStreamCB(void* underlyingSource, uint8_t flags)
{
    MOZ_ASSERT(!finalizeStreamCBCalled, "Invalid test setup");
    finalizeStreamCBCalled = true;
    finalizedStreamUnderlyingSource = underlyingSource;
}

static void
ResetCallbacks()
{
    dataRequestCBCalled = false;
    writeIntoRequestBufferCBCalled = false;
    cancelStreamReason = UndefinedValue();
    cancelStreamCBCalled = false;
    streamClosedCBCalled = false;
    streamErroredCBCalled = false;
    finalizeStreamCBCalled = false;
}

static bool
GetIterResult(JSContext* cx, HandleObject promise, MutableHandleValue value, bool* done)
{
    RootedObject iterResult(cx, &GetPromiseResult(promise).toObject());

    bool found;
    if (!JS_HasProperty(cx, iterResult, "value", &found))
        return false;
    MOZ_ASSERT(found);
    if (!JS_HasProperty(cx, iterResult, "done", &found))
        return false;
    MOZ_ASSERT(found);

    RootedValue doneVal(cx);
    if (!JS_GetProperty(cx, iterResult, "value", value))
        return false;
    if (!JS_GetProperty(cx, iterResult, "done", &doneVal))
        return false;

    *done = doneVal.toBoolean();
    MOZ_ASSERT_IF(*done, value.isUndefined());

    return true;
}

static JSObject*
GetReadChunk(JSContext* cx, HandleObject readRequest)
{
    MOZ_ASSERT(GetPromiseState(readRequest) == PromiseState::Fulfilled);
    RootedValue resultVal(cx, GetPromiseResult(readRequest));
    MOZ_ASSERT(resultVal.isObject());
    RootedObject result(cx, &resultVal.toObject());
    RootedValue chunkVal(cx);
    JS_GetProperty(cx, result, "value", &chunkVal);
    return &chunkVal.toObject();
}

BEGIN_TEST(testReadableStream_NewReadableStream)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    CHECK(ReadableStreamGetMode(stream) == ReadableStreamMode::Default);
    return true;
}
END_TEST(testReadableStream_NewReadableStream)

BEGIN_TEST(testReadableStream_NewReadableByteStream)
{
    RootedObject stream(cx, NewByteStream(cx));
    CHECK(stream);
    CHECK(ReadableStreamGetMode(stream) == ReadableStreamMode::Byte);
    return true;
}
END_TEST(testReadableStream_NewReadableByteStream)

BEGIN_TEST(testReadableStream_ReadableStreamGetReaderDefault)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);
    CHECK(IsReadableStreamDefaultReader(reader));
    CHECK(ReadableStreamIsLocked(stream));
    CHECK(!ReadableStreamReaderIsClosed(reader));

    return true;
}
END_TEST(testReadableStream_ReadableStreamGetReaderDefault)

BEGIN_TEST(testReadableStream_ReadableStreamGetReaderBYOB)
{
    RootedObject stream(cx, NewByteStream(cx));
    CHECK(stream);

    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::BYOB));
    CHECK(reader);
    CHECK(IsReadableStreamBYOBReader(reader));
    CHECK(ReadableStreamIsLocked(stream));
    CHECK(!ReadableStreamReaderIsClosed(reader));

    return true;
}
END_TEST(testReadableStream_ReadableStreamGetReaderBYOB)

BEGIN_TEST(testReadableStream_ReadableStreamTee)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject leftStream(cx);
    RootedObject rightStream(cx);
    CHECK(ReadableStreamTee(cx, stream, &leftStream, &rightStream));
    CHECK(ReadableStreamIsLocked(stream));
    CHECK(leftStream);
    CHECK(IsReadableStream(leftStream));
    CHECK(rightStream);
    CHECK(IsReadableStream(rightStream));

    return true;
}
END_TEST(testReadableStream_ReadableStreamTee)

BEGIN_TEST(testReadableStream_ReadableStreamEnqueue)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject chunk(cx, JS_NewPlainObject(cx));
    CHECK(chunk);
    RootedValue chunkVal(cx, ObjectValue(*chunk));
    CHECK(ReadableStreamEnqueue(cx, stream, chunkVal));

    return true;
}
END_TEST(testReadableStream_ReadableStreamEnqueue)

BEGIN_TEST(testReadableStream_ReadableByteStreamEnqueue)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject chunk(cx, JS_NewUint8Array(cx, 42));
    CHECK(chunk);
    CHECK(!ReadableByteStreamEnqueueBuffer(cx, stream, chunk));
    CHECK(JS_IsExceptionPending(cx));

    return true;
}
END_TEST(testReadableStream_ReadableByteStreamEnqueue)

BEGIN_TEST(testReadableStream_ReadableStreamDefaultReaderRead)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);

    RootedObject request(cx, ReadableStreamDefaultReaderRead(cx, reader));
    CHECK(request);
    CHECK(IsPromiseObject(request));
    CHECK(GetPromiseState(request) == PromiseState::Pending);

    RootedObject chunk(cx, JS_NewPlainObject(cx));
    CHECK(chunk);
    RootedValue chunkVal(cx, ObjectValue(*chunk));
    CHECK(ReadableStreamEnqueue(cx, stream, chunkVal));

    CHECK(GetReadChunk(cx, request) == chunk);

    return true;
}
END_TEST(testReadableStream_ReadableStreamDefaultReaderRead)

BEGIN_TEST(testReadableStream_ReadableByteStreamDefaultReaderRead)
{
    RootedObject stream(cx, NewByteStream(cx));
    CHECK(stream);

    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);

    RootedObject request(cx, ReadableStreamDefaultReaderRead(cx, reader));
    CHECK(request);
    CHECK(IsPromiseObject(request));
    CHECK(GetPromiseState(request) == PromiseState::Pending);

    size_t length = sizeof(test_buffer_data);
    RootedObject buffer(cx, JS_NewArrayBufferWithExternalContents(cx, length, test_buffer_data));
    CHECK(buffer);
    RootedObject chunk(cx, JS_NewUint8ArrayWithBuffer(cx, buffer, 0, length));
    CHECK(chunk);
    bool isShared;
    CHECK(!JS_IsDetachedArrayBufferObject(buffer));

    CHECK(ReadableByteStreamEnqueueBuffer(cx, stream, chunk));

    CHECK(JS_IsDetachedArrayBufferObject(buffer));
    RootedObject readChunk(cx, GetReadChunk(cx, request));
    CHECK(JS_IsUint8Array(readChunk));
    void* readBufferData;
    {
        JS::AutoCheckCannotGC autoNoGC(cx);
        readBufferData = JS_GetArrayBufferViewData(readChunk, &isShared, autoNoGC);
    }
    CHECK(readBufferData);
    CHECK(!memcmp(test_buffer_data, readBufferData, length));

    return true;
}
END_TEST(testReadableStream_ReadableByteStreamDefaultReaderRead)

BEGIN_TEST(testReadableStream_ReadableByteStreamBYOBReaderRead)
{
    RootedObject stream(cx, NewByteStream(cx));
    CHECK(stream);

    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::BYOB));
    CHECK(reader);

    size_t length = sizeof(test_buffer_data);
    RootedObject targetArray(cx, JS_NewUint8Array(cx, length));
    bool isShared;

    RootedObject request(cx, ReadableStreamBYOBReaderRead(cx, reader, targetArray));
    CHECK(request);
    CHECK(IsPromiseObject(request));
    CHECK(GetPromiseState(request) == PromiseState::Pending);
    CHECK(JS_IsDetachedArrayBufferObject(JS_GetArrayBufferViewBuffer(cx, targetArray, &isShared)));

    RootedObject buffer(cx, JS_NewArrayBufferWithExternalContents(cx, length, test_buffer_data));
    CHECK(buffer);
    CHECK(!JS_IsDetachedArrayBufferObject(buffer));

    CHECK(ReadableByteStreamEnqueueBuffer(cx, stream, buffer));

    CHECK(JS_IsDetachedArrayBufferObject(buffer));
    RootedObject readChunk(cx, GetReadChunk(cx, request));
    CHECK(JS_IsUint8Array(readChunk));
    void* readBufferData;
    {
        JS::AutoCheckCannotGC autoNoGC(cx);
        readBufferData = JS_GetArrayBufferViewData(readChunk, &isShared, autoNoGC);
    }
    CHECK(readBufferData);
    CHECK(!memcmp(test_buffer_data, readBufferData, length));
    // TODO: eliminate the memcpy that happens here.
//    CHECK(readBufferData == test_buffer_data);

    return true;
}
END_TEST(testReadableStream_ReadableByteStreamBYOBReaderRead)

BEGIN_TEST(testReadableStream_ReadableStreamDefaultReaderClose)
{
    SetReadableStreamCallbacks(cx, &DataRequestCB, &WriteIntoRequestBufferCB,
                                             &CancelStreamCB, &StreamClosedCB, &StreamErroredCB,
                                             &FinalizeStreamCB);
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);

    RootedObject request(cx, ReadableStreamDefaultReaderRead(cx, reader));
    CHECK(request);
    CHECK(IsPromiseObject(request));
    CHECK(GetPromiseState(request) == PromiseState::Pending);

    CHECK(ReadableStreamClose(cx, stream));

    bool done;
    RootedValue value(cx);
    CHECK(GetPromiseState(request) == PromiseState::Fulfilled);
    CHECK(GetIterResult(cx, request, &value, &done));
    CHECK(value.isUndefined());
    CHECK(done);

    // The callbacks are only invoked for external streams.
    CHECK(!streamClosedCBCalled);

    return true;
}
END_TEST(testReadableStream_ReadableStreamDefaultReaderClose)

BEGIN_TEST(testReadableStream_ReadableStreamDefaultReaderError)
{
    ResetCallbacks();
    SetReadableStreamCallbacks(cx, &DataRequestCB, &WriteIntoRequestBufferCB,
                                             &CancelStreamCB, &StreamClosedCB, &StreamErroredCB,
                                             &FinalizeStreamCB);
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);

    RootedObject request(cx, ReadableStreamDefaultReaderRead(cx, reader));
    CHECK(request);
    CHECK(IsPromiseObject(request));
    CHECK(GetPromiseState(request) == PromiseState::Pending);

    CHECK(ReadableStreamIsLocked(stream));
    CHECK(ReadableStreamIsReadable(stream));
    RootedValue error(cx, Int32Value(42));
    CHECK(ReadableStreamError(cx, stream, error));

    CHECK(GetPromiseState(request) == PromiseState::Rejected);
    RootedValue reason(cx, GetPromiseResult(request));
    CHECK(reason.isInt32());
    CHECK(reason.toInt32() == 42);

    // The callbacks are only invoked for external streams.
    CHECK(!streamErroredCBCalled);

    return true;
}
END_TEST(testReadableStream_ReadableStreamDefaultReaderError)

static JSObject*
NewExternalSourceStream(JSContext* cx, void* underlyingSource,
                        RequestReadableStreamDataCallback dataRequestCallback,
                        WriteIntoReadRequestBufferCallback writeIntoReadRequestCallback,
                        CancelReadableStreamCallback cancelCallback,
                        ReadableStreamClosedCallback closedCallback,
                        ReadableStreamErroredCallback erroredCallback,
                        ReadableStreamFinalizeCallback finalizeCallback)
{
    SetReadableStreamCallbacks(cx, dataRequestCallback, writeIntoReadRequestCallback,
                                             cancelCallback, closedCallback, erroredCallback,
                                             finalizeCallback);
    RootedObject stream(cx, NewReadableExternalSourceStreamObject(cx, underlyingSource));
    MOZ_ASSERT_IF(stream, IsReadableStream(stream));
    return stream;
}

BEGIN_TEST(testReadableStream_CreateReadableByteStreamWithExternalSource)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx, &test_buffer_data, &DataRequestCB,
                                                    &WriteIntoRequestBufferCB, &CancelStreamCB,
                                                    &StreamClosedCB, &StreamErroredCB,
                                                    &FinalizeStreamCB));
    CHECK(stream);
    CHECK(ReadableStreamGetMode(stream) == JS::ReadableStreamMode::ExternalSource);
    void* underlyingSource;
    CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
    CHECK(underlyingSource == &test_buffer_data);
    CHECK(ReadableStreamIsLocked(stream));
    ReadableStreamReleaseExternalUnderlyingSource(stream);

    return true;
}
END_TEST(testReadableStream_CreateReadableByteStreamWithExternalSource)

BEGIN_TEST(testReadableStream_ExternalSourceCancel)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx, &test_buffer_data, &DataRequestCB,
                                                    &WriteIntoRequestBufferCB, &CancelStreamCB,
                                                    &StreamClosedCB, &StreamErroredCB,
                                                    &FinalizeStreamCB));
    CHECK(stream);
    RootedValue reason(cx, Int32Value(42));
    CHECK(ReadableStreamCancel(cx, stream, reason));
    CHECK(cancelStreamCBCalled);
    CHECK(cancelStreamReason == reason);

    return true;
}
END_TEST(testReadableStream_ExternalSourceCancel)

BEGIN_TEST(testReadableStream_ExternalSourceGetReader)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx, &test_buffer_data, &DataRequestCB,
                                                    &WriteIntoRequestBufferCB, &CancelStreamCB,
                                                    &StreamClosedCB, &StreamErroredCB,
                                                    &FinalizeStreamCB));
    CHECK(stream);

    RootedValue streamVal(cx, ObjectValue(*stream));
    CHECK(JS_SetProperty(cx, global, "stream", streamVal));
    RootedValue rval(cx);
    EVAL("stream.getReader()", &rval);
    CHECK(rval.isObject());
    RootedObject reader(cx, &rval.toObject());
    CHECK(IsReadableStreamDefaultReader(reader));

    return true;
}
END_TEST(testReadableStream_ExternalSourceGetReader)

BEGIN_TEST(testReadableStream_ExternalSourceUpdateAvailableData)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx, &test_buffer_data, &DataRequestCB,
                                                    &WriteIntoRequestBufferCB, &CancelStreamCB,
                                                    &StreamClosedCB, &StreamErroredCB,
                                                    &FinalizeStreamCB));
    CHECK(stream);

    ReadableStreamUpdateDataAvailableFromSource(cx, stream, 1024);

    return true;
}
END_TEST(testReadableStream_ExternalSourceUpdateAvailableData)

struct ReadFromExternalSourceFixture : public JSAPITest
{
    virtual ~ReadFromExternalSourceFixture() {}

    bool readWithoutDataAvailable(const char* evalSrc, const char* evalSrc2,
                                  uint32_t writtenLength)
    {
        ResetCallbacks();
        definePrint();

        RootedObject stream(cx, NewExternalSourceStream(cx, &test_buffer_data, &DataRequestCB,
                                                        &WriteIntoRequestBufferCB,
                                                        &CancelStreamCB,
                                                        &StreamClosedCB, &StreamErroredCB,
                                                        &FinalizeStreamCB));
        CHECK(stream);
        js::RunJobs(cx);
        void* underlyingSource;
        CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
        CHECK(underlyingSource == &test_buffer_data);
        CHECK(ReadableStreamIsLocked(stream));
        ReadableStreamReleaseExternalUnderlyingSource(stream);

        RootedValue streamVal(cx, ObjectValue(*stream));
        CHECK(JS_SetProperty(cx, global, "stream", streamVal));

        RootedValue rval(cx);
        EVAL(evalSrc, &rval);
        CHECK(dataRequestCBCalled);
        CHECK(!writeIntoRequestBufferCBCalled);
        CHECK(rval.isObject());
        RootedObject promise(cx, &rval.toObject());
        CHECK(IsPromiseObject(promise));
        CHECK(GetPromiseState(promise) == PromiseState::Pending);

        size_t length = sizeof(test_buffer_data);
        ReadableStreamUpdateDataAvailableFromSource(cx, stream, length);

        CHECK(writeIntoRequestBufferCBCalled);
        CHECK(GetPromiseState(promise) == PromiseState::Fulfilled);
        RootedValue iterVal(cx);
        bool done;
        if (!GetIterResult(cx, promise, &iterVal, &done))
            return false;

        CHECK(!done);
        RootedObject chunk(cx, &iterVal.toObject());
        CHECK(JS_IsUint8Array(chunk));

        {
            JS::AutoCheckCannotGC noGC(cx);
            bool dummy;
            void* buffer = JS_GetArrayBufferViewData(chunk, &dummy, noGC);
            CHECK(!memcmp(buffer, test_buffer_data, writtenLength));
        }

        dataRequestCBCalled = false;
        writeIntoRequestBufferCBCalled = false;
        EVAL(evalSrc2, &rval);
        CHECK(dataRequestCBCalled);
        CHECK(!writeIntoRequestBufferCBCalled);

        return true;
    }

    bool readWithDataAvailable(const char* evalSrc, uint32_t writtenLength) {
        ResetCallbacks();
        definePrint();

        RootedObject stream(cx, NewExternalSourceStream(cx, &test_buffer_data, &DataRequestCB,
                                                        &WriteIntoRequestBufferCB, &CancelStreamCB,
                                                        &StreamClosedCB, &StreamErroredCB,
                                                        &FinalizeStreamCB));
        CHECK(stream);
        void* underlyingSource;
        CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
        CHECK(underlyingSource == &test_buffer_data);
        CHECK(ReadableStreamIsLocked(stream));
        ReadableStreamReleaseExternalUnderlyingSource(stream);

        size_t length = sizeof(test_buffer_data);
        ReadableStreamUpdateDataAvailableFromSource(cx, stream, length);

        RootedValue streamVal(cx, ObjectValue(*stream));
        CHECK(JS_SetProperty(cx, global, "stream", streamVal));

        RootedValue rval(cx);
        EVAL(evalSrc, &rval);
        CHECK(writeIntoRequestBufferCBCalled);
        CHECK(rval.isObject());
        RootedObject promise(cx, &rval.toObject());
        CHECK(IsPromiseObject(promise));
        CHECK(GetPromiseState(promise) == PromiseState::Fulfilled);
        RootedValue iterVal(cx);
        bool done;
        if (!GetIterResult(cx, promise, &iterVal, &done))
            return false;

        CHECK(!done);
        RootedObject chunk(cx, &iterVal.toObject());
        CHECK(JS_IsUint8Array(chunk));

        {
            JS::AutoCheckCannotGC noGC(cx);
            bool dummy;
            void* buffer = JS_GetArrayBufferViewData(chunk, &dummy, noGC);
            CHECK(!memcmp(buffer, test_buffer_data, writtenLength));
        }

        return true;
    }
};

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable)
{
    return readWithoutDataAvailable("r = stream.getReader(); r.read()", "r.read()", sizeof(test_buffer_data));
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable)

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceCloseWithPendingRead)
{
    CHECK(readWithoutDataAvailable("r = stream.getReader(); request0 = r.read(); "
                                   "request1 = r.read(); request0", "r.read()",
                                   sizeof(test_buffer_data)));

    RootedValue val(cx);
    CHECK(JS_GetProperty(cx, global, "request1", &val));
    CHECK(val.isObject());
    RootedObject request(cx, &val.toObject());
    CHECK(IsPromiseObject(request));
    CHECK(GetPromiseState(request) == PromiseState::Pending);

    CHECK(JS_GetProperty(cx, global, "stream", &val));
    RootedObject stream(cx, &val.toObject());
    ReadableStreamClose(cx, stream);

    val = GetPromiseResult(request);
    MOZ_ASSERT(val.isObject());
    RootedObject result(cx, &val.toObject());

    JS_GetProperty(cx, result, "done", &val);
    CHECK(val.isBoolean());
    CHECK(val.toBoolean() == true);

    JS_GetProperty(cx, result, "value", &val);
    CHECK(val.isUndefined());
    return true;
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceCloseWithPendingRead)

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceReadDefaultWithDataAvailable)
{
    return readWithDataAvailable("r = stream.getReader(); r.read()", sizeof(test_buffer_data));
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadDefaultWithDataAvailable)

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceReadBYOBWithoutDataAvailable)
{
    return readWithoutDataAvailable("r = stream.getReader({mode: 'byob'}); r.read(new Uint8Array(63))", "r.read(new Uint8Array(10))", 10);
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadBYOBWithoutDataAvailable)

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceReadBYOBWithDataAvailable)
{
    return readWithDataAvailable("r = stream.getReader({mode: 'byob'}); r.read(new Uint8Array(10))", 10);
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadBYOBWithDataAvailable)
