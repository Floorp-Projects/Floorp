/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use icu_provider::prelude::*;

/// An [`AnyProvider`] backed by baked data.
pub fn any() -> impl AnyProvider {
    UnstableDataProvider
}

#[doc(hidden)]
#[non_exhaustive]
#[derive(Debug)]
pub struct UnstableDataProvider;

mod baked {
    include!("../data/baked/mod.rs");
    impl_data_provider!(super::UnstableDataProvider);
    impl_any_provider!(super::UnstableDataProvider);
}
