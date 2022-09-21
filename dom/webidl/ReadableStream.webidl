[Exposed=*,
//Transferable See Bug 1562065
]
interface ReadableStream {
  [Throws]
  constructor(optional object underlyingSource, optional QueuingStrategy strategy = {});

  readonly attribute boolean locked;

  [NewObject]
  Promise<undefined> cancel(optional any reason);

  [Throws]
  ReadableStreamReader getReader(optional ReadableStreamGetReaderOptions options = {});

  [Pref="dom.streams.transform_streams.enabled", Throws]
  ReadableStream pipeThrough(ReadableWritablePair transform, optional StreamPipeOptions options = {});

  [Pref="dom.streams.pipeTo.enabled", NewObject]
  Promise<undefined> pipeTo(WritableStream destination, optional StreamPipeOptions options = {});

  [Throws]
  sequence<ReadableStream> tee();

  // Bug 1734244
  // async iterable<any>(optional ReadableStreamIteratorOptions options = {});
};

enum ReadableStreamReaderMode { "byob" };

dictionary ReadableStreamGetReaderOptions {
  ReadableStreamReaderMode mode;
};

dictionary ReadableWritablePair {
  required ReadableStream readable;
  required WritableStream writable;
};

dictionary StreamPipeOptions {
  boolean preventClose = false;
  boolean preventAbort = false;
  boolean preventCancel = false;
  AbortSignal signal;
};
