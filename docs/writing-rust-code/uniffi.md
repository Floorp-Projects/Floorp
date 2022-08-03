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
  - A JSM module that uses the scaffolding to provide the bindings API.

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

Here's how you can create a new set of bindings using UniFFI:
  - UniFFI your crate (if it isn't already):
    - [Create a UDL file to describe your interface](https://mozilla.github.io/uniffi-rs/udl_file_spec.html)
    - [Set up your Rust crate to generate scaffolding](https://mozilla.github.io/uniffi-rs/tutorial/Rust_scaffolding.html)
  - Add your crate as a Firefox dependency (if it isn't already)
    - If the code will exist in the mozilla-central repo:
      - Create a new directory for the Rust crate
      - Edit `toolkit/library/rust/shared/Cargo.toml` and add a dependency to your library path
    - If the code exists in an external repo:
      - Edit `toolkit/library/rust/shared/Cargo.toml` and add a dependency to your library URL
      - Run `mach vendor rust` to vendor in your Rust code
  - Generate bindings code for your crate
    - TODO: create a system for this and document it here.  I think we can do something like we do for the test
      fixtures, where devs don't need to touch that many moz.build files.
