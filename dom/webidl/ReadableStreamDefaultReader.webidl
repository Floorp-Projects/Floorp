
// This typedef should actually be the union of ReadableStreamDefaultReader
// and ReadableStreamBYOBReader. However, we've not implmented the latter
// yet, and so for now the typedef is subset.
typedef ReadableStreamDefaultReader ReadableStreamReader;


enum ReadableStreamType { "bytes" };

interface mixin ReadableStreamGenericReader {
  readonly attribute Promise<void> closed;

  [Throws]
  Promise<void> cancel(optional any reason);
};

[Exposed=(Window,Worker,Worklet)]
interface ReadableStreamDefaultReader {
  [Throws]
  constructor(ReadableStream stream);

  [Throws]
  Promise<ReadableStreamDefaultReadResult> read();

  [Throws]
  void releaseLock();
};
ReadableStreamDefaultReader includes ReadableStreamGenericReader;


dictionary ReadableStreamDefaultReadResult {
 any value;
 boolean done;
};
