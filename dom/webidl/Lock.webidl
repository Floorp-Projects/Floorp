[SecureContext, Exposed=(Window,Worker), Pref="dom.weblocks.enabled"]
interface Lock {
  readonly attribute DOMString name;
  readonly attribute LockMode mode;
};
