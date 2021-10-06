dictionary QueuingStrategy {
  unrestricted double highWaterMark;
  QueuingStrategySize size;
};

callback QueuingStrategySize = unrestricted double (optional any chunk);

dictionary QueuingStrategyInit {
  required unrestricted double highWaterMark;
};


[Exposed=(Window,Worker,Worklet)]
interface CountQueuingStrategy {
  constructor(QueuingStrategyInit init);

  readonly attribute unrestricted double highWaterMark;

  // This is currently inlined, but will need to be implemented
  // See Bug 1734239
  //
  // readonly attribute Function size;
};
