function test() {
  // Access an xpcom component from off the main thread.

  let threadman = Components.classes["@mozilla.org/thread-manager;1"].
                             getService(Ci.nsIThreadManager);
  let thread = threadman.newThread(0);

  thread.dispatch(
    function() {
      // Get another handle on the thread manager, this time from within the
      // thread we created.
      let threadman2 = Components.classes["@mozilla.org/thread-manager;1"].
                                  getService(Ci.nsIThreadManager);

      // Now do something with the thread manager.  It doesn't really matter
      // what we do.
      if (threadman2.mainThread == threadman2.currentThread)
        ok(false, "Shouldn't be on the main thread.");

    }, 1); // dispatch sync

  thread.shutdown();
  ok(true, "Done!");
}
