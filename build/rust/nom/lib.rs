/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Strict minimum to make glsl happy.
pub use nom::{branch, bytes, character, combinator, error, sequence, Err, IResult, ParseTo};

pub mod multi {
    use nom::multi::fold_many0 as nom_fold_many0;
    pub use nom::multi::{many0, many1, separated_list0};

    pub fn fold_many0<I, O, E, F, G, R>(
        f: F,
        init: R,
        g: G,
    ) -> impl FnMut(I) -> nom::IResult<I, R, E>
    where
        I: Clone + nom::InputLength,
        F: nom::Parser<I, O, E>,
        G: FnMut(R, O) -> R,
        E: nom::error::ParseError<I>,
        R: Clone,
    {
        nom_fold_many0(f, move || init.clone(), g)
    }
}
