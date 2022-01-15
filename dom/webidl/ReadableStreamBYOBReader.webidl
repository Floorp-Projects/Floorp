[Exposed=(Window,Worker,Worklet),  Pref="dom.streams.expose.ReadableStreamBYOBReader"]
interface ReadableStreamBYOBReader {
  [Throws]
  constructor(ReadableStream stream);

  [Throws]
  Promise<ReadableStreamBYOBReadResult> read(ArrayBufferView view);

  [Throws]
  void releaseLock();
};
ReadableStreamBYOBReader includes ReadableStreamGenericReader;

dictionary ReadableStreamBYOBReadResult {
 ArrayBufferView value;
 boolean done;
};
