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

ICU4X data is bundled directly as a rust crate with ``compiled_data`` feature.

But each ``icu_*_data`` crate has all locale data, so it might include unnecessary data. ``icu_segmenter`` uses custom crates in Gecko since it includes unnecessary data such as the word dictionaries for East Asian.

The script ``intl/update-icu4x.sh`` can generate and update this binary data. If you want to add the new data type, you modify this script and then, run it.

When using ICU4X 1.4.0 data with the latest data that is hard-coded, you have to run the following. The baked data of ``icu_segmenter`` is generated into ``intl/icu_segmenter_data/data``.

.. code:: bash

    $ cd $(TOPSRCDIR)/intl
    $ ./update-icu4x.sh https://github.com/unicode-org/icu4x.git icu@1.4.0 44.0.0 release-74-1 1.4.0

Updating ICU4X
==============

If you update ICU4X crate into Gecko, you have to check ``Cargo.toml`` in Gecko's root directory. We might have some hacks to replace crate.io version with a custom version.

C/C++ FFI
=========

ICU4X provides ``icu_capi`` crate for C/C++ FFI. ``mozilla::intl::GetDataProvider`` returns ``capi::ICU4XDataProvider`` of ``icu_capi``. It can return valid data until shutting down.

Accessing the data provider from Rust
=====================================

Use ``compiled_data`` feature. You don't consider data provider.

Adding new ICU4X features to Gecko
==================================

To reduce build time and binary size, embedded ICU4X in Gecko is minimal configuration. If you have to add new features, you have to update some files.

#. Adding the feature to ``icu_capi`` entry in ``js/src/rust/shared/Cargo.toml``.
#. Modify ``[features]`` section in ``intl/icu_capi/Cargo.toml`` to enable ``compiled_data`` feature of added crate.
#. Modify the patch file for ``intl/icu_capi/Cargo.toml`` into ``intl/icu4x-patches``.
#. (Optional) Modify ``intl/update-icu4x.sh`` to add generated ICU4X data if you want to modify ICU4X baked data.
