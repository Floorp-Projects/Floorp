[Exposed=(Window,Worker,Worklet)]
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
