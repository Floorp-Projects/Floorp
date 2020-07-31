/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/experimental/TypedData.h"  // JS_GetArrayBufferViewData, JS_IsUint8Array
#include "js/Stream.h"
#include "jsapi-tests/tests.h"

using namespace JS;

char testBufferData[] =
    "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

struct StubExternalUnderlyingSource
    : public JS::ReadableStreamUnderlyingSource {
  void* buffer = testBufferData;
  bool dataRequestCBCalled = false;
  bool writeIntoRequestBufferCBCalled = false;
  bool cancelStreamCBCalled = false;
  Value cancelStreamReason;
  bool streamClosedCBCalled = false;
  Value streamClosedReason;
  bool streamErroredCBCalled = false;
  Value streamErroredReason;
  bool finalizeStreamCBCalled = false;
  void* finalizedStreamUnderlyingSource;

  static StubExternalUnderlyingSource instance;

  void requestData(JSContext* cx, HandleObject stream,
                   size_t desiredSize) override {
    js::AssertSameCompartment(cx, stream);
    MOZ_RELEASE_ASSERT(!dataRequestCBCalled, "Invalid test setup");
    dataRequestCBCalled = true;
  }

  void writeIntoReadRequestBuffer(JSContext* cx, HandleObject stream,
                                  void* buffer, size_t length,
                                  size_t* bytesWritten) override {
    js::AssertSameCompartment(cx, stream);
    MOZ_RELEASE_ASSERT(!writeIntoRequestBufferCBCalled, "Invalid test setup");
    writeIntoRequestBufferCBCalled = true;

    MOZ_RELEASE_ASSERT(this == &StubExternalUnderlyingSource::instance);
    MOZ_RELEASE_ASSERT(StubExternalUnderlyingSource::instance.buffer ==
                       testBufferData);
    MOZ_RELEASE_ASSERT(length <= sizeof(testBufferData));
    memcpy(buffer, testBufferData, length);
    *bytesWritten = length;
  }

  Value cancel(JSContext* cx, HandleObject stream,
               HandleValue reason) override {
    js::AssertSameCompartment(cx, stream);
    js::AssertSameCompartment(cx, reason);
    MOZ_RELEASE_ASSERT(!cancelStreamCBCalled, "Invalid test setup");
    cancelStreamCBCalled = true;
    cancelStreamReason = reason;
    return reason;
  }

  void onClosed(JSContext* cx, HandleObject stream) override {
    js::AssertSameCompartment(cx, stream);
    MOZ_RELEASE_ASSERT(!streamClosedCBCalled, "Invalid test setup");
    streamClosedCBCalled = true;
  }

  void onErrored(JSContext* cx, HandleObject stream,
                 HandleValue reason) override {
    js::AssertSameCompartment(cx, stream);
    js::AssertSameCompartment(cx, reason);
    MOZ_RELEASE_ASSERT(!streamErroredCBCalled, "Invalid test setup");
    streamErroredCBCalled = true;
    streamErroredReason = reason;
  }

  void finalize() override {
    MOZ_RELEASE_ASSERT(!finalizeStreamCBCalled, "Invalid test setup");
    finalizeStreamCBCalled = true;
    finalizedStreamUnderlyingSource = this;
  }

  void reset() {
    dataRequestCBCalled = false;
    writeIntoRequestBufferCBCalled = false;
    cancelStreamReason = UndefinedValue();
    cancelStreamCBCalled = false;
    streamClosedCBCalled = false;
    streamErroredCBCalled = false;
    finalizeStreamCBCalled = false;
  }
};

StubExternalUnderlyingSource StubExternalUnderlyingSource::instance;

static_assert(MOZ_ALIGNOF(StubExternalUnderlyingSource) > 1,
              "UnderlyingSource pointers must not have the low bit set");

static JSObject* NewDefaultStream(JSContext* cx, HandleObject source = nullptr,
                                  HandleFunction size = nullptr,
                                  double highWaterMark = 1,
                                  HandleObject proto = nullptr) {
  RootedObject stream(cx, NewReadableDefaultStreamObject(cx, source, size,
                                                         highWaterMark, proto));
  if (stream) {
    MOZ_RELEASE_ASSERT(IsReadableStream(stream));
  }
  return stream;
}

