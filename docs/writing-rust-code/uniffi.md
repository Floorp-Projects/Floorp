# Generating Javascript bindings with UniFFI

Firefox supports auto-generating JS bindings for Rust components using [UniFFI](https://mozilla.github.io/uniffi-rs/).

## How it works

The Rust crate contains a
[UniFFI Definition Language (UDL) file](https://mozilla.github.io/uniffi-rs/udl_file_spec.html), which describes the
interface to generate bindings for.

The UniFFI core generates the scaffolding: Rust code which acts as the FFI layer from the UDL file.  The functions of
this layer all use the C calling convention and all structs use a C layout, this is the de facto standard for FFI
interoperability.

The [`uniffi-bindgen-gecko-js`](https://searchfox.org/mozilla-central/source/toolkit/components/uniffi-bindgen-gecko-js)
tool, which lives in the Firefox source tree, generates 2 things:
  - A JS interface for the scaffolding code, which uses [WebIDL](/dom/bindings/webidl/index.rst)
  - A module that uses the scaffolding to provide the bindings API.

Currently, this generated code gets checked in to source control.  We are working on a system to avoid this and
auto-generate it at build time instead (see [bugzilla 1756214](https://bugzilla.mozilla.org/show_bug.cgi?id=1756214)).

## Before creating new bindings with UniFFI

Keep a few things in mind before you create a new set of bindings:

 - **UniFFI was not written to maximize performance.**  It's code is efficient enough to handle many use cases, but at this
   point should probably be avoided for performance critical components.
 - **uniffi-bindgen-gecko-js bindings run with chrome privileges.**  Make sure this is acceptable for your project
 - **Only a subset of Rust types can be exposed via the FFI.**  Check the [UniFFI Book](https://mozilla.github.io/uniffi-rs/) to see what
   types are compatible with UniFFI.

If any of these are blockers for your work, consider discussing it further with the UniFFI devs to see if we can support
your project:

  - Chat with us on `#uniffi` on Matrix/Element
  - File an issue on [mozilla/uniffi](https://github.com/mozilla/uniffi-rs/)

## Creating new bindings with UniFFI

You can see an example of this feature in use: [when application-services swapped the tabs js sync engine with rust](https://bugzilla.mozilla.org/show_bug.cgi?id=1791851)

Here's how you can create a new set of bindings using UniFFI:

  1. UniFFI your crate (if it isn't already):
      - [Create a UDL file to describe your interface](https://mozilla.github.io/uniffi-rs/udl_file_spec.html)
      - [Set up your Rust crate to generate scaffolding](https://mozilla.github.io/uniffi-rs/tutorial/Rust_scaffolding.html)
  2. Add your crate as a Firefox dependency (if it isn't already)
      - **If the code will exist in the mozilla-central repo:**
        - Create a new directory for the Rust crate
        - Edit `toolkit/library/rust/shared/Cargo.toml` and add a dependency to your library path
      - **If the code exists in an external repo:**
        - Edit `toolkit/library/rust/shared/Cargo.toml` and add a dependency to your library URL
        - Run `mach vendor rust` to vendor in your Rust code
  3. Generate bindings code for your crate
      - Add the path of your UDL (that you made in step 1) in `toolkit/components/uniffi-bindgen-gecko-js/mach_commands.py`
      - Run `./mach uniffi generate`
          - add your newly generated `Rust{udl-name}.sys.mjs` file to `toolkit/components/uniffi-bindgen-gecko-js/components/moz.build`
      - Then simply import your module to the file you want to use it in and start using your APIs!

        Example from tabs module:

        ``` js
        ChromeUtils.defineESModuleGetters(lazy, {
          ...
          TabsStore: "resource://gre/modules/RustTabs.sys.mjs",
        });
        ...
        this._rustStore = await lazy.TabsStore.init(path);
        ```
