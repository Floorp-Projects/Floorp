[Exposed=(Window,Worker,Worklet),
//Transferable See Bug 1734240
]
interface ReadableStream {
  [Throws]
  constructor(optional object underlyingSource, optional QueuingStrategy strategy = {});

  readonly attribute boolean locked;

  [Throws]
  Promise<void> cancel(optional any reason);

  [Throws]
  ReadableStreamReader getReader(optional ReadableStreamGetReaderOptions options = {});

  // Bug 1734243
  // ReadableStream pipeThrough(ReadableWritablePair transform, optional StreamPipeOptions options = {});

  // Bug 1734241
  // Promise<undefined> pipeTo(WritableStream destination, optional StreamPipeOptions options = {});

  [Throws]
  sequence<ReadableStream> tee();

  // Bug 1734244
  // async iterable<any>(optional ReadableStreamIteratorOptions options = {});
};

enum ReadableStreamReaderMode { "byob" };

dictionary ReadableStreamGetReaderOptions {
  ReadableStreamReaderMode mode;
};
