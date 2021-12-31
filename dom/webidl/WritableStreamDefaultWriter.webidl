[Exposed=(Window,Worker,Worklet)]
interface WritableStreamDefaultWriter {
  [Throws]
  constructor(WritableStream stream);

  readonly attribute Promise<void> closed;
  [Throws] readonly attribute unrestricted double? desiredSize;
  readonly attribute Promise<void> ready;

  [Throws]
  Promise<void> abort(optional any reason);

  [Throws]
  Promise<void> close();

  [Throws]
  void releaseLock();

  [Throws]
  Promise<void> write(optional any chunk);
};
