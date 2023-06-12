# Implementing specifications using WHATWG Streams API

[Streams API](https://streams.spec.whatwg.org/) is [a modern way to produce and consume data progressively and asynchronously](https://developer.mozilla.org/en-US/docs/Web/API/Streams_API). Multiple specifications are starting to use it, namely [Fetch](https://fetch.spec.whatwg.org/), [File Stream](https://fs.spec.whatwg.org/), [WebTransport](https://w3c.github.io/webtransport/), and so on. This documentation will briefly explain how to implement such specifications in Gecko.

## Calling functions on stream objects

You can mostly follow the steps in a given spec as-is, as the implementation in Gecko is deliberately written in a way that a given spec prose can match 1:1 to a function call. Let's say the spec says:

> [Enqueue](https://streams.spec.whatwg.org/#readablestream-enqueue) `view` into `stream`.

The prose can be written in C++ as:

```cpp
stream->EnqueueNative(cx, view, rv);
```

Note that the function name ends with `Native` to disambiguate itself from the Web IDL `enqueue` method. See the [list below](#mapping-spec-terms-to-functions) for the complete mapping between spec terms and functions.

## Creating a stream

The stream creation can be generally done by calling `CreateNative()`. You may need to call something else if the spec:

* Wants a byte stream and uses the term "Set up with byte reading support". In that case you need to call `ByteNative` variant.
* Defines a new interface that inherits the base stream interfaces. In this case you need to define a subclass and call `SetUpNative()` inside its init method.
   * To make the cycle collection happy, you need to pass `HoldDropJSObjectsCaller::Explicit` to the superclass constructor and call `mozilla::HoldJSObjects(this)`/`mozilla::DropJSObjects(this)` respectively in the constructor/destructor.

Both `CreateNative()`/`SetUpNative()` functions require an argument to implement custom algorithms for callbacks, whose corresponding spec phrases could be:

> 1. Let `readable` be a [new](https://webidl.spec.whatwg.org/#new) [`ReadableStream`](https://streams.spec.whatwg.org/#readablestream).
> 1. Let `pullAlgorithm` be the following steps:
>    1. (...)
> 1. Set up `stream` with `pullAlgorithm` set to `pullAlgorithm`.

This can roughly translate to the following C++:

```cpp
class MySourceAlgorithms : UnderlyingSourceAlgorithmsWrapper {
   already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController,
      ErrorResult& aRv) override;
};

already_AddRefed<ReadableStream> CreateMyReadableStream(
   JSContext* aCx, nsIGlobalObject* aGlobal, ErrorResult& aRv) {
   // Step 2: Let pullAlgorithm be the following steps:
   auto algorithms = MakeRefPtr<MySourceAlgorithms>();

   // Step 1: Let readable be a new ReadableStream.
   // Step 3: Set up stream with pullAlgorithm set to pullAlgorithm.
   RefPtr<ReadableStream> readable = ReadableStream::CreateNative(
      aCx,
      aGlobal,
      *algorithms,
      /* aHighWaterMark */ Nothing(),
      /* aSizeAlgorithm */ nullptr,
      aRv
   );
}
```

Note that the `new ReadableStream()` and "Set up" steps are done together inside `CreateNative()` for convenience. For subclasses this needs to be split again:

```cpp
class MyReadableStream : public ReadableStream {
 public:
   MyReadableStream(nsIGlobalObject* aGlobal)
      : ReadableStream(aGlobal, ReadableStream::HoldDropJSObjectsCaller::Explicit) {
      mozilla::HoldJSObjects(this);
   }

   ~MyReadableStream() {
      mozilla::DropJSObjects(this);
   }

   void Init(ErrorResult& aRv) {
      // Step 2: Let pullAlgorithm be the following steps:
      auto algorithms = MakeRefPtr<MySourceAlgorithms>();

      // Step 3: Set up stream with pullAlgorithm set to pullAlgorithm.
      //
      // NOTE:
      // For now there's no SetUpNative but only SetUpByteNative.
      // File a bug on DOM: Streams if you need to create a subclass
      // for non-byte ReadableStream.
      SetUpNative(aCx, *algorithms, Nothing(), nullptr, aRv);
   }
}
```

After creating the stream with the algorithms, the rough flow will look like this:

```{mermaid}
sequenceDiagram
   JavaScript->>ReadableStream: await reader.read()
   ReadableStream->>UnderlyingSourceAlgorithmsWrapper: PullCallback()
   UnderlyingSourceAlgorithmsWrapper->>(Data source): (implementation detail)
   NOTE left of (Data source): (Can be file IO, network IO, etc.)
   (Data source)->>UnderlyingSourceAlgorithmsWrapper: (notifies back)
   UnderlyingSourceAlgorithmsWrapper->>ReadableStream: EnqueueNative()
   ReadableStream->>JavaScript: Resolves reader.read()
```

### Implementing the callbacks

As the flow says, the real implementation will be done inside the algorithms, in this case PullCallbackImpl(). Let's say there's a spec term:

> 1. Let `pullAlgorithm` be the following steps:
>    1. [Enqueue](https://streams.spec.whatwg.org/#readablestream-enqueue) a JavaScript string value "Hello Fox!".

This can translate to the following C++:

```cpp
class MySourceAlgorithms : UnderlyingSourceAlgorithmsWrapper {
   // Step 1: Let `pullAlgorithm` be the following steps:
   already_AddRefed<Promise> PullCallbackImpl(
      JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
      RefPtr<ReadableStream> stream = aController.Stream();

      // Step 1.1: Enqueue a JavaScript string value "Hello Fox!".
      JS::Rooted<JSString*> hello(aCx, JS_NewStringCopyZ(aCx, "Hello Fox!"));
      stream->EnqueueNative(aCx, JS::StringValue(hello), aRv);

      // Return a promise if the task is asynchronous, or nullptr if not.
      return nullptr;

      // NOTE:
      // Please don't use aController directly, as it's more for JavaScript.
      // The *Native() functions are safer with additional assertions and more
      // automatic state management.
      // Please file a bug if there's no *Native() function that fulfills your need.
      // In the future this function should receive a ReadableStream instead.

      // Also note that you'll need to touch JS APIs frequently as the functions
      // often expect JS::Value.
   };
};
```

Note that `PullCallbackImpl` returns a promise. The function will not be called again until the promise resolves. The call sequence would be roughly look like the following with repeated read requests:

1. `await read()` from JS
1. `PullCallbackImpl()` call, which returns a Promise
1. The second `await read()` from JS
1. (Time flies)
1. The promise resolves
1. The second `PullCallbackImpl()` call

The same applies to write and transform callbacks in `WritableStream` and `TransformStream`, except they use `UnderlyingSinkAlgorithmsWrapper` and `TransformerAlgorithmsWrapper` respectively.

## Exposing existing XPCOM streams as WHATWG Streams

You may simply want to expose an existing XPCOM stream to JavaScript without any more customization. Fortunately there are some helper functions for this. You can use:

* `InputToReadableStreamAlgorithms` to send data from nsIAsyncInputStream to ReadableStream
* `WritableStreamToOutputAlgorithms` to receive data from WritableStream to nsIAsyncOutputStream

The usage would look like the following:

```cpp
// For nsIAsyncInputStream:
already_AddRefed<ReadableStream> ConvertInputStreamToReadableStream(
   JSContext* aCx, nsIGlobalObject* aGlobal, nsIAsyncInputStream* aInput,
   ErrorResult& aRv) {
   auto algorithms = MakeRefPtr<InputToReadableStreamAlgorithms>(
         stream->GetParentObject(), aInput);
   return do_AddRef(ReadableStream::CreateNative(aCx, aGlobal, *algorithms,
                                                 Nothing(), nullptr, aRv));
}

// For nsIAsyncOutputStream
already_AddRefed<ReadableStream> ConvertOutputStreamToWritableStream(
   JSContext* aCx, nsIGlobalObject* aGlobal, nsIAsyncOutputStream* aInput,
   ErrorResult& aRv) {
   auto algorithms = MakeRefPtr<WritableStreamToOutputAlgorithms>(
         stream->GetParentObject(), aInput);
   return do_AddRef(WritableStream::CreateNative(aCx, aGlobal, *algorithms,
                                                 Nothing(), nullptr, aRv));
}
```

## Mapping spec terms to functions

1. [ReadableStream](https://streams.spec.whatwg.org/#other-specs-rs)
   * [Set up](https://streams.spec.whatwg.org/#readablestream-set-up): [`CreateNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#132)
   * [Set up with byte reading support](https://streams.spec.whatwg.org/#readablestream-set-up-with-byte-reading-support):
      - [`CreateByteNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#143): You can call this when the spec uses the term with `new ReadableStream`.
      - [`SetUpByteNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#150): You need to use this instead when the spec uses the term with a subclass of `ReadableStream`. Call this inside the constructor of the subclass.
   * [Close](https://streams.spec.whatwg.org/#readablestream-close): [`CloseNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#160)
   * [Error](https://streams.spec.whatwg.org/#readablestream-error): [`ErrorNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#163)
   * [Enqueue](https://streams.spec.whatwg.org/#readablestream-enqueue): [`EnqueueNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#167)
   * [Get a reader](https://streams.spec.whatwg.org/#readablestream-get-a-reader): [`GetReader()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStream.h#177)
      * [Read a chunk](https://streams.spec.whatwg.org/#readablestreamdefaultreader-read-a-chunk) on reader: [`ReadChunk()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/ReadableStreamDefaultReader.h#81) on ReadableStreamDefaultReader
2. [WritableStream](https://streams.spec.whatwg.org/#other-specs-ws)
   * [Set up](https://streams.spec.whatwg.org/#writablestream-set-up):
      - [`CreateNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/WritableStream.h#182): You can call this when the spec uses the term with `new WritableStream`.
      - [`SetUpNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/WritableStream.h#174): You need to use this instead when the spec uses the term with a subclass of `WritableStream`. Call this inside the constructor of the subclass.
   * [Error](https://streams.spec.whatwg.org/#writablestream-error): [`ErrorNative()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/WritableStream.h#192)
3. [TransformStream](https://streams.spec.whatwg.org/#other-specs-ts): For now this just uses the functions in TransfromStreamDefaultController, which will be provided as an argument of transform or flush algorithms.
   * [Enqueue](https://streams.spec.whatwg.org/#transformstream-enqueue): [`Enqueue()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/TransformStreamDefaultController.h#47) on TransformStreamDefaultController
   * [Terminate](https://streams.spec.whatwg.org/#transformstream-terminate): [`Terminate()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/TransformStreamDefaultController.h#51) on TransformStreamDefaultController
   * [Error](https://streams.spec.whatwg.org/#transformstream-error): [`Error()`](https://searchfox.org/mozilla-central/rev/31f5847a4494b3646edabbdd7ea39cb88509afe2/dom/streams/TransformStreamDefaultController.h#49) on TransformStreamDefaultController

The mapping is only implemented on demand and does not cover every function in the spec. Please file a bug on [DOM: Streams](https://bugzilla.mozilla.org/describecomponents.cgi?product=Core&component=DOM%3A%20Streams#DOM%3A%20Streams) component in Bugzilla if you need something that is missing here.
