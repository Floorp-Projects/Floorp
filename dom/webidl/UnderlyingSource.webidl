[GenerateInit]
dictionary UnderlyingSource {
  UnderlyingSourceStartCallback start;
  UnderlyingSourcePullCallback pull;
  UnderlyingSourceCancelCallback cancel;
  ReadableStreamType type;
  [EnforceRange] unsigned long long autoAllocateChunkSize;
};

// Until ReadableByteStreamController is implemented, this typedef is only a subset.
typedef ReadableStreamDefaultController ReadableStreamController;

callback UnderlyingSourceStartCallback = any (ReadableStreamController controller);
callback UnderlyingSourcePullCallback = Promise<void> (ReadableStreamController controller);
callback UnderlyingSourceCancelCallback = Promise<void> (optional any reason);
