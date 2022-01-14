[Exposed=(Window,Worker,Worklet), Pref="dom.streams.expose.ReadableStreamDefaultController"]
interface ReadableStreamDefaultController {
  readonly attribute unrestricted double? desiredSize;

  [Throws]
  void close();

  [Throws]
  void enqueue(optional any chunk);

  [Throws]
  void error(optional any e);
};
