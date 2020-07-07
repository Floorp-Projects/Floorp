# Rust/C++ interop

This document describes how to use FFI in Firefox to get Rust code and C++ code to interoperate.

## Transferable types

Generally speaking, the more complicated is the data you want to transfer, the
harder it'll be to transfer across the FFI boundary.

Booleans, integers, and pointers cause little trouble.
- C++ `bool` matches Rust `bool`
- C++ `uint8_t` matches Rust `u8`, `int32_t` matches Rust `i32`, etc.
- C++ `const T*` matches Rust `*const T`, `T*` matches Rust `*mut T`.

Lists are handled by C++ `nsTArray` and Rust `ThinVec`.

For strings, it is best to use the `nsstring` helper crate. Using a raw pointer
plus length is also possible for strings, but more error-prone.

If you need a hashmap, you'll likely want to decompose it into two lists (keys
and values) and transfer them separately.

Other types can be handled with tools that generate bindings, as the following
sections describe.

## Accessing C++ code and data from Rust

To call a C++ function from Rust requires adding a function declaration to Rust.
For example, for this C++ function:

```
extern "C" {
bool UniquelyNamedFunction(const nsCString* aInput, nsCString* aRetVal) {
  return true;
}
}
```
add this declaration to the Rust code:
```rust
extern "C" {
    pub fn UniquelyNamedFunction(input: &nsCString, ret_val: &mut nsCString) -> bool;
}
```

Rust code can now call `UniquelyNamedFunction()` within an `unsafe` block. Note
that if the declarations do not match (e.g. because the C++ function signature
changes without the Rust declaration being updated) crashes are likely. (Hence
the `unsafe` block.)

Because of this unsafety, for non-trivial interfaces (in particular when C++
structs and classes must be accessed from Rust code) it's common to use
[rust-bindgen](https://github.com/rust-lang/rust-bindgen), which generates Rust
bindings. The documentation is
[here](https://rust-lang.github.io/rust-bindgen/).

## Accessing Rust code and data from C++

A common option for accessing Rust code and data from C++ is to use
[cbindgen](https://github.com/eqrion/cbindgen), which generates C++ header
files. for Rust crates that expose a public C API. cbindgen is a very powerful
tool, and this section only covers some basic uses of it.

### Basics

First, add suitable definitions to your Rust. `#[no_mangle]` and `extern "C"`
are required.

```rust
#[no_mangle]
pub unsafe extern "C" fn unic_langid_canonicalize(
    langid: &nsCString,
    ret_val: &mut nsCString
) -> bool {
    ret_val.assign("new value");
    true
}
```

Then, add a `cbindgen.toml` file in the root of your crate. It may look like this:

```toml
header = """/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */"""
autogen_warning = """/* DO NOT MODIFY THIS MANUALLY! This file was generated using cbindgen. See RunCbindgen.py */
#ifndef mozilla_intl_locale_MozLocaleBindings_h
#error "Don't include this file directly, instead include MozLocaleBindings.h"
#endif
"""
include_version = true
braces = "SameLine"
line_length = 100
tab_width = 2
language = "C++"
# Put FFI calls in the `mozilla::intl::ffi` namespace.
namespaces = ["mozilla", "intl", "ffi"]

# Export `ThinVec` references as `nsTArray`.
[export.rename]
"ThinVec" = "nsTArray"
```

Next, extend the relevant `moz.build` file to invoke cbindgen.

```python
if CONFIG['COMPILE_ENVIRONMENT']:
    CbindgenHeader('unic_langid_ffi_generated.h',
                   inputs=['/intl/locale/rust/unic-langid-ffi'])

    EXPORTS.mozilla.intl += [
        '!unic_langid_ffi_generated.h',
    ]
```

This tells the build system to run cbindgen on
`intl/locale/rust/unic-langid-ffi` to generate `unic_langid_ffi_generated.h`,
which will be placed in `$OBJDIR/dist/include/mozilla/intl/`.

Finally, include the generated header into a C++ file and call the function.

```c++
#include "mozilla/intl/unic_langid_ffi_generated.h"

using namespace mozilla::intl::ffi;

void Locale::MyFunction(nsCString& aInput) const {
  nsCString result;
  unic_langid_canonicalize(aInput, &result);
}
```

### Complex types

Many complex Rust types can be exposed to C++, and cbindgen will generate
appropriate bindings for all `pub` types. For example:

```rust
#[repr(C)]
pub enum FluentPlatform {
    Linux,
    Windows,
    Macos,
    Android,
    Other,
}

extern "C" {
    pub fn FluentBuiltInGetPlatform() -> FluentPlatform;
}
```

```c++
ffi::FluentPlatform FluentBuiltInGetPlatform() {
  return ffi::FluentPlatform::Linux;
}
```

For an example using cbindgen to expose much more complex Rust types to C++,
see [this blog post].

[this blog post]: https://crisal.io/words/2020/02/28/C++-rust-ffi-patterns-1-complex-data-structures.html

### Instances

If you need to create and destroy a Rust struct from C++ code, the following
example may be helpful.

First, define constructor, destructor and getter functions in Rust. (C++
declarations for these will be generated by cbindgen.)

```rust
#[no_mangle]
pub unsafe extern "C" fn unic_langid_new() -> *mut LanguageIdentifier {
    let langid = LanguageIdentifier::default();
    Box::into_raw(Box::new(langid))
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_destroy(langid: *mut LanguageIdentifier) {
    drop(Box::from_raw(langid));
}

#[no_mangle]
pub unsafe extern "C" fn unic_langid_as_string(
    langid: &mut LanguageIdentifier,
    ret_val: &mut nsACString,
) {
    ret_val.assign(&langid.to_string());
}
```

Next, in a C++ header define a destructor via `DefaultDelete`.

```c++
#include "mozilla/intl/unic_langid_ffi_generated.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

template <>
class DefaultDelete<intl::ffi::LanguageIdentifier> {
 public:
  void operator()(intl::ffi::LanguageIdentifier* aPtr) const {
    unic_langid_destroy(aPtr);
  }
};

}  // namespace mozilla
```

(This definition must be visible any place where
`UniquePtr<intl::ffi::LanguageIdentifier>` is used, otherwise C++ will try to
free the code, which might lead to strange behaviour!)

Finally, implement the class.

```c++
class Locale {
public:
  explicit Locale(const nsACString& aLocale)
    : mRaw(unic_langid_new()) {}

  const nsCString Locale::AsString() const {
    nsCString tag;
    unic_langid_as_string(mRaw.get(), &tag);
    return tag;
  }

private:
  UniquePtr<ffi::LanguageIdentifier> mRaw;
}
```

This makes it possible to instantiate a `Locale` object and call `AsString()`,
all from C++ code.

## Other examples

For a detailed explanation of an interface in Firefox that doesn't use cbindgen
or rust-bindgen, see [this blog post](https://hsivonen.fi/modern-cpp-in-rust/).
