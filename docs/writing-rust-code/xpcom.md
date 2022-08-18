# XPCOM components in Rust

XPCOM components can be written in Rust.

## A tiny example

The following example shows a new type that implements `nsIObserver`.

First, create a new empty crate (e.g. with `cargo init --lib`), and add the
following dependencies in its `Cargo.toml` file.

```toml
[dependencies]
libc = "0.2"
nserror = { path = "../../../xpcom/rust/nserror" }
nsstring = { path = "../../../xpcom/rust/nsstring" }
xpcom = { path = "../../../xpcom/rust/xpcom" }
```

(The number of `../` occurrences will depend on the depth of the crate in the
file hierarchy.)

Next hook it into the build system according to the [build
documentation](/build/buildsystem/rust.rst).

The Rust code will need to import some basic types. `xpcom::interfaces`
contains all the usual `nsI` interfaces.

```rust
use libc::c_char;
use nserror::nsresult;
use std::sync::atomic::{AtomicBool, Ordering};
use xpcom::{interfaces::nsISupports, RefPtr};
```

The next part declares the implementation.

```rust
#[xpcom(implement(nsIObserver), atomic)]
struct MyObserver {
    ran: AtomicBool,
}
```

This defines the implementation type, which will be refcounted in the specified
way and implement the listed xpidl interfaces. It will also declare a second
initializer struct `InitMyObserver` which can be used to allocate a new
`MyObserver` using the `MyObserver::allocate` method.

Next, all interface methods are declared in the `impl` block as `unsafe` methods.

```rust
impl MyObserver {
    #[allow(non_snake_case)]
    unsafe fn Observe(
        &self,
        _subject: *const nsISupports,
        _topic: *const c_char,
        _data: *const u16,
    ) -> nsresult {
        self.ran.store(true, Ordering::SeqCst);
        nserror::NS_OK
    }
}
```

These methods always take `&self`, not `&mut self`, so we need to use interior
mutability: `AtomicBool`, `RefCell`, `Cell`, etc. This is because all XPCOM
objects are reference counted (like `Arc<T>`), so cannot provide exclusive access.

XPCOM methods are unsafe by default, but the
[xpcom_method!](https://searchfox.org/mozilla-central/source/xpcom/rust/xpcom/src/method.rs)
macro can be used to clean this up. It also takes care of null-checking and
hiding pointers behind references, lets you return a `Result` instead of an
`nsresult,` and so on.

To use this type within Rust code, do something like the following.

```rust
let observer = MyObserver::allocate(InitMyObserver {
  ran: AtomicBool::new(false),
});
let rv = unsafe {
  observer.Observe(x.coerce(),
                   cstr!("some-topic").as_ptr(),
                   ptr::null())
};
assert!(rv.succeeded());
```

The implementation has an (auto-generated) `allocate` method that takes in an
initialization struct, and returns a `RefPtr` to the instance.

`coerce` casts any XPCOM object to one of its base interfaces; in this case,
the base interface is `nsISupports`. In C++, this would be handled
automatically through inheritance, but Rust doesn’t have inheritance, so the
conversion must be explicit.

## Bigger examples

The following XPCOM components are written in Rust.

- [kvstore](https://searchfox.org/mozilla-central/source/toolkit/components/kvstore),
  which exposes the LMDB key-value store (via the [Rkv
  library](https://docs.rs/rkv)) The API is asynchronous, using `moz_task` to
  schedule all I/O on a background thread, and supports getting, setting, and
  iterating over keys.
- [cert_storage](https://searchfox.org/mozilla-central/source/security/manager/ssl/cert_storage),
  which stores lists of [revoked intermediate certificates](https://blog.mozilla.org/security/2015/03/03/revoking-intermediate-certificates-introducing-onecrl/).
- [bookmark_sync](https://searchfox.org/mozilla-central/source/toolkit/components/places/bookmark_sync),
  which [merges](https://mozilla.github.io/dogear) bookmarks from Firefox Sync
  with bookmarks in the Places database.
  [There's also some docs on how Rust interacts with Sync](/services/sync/rust-engines.rst)
- [webext_storage_bridge](https://searchfox.org/mozilla-central/source/toolkit/components/extensions/storage/webext_storage_bridge),
  which powers the WebExtension storage.sync API. It's a self-contained example
  that pulls in a crate from application-services for the heavy lifting, wraps
  that up in a Rust XPCOM component, and then wraps the component in a JS
  interface. There's also some boilerplate there around adding a
  `components.conf` file, and a dummy C++ header that declares the component
  constructor. [It has some in-depth documentation on how it hangs together](../toolkit/components/extensions/webextensions/webext-storage.rst).
