[Exposed=(Window,Worker,Worklet), Pref="dom.streams.expose.ReadableByteStreamController"]
interface ReadableByteStreamController {
  readonly attribute ReadableStreamBYOBRequest? byobRequest;
  readonly attribute unrestricted double? desiredSize;

  [Throws]
  void close();

  [Throws]
  void enqueue(ArrayBufferView chunk);

  [Throws]
  void error(optional any e);
};
