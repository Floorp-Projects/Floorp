  private:
    nsHtml5Parser* mParser; // weak ref
    PRBool         mHasProcessedBase;
    void           MaybeFlushAndMaybeSuspend();
  public:
    nsHtml5TreeBuilder(nsHtml5Parser* aParser);
    ~nsHtml5TreeBuilder();
    void Flush();
