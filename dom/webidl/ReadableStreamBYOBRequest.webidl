[Exposed=(Window,Worker,Worklet), Pref="dom.streams.expose.ReadableStreamBYOBRequest"]
interface ReadableStreamBYOBRequest {
  readonly attribute ArrayBufferView? view;

  [Throws]
  void respond([EnforceRange] unsigned long long bytesWritten);
  
  [Throws]
  void respondWithNewView(ArrayBufferView view);
};
