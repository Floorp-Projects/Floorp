[Exposed=(Window,Worker,Worklet)]
interface ReadableStreamDefaultController {
  readonly attribute unrestricted double? desiredSize;

  [Throws]
  void close();

  [Throws]
  void enqueue(optional any chunk);

  [Throws]
  void error(optional any e);
};