static bool GetIterResult(JSContext* cx, HandleObject promise,
                          MutableHandleValue value, bool* done) {
  RootedObject iterResult(cx, &GetPromiseResult(promise).toObject());

  bool found;
  if (!JS_HasProperty(cx, iterResult, "value", &found)) {
    return false;
  }
  MOZ_RELEASE_ASSERT(found);
  if (!JS_HasProperty(cx, iterResult, "done", &found)) {
    return false;
  }
  MOZ_RELEASE_ASSERT(found);

  RootedValue doneVal(cx);
  if (!JS_GetProperty(cx, iterResult, "value", value)) {
    return false;
  }
  if (!JS_GetProperty(cx, iterResult, "done", &doneVal)) {
    return false;
  }

  *done = doneVal.toBoolean();
  if (*done) {
    MOZ_RELEASE_ASSERT(value.isUndefined());
  }

  return true;
}

static JSObject* GetReadChunk(JSContext* cx, HandleObject readRequest) {
  MOZ_RELEASE_ASSERT(GetPromiseState(readRequest) == PromiseState::Fulfilled);
  RootedValue resultVal(cx, GetPromiseResult(readRequest));
  MOZ_RELEASE_ASSERT(resultVal.isObject());
  RootedObject result(cx, &resultVal.toObject());
  RootedValue chunkVal(cx);
  JS_GetProperty(cx, result, "value", &chunkVal);
  return &chunkVal.toObject();
}

struct StreamTestFixture : public JSAPITest {
  virtual ~StreamTestFixture() {}
};

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_NewReadableStream) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  ReadableStreamMode mode;
  CHECK(ReadableStreamGetMode(cx, stream, &mode));
  CHECK(mode == ReadableStreamMode::Default);
  return true;
}
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_NewReadableStream)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetReaderDefault) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);

  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamTee) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamTee)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamEnqueue) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);

  RootedObject chunk(cx, JS_NewPlainObject(cx));
  CHECK(chunk);
  RootedValue chunkVal(cx, ObjectValue(*chunk));
  CHECK(ReadableStreamEnqueue(cx, stream, chunkVal));

  return true;
}
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamEnqueue)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamDefaultReaderRead) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
                   testReadableStream_ReadableStreamDefaultReaderClose) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
  CHECK(!StubExternalUnderlyingSource::instance.streamClosedCBCalled);

  return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamDefaultReaderClose)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamDefaultReaderError) {
  StubExternalUnderlyingSource::instance.reset();
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
  CHECK(!StubExternalUnderlyingSource::instance.streamErroredCBCalled);

  return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamDefaultReaderError)

static JSObject* NewExternalSourceStream(
    JSContext* cx, ReadableStreamUnderlyingSource* source) {
  RootedObject stream(cx, NewReadableExternalSourceStreamObject(cx, source));
  if (stream) {
    MOZ_RELEASE_ASSERT(IsReadableStream(stream));
  }
  return stream;
}

static JSObject* NewExternalSourceStream(JSContext* cx) {
  return NewExternalSourceStream(cx, &StubExternalUnderlyingSource::instance);
}

BEGIN_FIXTURE_TEST(
    StreamTestFixture,
    testReadableStream_CreateReadableByteStreamWithExternalSource) {
  StubExternalUnderlyingSource::instance.reset();

  RootedObject stream(cx, NewExternalSourceStream(cx));
  CHECK(stream);
  ReadableStreamMode mode;
  CHECK(ReadableStreamGetMode(cx, stream, &mode));
  CHECK(mode == ReadableStreamMode::ExternalSource);
  ReadableStreamUnderlyingSource* underlyingSource;
  CHECK(
      ReadableStreamGetExternalUnderlyingSource(cx, stream, &underlyingSource));
  CHECK(underlyingSource == &StubExternalUnderlyingSource::instance);
  bool locked;
  CHECK(ReadableStreamIsLocked(cx, stream, &locked));
  CHECK(locked);
  CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

  return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_CreateReadableByteStreamWithExternalSource)

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_ExternalSourceCancel) {
  StubExternalUnderlyingSource::instance.reset();

  RootedObject stream(cx, NewExternalSourceStream(cx));
  CHECK(stream);
  RootedValue reason(cx, Int32Value(42));
  CHECK(ReadableStreamCancel(cx, stream, reason));
  CHECK(StubExternalUnderlyingSource::instance.cancelStreamCBCalled);
  CHECK(StubExternalUnderlyingSource::instance.cancelStreamReason == reason);

  return true;
}
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ExternalSourceCancel)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ExternalSourceGetReader) {
  StubExternalUnderlyingSource::instance.reset();

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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ExternalSourceGetReader)

enum class CompartmentMode {
  Same,
  Cross,
};

struct ReadFromExternalSourceFixture : public StreamTestFixture {
  virtual ~ReadFromExternalSourceFixture() {}

