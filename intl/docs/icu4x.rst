#####
ICU4X
#####

This file documents the procedures for building with `ICU4X <https://github.com/unicode-org/icu4x>`__.

Enabling ICU4X
==============

#. Add the ``ac_add_options --enable-icu4x`` mozconfig. (This is the default)
#. Do a full build.

Updating the bundled ICU4X data
===============================

ICU4X data is bundled directly as a rust crate in the ``intl/icu_testdata``. Although this crate is the same name as ICU4X's ``icu_testdata`` crate, it has customized data for Gecko.

The script ``intl/update-icu4x.sh`` can generate and update this binary data. If you want to add the new data type, you modify this script and then, run it.

When using ICU4X 1.2.0 data, CLDR 43, and ICU4C 73.1, you have to run the following. The baked data is generated into ``intl/icu_testdata/data/baked``.

.. code:: bash

    $ cd $(TOPSRCDIR)/intl
    $ ./update-icu4x.sh https://github.com/unicode-org/icu4x.git icu@1.2.0 43.0.0 release-73-1

ICU4X 1.3 will have new feature "``complied_data``" that replaces customized ``icu_testdata``. After upgrading to 1.3, we update this document too.

Updating ICU4X
==============

If you update ICU4X crate into Gecko, you have to check ``Cargo.toml`` in Gecko's root directory. We might have some hacks to replace crate.io version with a custom version.

C/C++ FFI
=========

ICU4X provides ``icu_capi`` crate for C/C++ FFI. ``mozilla::intl::GetDataProvider`` returns ``capi::ICU4XDataProvider`` of ``icu_capi``. It can return valid data until shutting down.

Accessing the data provider from Rust
=====================================

``icu_testdata::any()`` returns any data provider.

If you want to use unstable data provider from Rust, you should add it to ``intl/icu_testdata/src/lib.rs`` like the following, then use ``icu_testdata::unstable()``.

.. code:: rust

    pub fn unstable() -> UnstableDataProvider {
        UnstableDataProvider
    }

Adding new ICU4X features to Gecko
==================================

To reduce build time and binary size, embedded ICU4X in Gecko is minimal configuration. If you have to add new features, you have to update some files.

#. Adding the feature to ``icu_capi`` entry in ``js/src/rust/shared/Cargo.toml``.
#. Modify ``intl/update-icu4x.sh`` to add generated ICU4X data.
#. Modify ``intl/icu_testdata/Cargo.toml`` to add the data for enabled feature.
