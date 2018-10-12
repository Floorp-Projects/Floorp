/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"

#include "jsapi-tests/tests.h"

using namespace JS;

struct StubExternalUnderlyingSource {
    void* buffer;
};

char testBufferData[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

StubExternalUnderlyingSource stubExternalUnderlyingSource = {
    testBufferData
};

static_assert(MOZ_ALIGNOF(StubExternalUnderlyingSource) > 1,
              "UnderlyingSource pointers must not have the low bit set");

static JSObject*
NewDefaultStream(JSContext* cx, HandleObject source = nullptr, HandleFunction size = nullptr,
                 double highWaterMark = 1, HandleObject proto = nullptr)
{
    RootedObject stream(cx, NewReadableDefaultStreamObject(cx, source, size, highWaterMark,
                                                           proto));
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
    writeIntoRequestBufferCBCalled = true;

    MOZ_ASSERT(underlyingSource == &stubExternalUnderlyingSource);
    MOZ_ASSERT(stubExternalUnderlyingSource.buffer == testBufferData);
    MOZ_ASSERT(length <= sizeof(testBufferData));
    memcpy(buffer, testBufferData, length);
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
    if (!JS_HasProperty(cx, iterResult, "value", &found)) {
        return false;
    }
    MOZ_ASSERT(found);
    if (!JS_HasProperty(cx, iterResult, "done", &found)) {
        return false;
    }
    MOZ_ASSERT(found);

    RootedValue doneVal(cx);
    if (!JS_GetProperty(cx, iterResult, "value", value)) {
        return false;
    }
    if (!JS_GetProperty(cx, iterResult, "done", &doneVal)) {
        return false;
    }

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

struct StreamTestFixture : public JSAPITest
{
    virtual ~StreamTestFixture() {}
};

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_NewReadableStream)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    ReadableStreamMode mode;
    CHECK(ReadableStreamGetMode(cx, stream, &mode));
    CHECK(mode == ReadableStreamMode::Default);
    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_NewReadableStream)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetReaderDefault)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);
    CHECK(IsReadableStreamDefaultReader(reader));
    bool locked;
    CHECK(ReadableStreamIsLocked(cx, stream, &locked));
    CHECK(locked);
    bool closed;
    CHECK(ReadableStreamReaderIsClosed(cx, reader, &closed));
    CHECK(!closed);

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetReaderDefault)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamTee)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject leftStream(cx);
    RootedObject rightStream(cx);
    CHECK(ReadableStreamTee(cx, stream, &leftStream, &rightStream));
    bool locked;
    CHECK(ReadableStreamIsLocked(cx, stream, &locked));
    CHECK(locked);
    CHECK(leftStream);
    CHECK(IsReadableStream(leftStream));
    CHECK(rightStream);
    CHECK(IsReadableStream(rightStream));

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamTee)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamEnqueue)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject chunk(cx, JS_NewPlainObject(cx));
    CHECK(chunk);
    RootedValue chunkVal(cx, ObjectValue(*chunk));
    CHECK(ReadableStreamEnqueue(cx, stream, chunkVal));

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamEnqueue)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamDefaultReaderRead)
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
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamDefaultReaderRead)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamDefaultReaderClose)
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
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamDefaultReaderClose)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamDefaultReaderError)
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

    bool locked;
    CHECK(ReadableStreamIsLocked(cx, stream, &locked));
    CHECK(locked);
    bool readable;
    CHECK(ReadableStreamIsReadable(cx, stream, &readable));
    CHECK(readable);
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
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamDefaultReaderError)

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

static JSObject*
NewExternalSourceStream(JSContext* cx)
{
    return NewExternalSourceStream(cx,
                                   &stubExternalUnderlyingSource,
                                   &DataRequestCB,
                                   &WriteIntoRequestBufferCB,
                                   &CancelStreamCB,
                                   &StreamClosedCB,
                                   &StreamErroredCB,
                                   &FinalizeStreamCB);
}

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_CreateReadableByteStreamWithExternalSource)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx));
    CHECK(stream);
    ReadableStreamMode mode;
    CHECK(ReadableStreamGetMode(cx, stream, &mode));
    CHECK(mode == ReadableStreamMode::ExternalSource);
    void* underlyingSource;
    CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
    CHECK(underlyingSource == &stubExternalUnderlyingSource);
    bool locked;
    CHECK(ReadableStreamIsLocked(cx, stream, &locked));
    CHECK(locked);
    CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_CreateReadableByteStreamWithExternalSource)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ExternalSourceCancel)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx));
    CHECK(stream);
    RootedValue reason(cx, Int32Value(42));
    CHECK(ReadableStreamCancel(cx, stream, reason));
    CHECK(cancelStreamCBCalled);
    CHECK(cancelStreamReason == reason);

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ExternalSourceCancel)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ExternalSourceGetReader)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx));
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
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ExternalSourceGetReader)

struct ReadFromExternalSourceFixture : public StreamTestFixture
{
    virtual ~ReadFromExternalSourceFixture() {}