  // On success, streamGlobal is a global object (not a wrapper)
  // and stream is in the same compartment as cx (it may be a CCW).
  bool createExternalSourceStream(CompartmentMode compartmentMode,
                                  MutableHandleObject streamGlobal,
                                  MutableHandleObject stream) {
    if (compartmentMode == CompartmentMode::Same) {
      streamGlobal.set(global);
      stream.set(NewExternalSourceStream(cx));
      if (!stream) {
        return false;
      }
    } else {
      RootedObject savedGlobal(cx, global);
      streamGlobal.set(createGlobal());
      if (!streamGlobal) {
        return false;
      }
      global = savedGlobal;

      {
        JSAutoRealm ar(cx, streamGlobal);
        stream.set(NewExternalSourceStream(cx));
        if (!stream) {
          return false;
        }
      }
      if (!JS_WrapObject(cx, stream)) {
        return false;
      }
    }
    return true;
  }

  bool readWithoutDataAvailable(CompartmentMode compartmentMode,
                                const char* evalSrc, const char* evalSrc2,
                                uint32_t writtenLength) {
    StubExternalUnderlyingSource::instance.reset();
    definePrint();

    // Create the stream.
    RootedObject streamGlobal(cx);
    RootedObject stream(cx);  // can be a wrapper
    CHECK(createExternalSourceStream(compartmentMode, &streamGlobal, &stream));
    js::RunJobs(cx);

    // GetExternalUnderlyingSource locks the stream.
    ReadableStreamUnderlyingSource* underlyingSource;
    CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream,
                                                    &underlyingSource));
    CHECK(underlyingSource == &StubExternalUnderlyingSource::instance);
    bool locked;
    CHECK(ReadableStreamIsLocked(cx, stream, &locked));
    CHECK(locked);
    CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

    // Run caller-supplied JS code to read from the stream.
    RootedValue streamVal(cx, ObjectValue(*stream));
    CHECK(JS_SetProperty(cx, global, "stream", streamVal));
    RootedValue rval(cx);
    EVAL(evalSrc, &rval);
    CHECK(StubExternalUnderlyingSource::instance.dataRequestCBCalled);
    CHECK(
        !StubExternalUnderlyingSource::instance.writeIntoRequestBufferCBCalled);
    CHECK(rval.isObject());
    RootedObject unwrappedPromise(cx,
                                  js::CheckedUnwrapStatic(&rval.toObject()));
    CHECK(unwrappedPromise);
    CHECK(IsPromiseObject(unwrappedPromise));
    CHECK(GetPromiseState(unwrappedPromise) == PromiseState::Pending);

    // Stream in some data; this resolves the read() result promise.
    size_t length = sizeof(testBufferData);
    CHECK(ReadableStreamUpdateDataAvailableFromSource(cx, stream, length));
    CHECK(
        StubExternalUnderlyingSource::instance.writeIntoRequestBufferCBCalled);
    CHECK(GetPromiseState(unwrappedPromise) == PromiseState::Fulfilled);
    RootedObject chunk(cx);
    {
      JSAutoRealm ar(cx, unwrappedPromise);
      RootedValue iterVal(cx);
      bool done;
      if (!GetIterResult(cx, unwrappedPromise, &iterVal, &done)) {
        return false;
      }
      CHECK(!done);
      chunk = &iterVal.toObject();
    }
    CHECK(JS_WrapObject(cx, &chunk));
    CHECK(JS_IsUint8Array(chunk));

    {
      JS::AutoCheckCannotGC noGC(cx);
      bool dummy;
      void* buffer = JS_GetArrayBufferViewData(chunk, &dummy, noGC);
      CHECK(!memcmp(buffer, testBufferData, writtenLength));
    }

    // Check the callbacks fired by calling read() again.
    StubExternalUnderlyingSource::instance.dataRequestCBCalled = false;
    StubExternalUnderlyingSource::instance.writeIntoRequestBufferCBCalled =
        false;
    EVAL(evalSrc2, &rval);
    CHECK(StubExternalUnderlyingSource::instance.dataRequestCBCalled);
    CHECK(
        !StubExternalUnderlyingSource::instance.writeIntoRequestBufferCBCalled);

    return true;
  }

  bool readWithDataAvailable(CompartmentMode compartmentMode,
                             const char* evalSrc, uint32_t writtenLength) {
    StubExternalUnderlyingSource::instance.reset();
    definePrint();

    // Create a stream.
    RootedObject streamGlobal(cx);
    RootedObject stream(cx);
    CHECK(createExternalSourceStream(compartmentMode, &streamGlobal, &stream));

    // Getting the underlying source locks the stream.
    ReadableStreamUnderlyingSource* underlyingSource;
    CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream,
                                                    &underlyingSource));
    CHECK(underlyingSource == &StubExternalUnderlyingSource::instance);
    bool locked;
    CHECK(ReadableStreamIsLocked(cx, stream, &locked));
    CHECK(locked);
    CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

    // Make some data available.
    size_t length = sizeof(testBufferData);
    CHECK(ReadableStreamUpdateDataAvailableFromSource(cx, stream, length));

    // Read from the stream.
    RootedValue streamVal(cx, ObjectValue(*stream));
    CHECK(JS_SetProperty(cx, global, "stream", streamVal));
    RootedValue rval(cx);
    EVAL(evalSrc, &rval);
    CHECK(
        StubExternalUnderlyingSource::instance.writeIntoRequestBufferCBCalled);
    CHECK(rval.isObject());
    RootedObject unwrappedPromise(cx,
                                  js::CheckedUnwrapStatic(&rval.toObject()));
    CHECK(unwrappedPromise);
    CHECK(IsPromiseObject(unwrappedPromise));
    CHECK(GetPromiseState(unwrappedPromise) == PromiseState::Fulfilled);
    RootedObject chunk(cx);
    {
      JSAutoRealm ar(cx, unwrappedPromise);
      RootedValue iterVal(cx);
      bool done;
      if (!GetIterResult(cx, unwrappedPromise, &iterVal, &done)) {
        return false;
      }
      CHECK(!done);
      chunk = &iterVal.toObject();
    }
    CHECK(JS_WrapObject(cx, &chunk));
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

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable) {
  return readWithoutDataAvailable(CompartmentMode::Same,
                                  "r = stream.getReader(); r.read()",
                                  "r.read()", sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable)

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable_CrossCompartment1) {
  // Scenario 1: The stream and reader are both in the same compartment, but
  // ReadableStreamUpdateDataAvailableFromSource is applied to a wrapper.
  return readWithoutDataAvailable(CompartmentMode::Cross,
                                  "r = stream.getReader(); r.read()",
                                  "r.read()", sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable_CrossCompartment1)

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable_CrossCompartment2) {
  // Scenario 2: The stream and reader are in the same compartment, but a
  // `read` method from another compartment is used on the reader.
  return readWithoutDataAvailable(
      CompartmentMode::Cross,
      "r = stream.getReader(); read = new "
      "ReadableStream({start(){}}).getReader().read; read.call(r)",
      "read.call(r)", sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable_CrossCompartment2)

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable_CrossCompartment3) {
  // Scenario 3: The stream and reader are in different compartments.
  return readWithoutDataAvailable(
      CompartmentMode::Cross,
      "r = ReadableStream.prototype.getReader.call(stream); r.read()",
      "r.read()", sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithoutDataAvailable_CrossCompartment3)

BEGIN_FIXTURE_TEST(ReadFromExternalSourceFixture,
                   testReadableStream_ExternalSourceCloseWithPendingRead) {
  CHECK(readWithoutDataAvailable(CompartmentMode::Same,
                                 "r = stream.getReader(); request0 = r.read(); "
                                 "request1 = r.read(); request0",
                                 "r.read()", sizeof(testBufferData)));

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
  CHECK(val.isObject());
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

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable) {
  return readWithDataAvailable(CompartmentMode::Same,
                               "r = stream.getReader(); r.read()",
                               sizeof(testBufferData));
}
END_FIXTURE_TEST(ReadFromExternalSourceFixture,
                 testReadableStream_ExternalSourceReadDefaultWithDataAvailable)

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable_CrossCompartment1) {
  // Scenario 1: The stream and reader are both in the same compartment, but
  // ReadableStreamUpdateDataAvailableFromSource is applied to a wrapper.
  return readWithDataAvailable(CompartmentMode::Cross,
                               "r = stream.getReader(); r.read()",
                               sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable_CrossCompartment1)

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable_CrossCompartment2) {
  // Scenario 2: The stream and reader are in the same compartment, but a
  // `read` method from another compartment is used on the reader.
  return readWithDataAvailable(
      CompartmentMode::Cross,
      "r = stream.getReader(); read = new "
      "ReadableStream({start(){}}).getReader().read; read.call(r)",
      sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable_CrossCompartment2)

BEGIN_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable_CrossCompartment3) {
  // Scenario 3: The stream and reader are in different compartments.
  return readWithDataAvailable(
      CompartmentMode::Cross,
      "r = ReadableStream.prototype.getReader.call(stream); r.read()",
      sizeof(testBufferData));
}
END_FIXTURE_TEST(
    ReadFromExternalSourceFixture,
    testReadableStream_ExternalSourceReadDefaultWithDataAvailable_CrossCompartment3)

