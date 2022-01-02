# Experimentation with ICU4X

We're currently conducting some experiments with using [ICU4X](https://github.com/unicode-org/icu4x) in Gecko rather than ICU4C. This file documents the procedures for building with ICU4X. The current implementation is incomplete, and hopefully we can begin to land code incrementally. This document will serve as a documentation for the status of this experimentation on what has been landed in tree.

## Enabling ICU4X

To enable the ICU4X experimentation:

 1. Add the `ac_add_options --enable-icu4x` mozconfig.
 2. Generate the locale data by running `./intl/update-icu4x.sh`.
 3. Do a full build.

## Pieces of the ICU4X integration

#### Bundle the ICU4X data

The data is bundled directly into the binary in the `config/external/icu4x` directory. The "icu4xdata" is a separate library, which consists of an assembly file that directly includes the ICU4X locale binary data. Eventually this binary data will be directly accessed via ICU4X's StaticDataProvider.

The script `intl/update-icu4x.sh` can generate and update this binary data. At the time of this writing, this data is not checked in to source control since it's quite large with no ability to prune the data with only exporting certain keys. See [unicode-org/icu4x#192](https://github.com/unicode-org/icu4x/issues/192) for supporting splitting out keys.

#### Vendored ICU4X

At the time of this writing, ICU4X has been [added to the allow list for vendored code](https://searchfox.org/mozilla-central/rev/b24799980a929597dcc553cb0854aa6c960c82b5/python/mozbuild/mozbuild/vendor/vendor_rust.py#284-293) but the files still need to be vendored in. This is currently blocked on some large testdata being included. There is a tracking issue [unicode-org/icu4x#849](https://github.com/unicode-org/icu4x/issues/849) for the ICU4X integration, but we might find a way to prune the files on the Gecko vendoring code.

#### Static Data Provider

The data provider in ICU4X is the mechanism to load the locale-specific data. In [previous experiments](https://bugzilla.mozilla.org/show_bug.cgi?id=1713136) we used the `FsDataProvider` which hits the file system every time the APIs need any data. Future integrations should look into using the `StaticDataProvider`.

#### Building with FFIs

The final step in the ICU4X integration is to actually build and include the C++ FFI files. In the Summer 2021 experimentation we used manually built FFIs, but future experiments will rely on the [Diplomat](https://github.com/rust-diplomat/diplomat)-built FFIs.
