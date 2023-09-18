// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use proc_macro::TokenStream;
use quote::quote;
use syn::parse::{Parse, ParseStream, Result};
use syn::{parse_macro_input, Attribute, LitStr, Signature};

/* Proc macro equivalent to the following rust macro:
 * ```
 * macro_rules! link {
 *    ($library:literal $abi:literal $($link_name:literal)? $(#[$($doc:tt)*])* fn $name:ident($($arg:ident: $argty:ty),*)->$ret:ty) => (
 *        extern $abi {
 *            #[link(name = $library)]
 *            $(#[link_name=$link_name])?
 *            pub fn $name($($arg: $argty),*) -> $ret;
 *        }
 *     )
 * }
 * ```
 * with the additional feature of removing ".dll" from the $library literal.
 *
 * The macro is derived from the equivalent macro in the real windows-targets crate,
 * with the difference that it uses #[link] with the name of the library rather than
 * a single "windows.$version" library, so as to avoid having to vendor all the fake
 * "windows.$version" import libraries. We can do that because we also require MSVC
 * to build, so we do have the real import libraries available.
 *
 * As the library name is there in the original for raw-dylib support, it contains
 * a suffixed name, but plain #[link] expects a non-suffixed name, which is why we
 * remove the suffix (and why this had to be a proc-macro).
 *
 * Once raw-dylib is more widely available and tested, we'll be able to use the
 * raw-dylib variants directly.
 */

struct LinkMacroInput {
    library: LitStr,
    abi: LitStr,
    link_name: Option<LitStr>,
    function: Signature,
}

impl Parse for LinkMacroInput {
    fn parse(input: ParseStream) -> Result<Self> {
        let library: LitStr = input.parse()?;
        let abi: LitStr = input.parse()?;
        let link_name: Option<LitStr> = input.parse().ok();
        let _doc_comments = Attribute::parse_outer(input)?;
        let function: Signature = input.parse()?;
        Ok(LinkMacroInput {
            library,
            abi,
            link_name,
            function,
        })
    }
}

#[proc_macro]
pub fn link(input: TokenStream) -> TokenStream {
    let LinkMacroInput {
        library,
        abi,
        link_name,
        function,
    } = parse_macro_input!(input as LinkMacroInput);

    let link_name_attr = link_name.map(|lit| quote! { #[link_name = #lit] });

    let library = library.value();
    let library = library.strip_suffix(".dll").unwrap_or(&library);

    let generated = quote! {
        extern #abi {
            #[link(name = #library)]
            #link_name_attr
            pub #function;
        }
    };

    TokenStream::from(generated)
}