// Cross-global tests:
BEGIN_FIXTURE_TEST(
    StreamTestFixture,
    testReadableStream_ReadableStreamOtherGlobalDefaultReaderRead) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject otherGlobal(cx, createGlobal());
  CHECK(otherGlobal);

  {
    JSAutoRealm ar(cx, otherGlobal);
    CHECK(JS_WrapObject(cx, &stream));
    RootedObject reader(cx, ReadableStreamGetReader(
                                cx, stream, ReadableStreamReaderMode::Default));
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

BEGIN_FIXTURE_TEST(
    StreamTestFixture,
    testReadableStream_ReadableStreamGetExternalUnderlyingSource) {
  StubExternalUnderlyingSource::instance.reset();

  RootedObject stream(cx, NewExternalSourceStream(cx));
  CHECK(stream);
  ReadableStreamUnderlyingSource* source;
  CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &source));
  CHECK(source == &StubExternalUnderlyingSource::instance);
  CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));

  RootedObject otherGlobal(cx, createGlobal());
  CHECK(otherGlobal);
  {
    JSAutoRealm ar(cx, otherGlobal);
    CHECK(JS_WrapObject(cx, &stream));
    ReadableStreamUnderlyingSource* source;
    CHECK(ReadableStreamGetExternalUnderlyingSource(cx, stream, &source));
    CHECK(source == &StubExternalUnderlyingSource::instance);
    CHECK(ReadableStreamReleaseExternalUnderlyingSource(cx, stream));
  }

  return true;
}
END_FIXTURE_TEST(StreamTestFixture,
                 testReadableStream_ReadableStreamGetExternalUnderlyingSource)

