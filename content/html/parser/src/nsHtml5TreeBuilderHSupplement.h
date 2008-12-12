  private:
    nsHtml5Parser* mParser; // weak ref
    PRUnichar*     mCharBuffer;
    PRInt32        mCharBufferFillLength;
    PRInt32        mCharBufferAllocLength;
    PRBool         mHasProcessedBase;
    void           MaybeFlushAndMaybeSuspend();
  public:
    nsHtml5TreeBuilder(nsHtml5Parser* aParser);
    ~nsHtml5TreeBuilder();
    void Flush();
