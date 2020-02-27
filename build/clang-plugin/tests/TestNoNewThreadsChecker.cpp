// Dummy NS_NewNamedThread.
void NS_NewNamedThread(const char *aName) {}

void func_threads() {
  // Test to see if the checker recognizes a bad name, and if it recognizes a
  // name from the ThreadAllows.txt.
  NS_NewNamedThread("A bad name"); // expected-error {{Thread name not recognized. Please use the background thread pool.}} expected-note {{NS_NewNamedThread has been deprecated in favor of background task dispatch via NS_DispatchBackgroundTask and NS_CreateBackgroundTaskQueue. If you must create a new ad-hoc thread, have your thread name added to ThreadAllows.txt.}}
  NS_NewNamedThread("Checker Test");
}