BEGIN_FIXTURE_TEST(
    StreamTestFixture,
    testReadableStream_ReadableStreamUpdateDataAvailableFromSource) {
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

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_IsReadableStream) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_IsReadableStream)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetMode) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamGetMode)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamIsReadable) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamIsReadable)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamIsLocked) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamIsLocked)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamIsDisturbed) {
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

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamCancel) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamCancel)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamGetReader) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);

  RootedObject reader(cx);
  reader =
      ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default);
  CHECK(reader);
  CHECK(IsReadableStreamDefaultReader(reader));
  CHECK(ReadableStreamReaderReleaseLock(cx, reader));

  RootedObject otherGlobal(cx, createGlobal());
  CHECK(otherGlobal);
  {
    JSAutoRealm ar(cx, otherGlobal);
    CHECK(JS_WrapObject(cx, &stream));
    JSObject* callResult =
        ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default);
    CHECK(callResult);
  }

  return true;
}
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamGetReader)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamTee_CrossCompartment) {
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
                   testReadableStream_ReadableStreamGetDesiredSize) {
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

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamClose) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamClose)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_ReadableStreamEnqueue_CrossCompartment) {
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

BEGIN_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamError) {
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_ReadableStreamError)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_IsReadableStreamReader) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
END_FIXTURE_TEST(StreamTestFixture, testReadableStream_IsReadableStreamReader)

BEGIN_FIXTURE_TEST(StreamTestFixture,
                   testReadableStream_IsReadableStreamDefaultReader) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
                   testReadableStream_ReadableStreamReaderIsClosed) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);

  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
                   testReadableStream_ReadableStreamReaderCancel) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
                   testReadableStream_ReadableStreamReaderReleaseLock) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
  CHECK(reader);
  CHECK(ReadableStreamReaderReleaseLock(cx, reader));

  // Repeat the test cross-compartment. This creates a new reader, since
  // releasing the lock above deactivated the first reader.
  reader =
      ReadableStreamGetReader(cx, stream, ReadableStreamReaderMode::Default);
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

BEGIN_FIXTURE_TEST(
    StreamTestFixture,
    testReadableStream_ReadableStreamDefaultReaderRead_CrossCompartment) {
  RootedObject stream(cx, NewDefaultStream(cx));
  CHECK(stream);
  RootedObject reader(cx, ReadableStreamGetReader(
                              cx, stream, ReadableStreamReaderMode::Default));
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
END_FIXTURE_TEST(
    StreamTestFixture,
    testReadableStream_ReadableStreamDefaultReaderRead_CrossCompartment)
