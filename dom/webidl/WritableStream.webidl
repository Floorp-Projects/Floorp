[Exposed=(Window,Worker,Worklet),
//Transferable See Bug 1734240
]
interface WritableStream {
  [Throws]
  constructor(optional object underlyingSink, optional QueuingStrategy strategy = {});

  readonly attribute boolean locked;

  [Throws]
  Promise<void> abort(optional any reason);

  [Throws]
  Promise<void> close();

  [Throws]
  WritableStreamDefaultWriter getWriter();
};