    bool readWithoutDataAvailable(const char* evalSrc, const char* evalSrc2,
                                  uint32_t writtenLength)
    {
        ResetCallbacks();
        definePrint();

        RootedObject stream(cx, NewExternalSourceStream(cx));
        CHECK(stream);
        js::RunJobs(cx);
        void* underlyingSource;
        CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
        CHECK(underlyingSource == &stubExternalUnderlyingSource);
        bool locked;
        CHECK(ReadableStreamIsLocked(cx, stream, &locked));
        CHECK(locked);
        CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

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

        size_t length = sizeof(testBufferData);
        ReadableStreamUpdateDataAvailableFromSource(cx, stream, length);

        CHECK(writeIntoRequestBufferCBCalled);
        CHECK(GetPromiseState(promise) == PromiseState::Fulfilled);
        RootedValue iterVal(cx);
        bool done;
        if (!GetIterResult(cx, promise, &iterVal, &done)) {
            return false;
        }

        CHECK(!done);
        RootedObject chunk(cx, &iterVal.toObject());
        CHECK(JS_IsUint8Array(chunk));

        {
            JS::AutoCheckCannotGC noGC(cx);
            bool dummy;
            void* buffer = JS_GetArrayBufferViewData(chunk, &dummy, noGC);
            CHECK(!memcmp(buffer, testBufferData, writtenLength));
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

        RootedObject stream(cx, NewExternalSourceStream(cx));
        CHECK(stream);
        void* underlyingSource;
        CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
        CHECK(underlyingSource == &stubExternalUnderlyingSource);
        bool locked;
        CHECK(ReadableStreamIsLocked(cx, stream, &locked));
        CHECK(locked);
        CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

        size_t length = sizeof(testBufferData);
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
        if (!GetIterResult(cx, promise, &iterVal, &done)) {
            return false;
        }

        CHECK(!done);
        RootedObject chunk(cx, &iterVal.toObject());
        CHECK(JS_IsUint8Array(chunk));

        {
            JS::AutoCheckCannotGC noGC(cx);
            bool dummy;
            void* buffer = JS_GetArrayBufferViewData(chunk, &dummy, noGC);
            CHECK(!memcmp(buffer, testBufferData, writtenLength));
        }

        return true;
    }
};

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable)
{
    return readWithoutDataAvailable("r = stream.getReader(); r.read()", "r.read()", sizeof(testBufferData));
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable)

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceCloseWithPendingRead)
{
    CHECK(readWithoutDataAvailable("r = stream.getReader(); request0 = r.read(); "
                                   "request1 = r.read(); request0", "r.read()",
                                   sizeof(testBufferData)));

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
    return readWithDataAvailable("r = stream.getReader(); r.read()", sizeof(testBufferData));
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadDefaultWithDataAvailable)

