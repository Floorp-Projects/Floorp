import java.util.Hashtable;

class Context {
    
    static boolean printTrees = true;
    public static final int VERSION_DEFAULT =    0;
    public static final int VERSION_1_2 =      120;
    
    public Context() {
        setLanguageVersion(VERSION_DEFAULT);
    }

    public synchronized void enter() throws ThreadLinkException,
                                            InterruptedException
    {
        Thread t = Thread.currentThread();

        while (currentThread != null) {

            // Check to avoid deadlock
            if (t == currentThread) {
                throw new ThreadLinkException(
                            "Context already associated with a Thread");
            }
            wait();
        }
        currentThread = t;

        synchronized (threadContexts) {
            Context cx = (Context) threadContexts.get(t);
            if (cx != null) {
                currentThread = null;
                throw new ThreadLinkException(
                            "Thread already associated with a Context");
            }
            threadContexts.put(t, this);
        }
    }

    static Context getContext() {
        Thread t = Thread.currentThread();
        Context cx = (Context) threadContexts.get(t);
        if (cx == null) {
            throw new WrappedException(new ThreadLinkException(
                "No Context associated with current Thread"));
        }
        return cx;
    }
    
    public void setLanguageVersion(int version) {
        this.version = version;
    }

    public int getLanguageVersion() {
       return version;
    }
    int version;

    static String getMessage(String messageId, Object[] arguments)
    {
        return messageId;
    }
    
    static void reportError(String message, String sourceName,
                                   int lineno, String lineSource,
                                   int lineOffset)
    {
    }
                                   
    static void reportWarning(String message, String sourceName,
                                     int lineno, String lineSource,
                                     int lineOffset)
    {
    }
   
    private Thread currentThread;
    private static Hashtable threadContexts = new Hashtable(11);
};
    