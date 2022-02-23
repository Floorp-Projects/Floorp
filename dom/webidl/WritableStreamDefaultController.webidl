[Exposed=(Window,Worker,Worklet)]
interface WritableStreamDefaultController {
  [Throws]
  void error(optional any e);
};

// TODO: AbortSignal is not exposed on Worklet
[Exposed=(Window,Worker)]
partial interface WritableStreamDefaultController {
  readonly attribute AbortSignal signal;
};