// Cross-global tests:
BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamOtherGlobalDefaultReaderRead)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);

    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        RootedObject reader(cx, ReadableStreamGetReader(cx, stream,
                                                        ReadableStreamReaderMode::Default));
        CHECK(reader);

        RootedObject request(cx, ReadableStreamDefaultReaderRead(cx, reader));
        CHECK(request);
        CHECK(IsPromiseObject(request));
        CHECK(!js::IsWrapper(request));
        CHECK(GetPromiseState(request) == PromiseState::Pending);

        RootedObject chunk(cx, JS_NewPlainObject(cx));
        CHECK(chunk);
        RootedValue chunkVal(cx, ObjectValue(*chunk));
        CHECK(ReadableStreamEnqueue(cx, stream, chunkVal));

        CHECK(GetReadChunk(cx, request) == chunk);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamOtherGlobalDefaultReaderRead)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetEmbeddingFlags)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    uint8_t flags;
    CHECK(ReadableStreamGetEmbeddingFlags(cx, stream, &flags));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        uint8_t flags;
        CHECK(ReadableStreamGetEmbeddingFlags(cx, stream, &flags));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetEmbeddingFlags)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetExternalUnderlyingSource)
{
    ResetCallbacks();

    RootedObject stream(cx, NewExternalSourceStream(cx));
    CHECK(stream);
    void* source;
    CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &source));
    CHECK(source == &stubExternalUnderlyingSource);
    CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        void* source;
        CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &source));
        CHECK(source == &stubExternalUnderlyingSource);
        CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetExternalUnderlyingSource)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamUpdateDataAvailableFromSource)
{
    RootedObject stream(cx, NewExternalSourceStream(cx));
    CHECK(stream);
    CHECK(ReadableStreamUpdateDataAvailableFromSource(cx, stream, 0));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamUpdateDataAvailableFromSource(cx, stream, 1));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamUpdateDataAvailableFromSource)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_IsReadableStream)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    CHECK(IsReadableStream(stream));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(IsReadableStream(stream));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_IsReadableStream)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetMode)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    ReadableStreamMode mode;
    CHECK(ReadableStreamGetMode(cx, stream, &mode));
    CHECK(mode == ReadableStreamMode::Default);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamGetMode(cx, stream, &mode));
        CHECK(mode == ReadableStreamMode::Default);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetMode)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamIsReadable)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    bool result;
    CHECK(ReadableStreamIsReadable(cx, stream, &result));
    CHECK(result);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamIsReadable(cx, stream, &result));
        CHECK(result);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamIsReadable)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamIsLocked)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    bool result;
    CHECK(ReadableStreamIsLocked(cx, stream, &result));
    CHECK_EQUAL(result, false);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamIsLocked(cx, stream, &result));
        CHECK_EQUAL(result, false);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamIsLocked)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamIsDisturbed)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    bool result;
    CHECK(ReadableStreamIsDisturbed(cx, stream, &result));
    CHECK_EQUAL(result, false);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamIsDisturbed(cx, stream, &result));
        CHECK_EQUAL(result, false);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamIsDisturbed)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamCancel)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);


    RootedValue reason(cx);
    JSObject* callResult = ReadableStreamCancel(cx, stream, reason);
    CHECK(callResult);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        RootedValue reason(cx);
        JSObject* callResult = ReadableStreamCancel(cx, stream, reason);
        CHECK(callResult);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamCancel)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetReader)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);


    RootedObject reader(cx);
    reader = ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default);
    CHECK(reader);
    CHECK(IsReadableStreamDefaultReader(reader));
    CHECK(ReadableStreamReaderReleaseLock(cx, reader));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        JSObject* callResult = ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default);
        CHECK(callResult);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetReader)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamTee_CrossCompartment)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);


    RootedObject branch1Stream(cx);
    RootedObject branch2Stream(cx);
    CHECK(ReadableStreamTee(cx, stream, &branch1Stream, &branch2Stream));
    CHECK(IsReadableStream(branch1Stream));
    CHECK(IsReadableStream(branch2Stream));
    stream = branch1Stream;

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamTee(cx, stream, &branch1Stream, &branch2Stream));
        CHECK(IsReadableStream(branch1Stream));
        CHECK(IsReadableStream(branch2Stream));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamTee_CrossCompartment)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetDesiredSize)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    bool hasValue;
    double value;
    CHECK(ReadableStreamGetDesiredSize(cx, stream, &hasValue, &value));
    CHECK_EQUAL(hasValue, true);
    CHECK_EQUAL(value, 1.0);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        hasValue = false;
        value = 0;
        CHECK(ReadableStreamGetDesiredSize(cx, stream, &hasValue, &value));
        CHECK_EQUAL(hasValue, true);
        CHECK_EQUAL(value, 1.0);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetDesiredSize)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamClose)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    CHECK(ReadableStreamClose(cx, stream));

    stream = NewDefaultStream(cx);
    CHECK(stream);
    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        CHECK(ReadableStreamClose(cx, stream));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamClose)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamEnqueue_CrossCompartment)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedValue chunk(cx);
    CHECK(ReadableStreamEnqueue(cx, stream, chunk));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        RootedValue chunk(cx);
        CHECK(ReadableStreamEnqueue(cx, stream, chunk));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamEnqueue_CrossCompartment)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamError)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedValue error(cx);
    CHECK(ReadableStreamError(cx, stream, error));

    stream = NewDefaultStream(cx);
    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &stream));
        RootedValue error(cx);
        CHECK(ReadableStreamError(cx, stream, error));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamError)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_IsReadableStreamReader)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);
    CHECK(IsReadableStreamReader(reader));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &reader));
        CHECK(IsReadableStreamReader(reader));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_IsReadableStreamReader)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_IsReadableStreamDefaultReader)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(IsReadableStreamDefaultReader(reader));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &reader));
        CHECK(IsReadableStreamDefaultReader(reader));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_IsReadableStreamDefaultReader)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamReaderIsClosed)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);

    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    bool result;
    CHECK(ReadableStreamReaderIsClosed(cx, reader, &result));
    CHECK_EQUAL(result, false);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &reader));
        bool result;
        CHECK(ReadableStreamReaderIsClosed(cx, reader, &result));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamReaderIsClosed)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamReaderCancel)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    RootedValue reason(cx);
    CHECK(ReadableStreamReaderCancel(cx, reader, reason));

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &reader));
        RootedValue reason(cx);
        CHECK(ReadableStreamReaderCancel(cx, reader, reason));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamReaderCancel)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamReaderReleaseLock)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    CHECK(reader);
    CHECK(ReadableStreamReaderReleaseLock(cx, reader));

    // Repeat the test cross-compartment. This creates a new reader, since
    // releasing the lock above deactivated the first reader.
    reader = ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default);
    CHECK(reader);
    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &reader));
        CHECK(ReadableStreamReaderReleaseLock(cx, reader));
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamReaderReleaseLock)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamDefaultReaderRead_CrossCompartment)
{
    RootedObject stream(cx, NewDefaultStream(cx));
    CHECK(stream);
    RootedObject reader(cx, ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default));
    JSObject* callResult = ReadableStreamDefaultReaderRead(cx, reader);
    CHECK(callResult);

    RootedObject otherGlobal(cx, createGlobal());
    CHECK(otherGlobal);
    {
        JSAutoRealm ar(cx, otherGlobal);
        CHECK(JS_WrapObject(cx, &reader));
        JSObject* callResult = ReadableStreamDefaultReaderRead(cx, reader);
        CHECK(callResult);
    }

    return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamDefaultReaderRead_CrossCompartment)

