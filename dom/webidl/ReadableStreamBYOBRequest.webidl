[Exposed=(Window,Worker,Worklet)]
interface ReadableStreamBYOBRequest {
  readonly attribute ArrayBufferView? view;

  [Throws]
  void respond([EnforceRange] unsigned long long bytesWritten);
  
  [Throws]
  void respondWithNewView(ArrayBufferView view);
};
