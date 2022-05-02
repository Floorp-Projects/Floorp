[SecureContext, Exposed=(Window,Worker), Pref="dom.weblocks.enabled"]
interface LockManager {
  [NewObject]
  Promise<any> request(DOMString name,
                       LockGrantedCallback callback);
  [NewObject]
  Promise<any> request(DOMString name,
                       LockOptions options,
                       LockGrantedCallback callback);

  [NewObject]
  Promise<LockManagerSnapshot> query();
};

callback LockGrantedCallback = Promise<any> (Lock? lock);

enum LockMode { "shared", "exclusive" };

dictionary LockOptions {
  LockMode mode = "exclusive";
  boolean ifAvailable = false;
  boolean steal = false;
  AbortSignal signal;
};

dictionary LockManagerSnapshot {
  sequence<LockInfo> held;
  sequence<LockInfo> pending;
};

dictionary LockInfo {
  DOMString name;
  LockMode mode;
  DOMString clientId;
};
