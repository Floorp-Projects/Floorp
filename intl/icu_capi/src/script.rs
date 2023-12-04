// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[diplomat::bridge]
pub mod ffi {
    use crate::provider::ffi::ICU4XDataProvider;
    use alloc::boxed::Box;
    use icu_properties::{script, sets::CodePointSetData, Script};

    use crate::errors::ffi::ICU4XError;
    use crate::properties_iter::ffi::CodePointRangeIterator;
    use crate::properties_sets::ffi::ICU4XCodePointSetData;

    #[diplomat::opaque]
    /// An ICU4X ScriptWithExtensions map object, capable of holding a map of codepoints to scriptextensions values
    #[diplomat::rust_link(icu::properties::script::ScriptWithExtensions, Struct)]
    #[diplomat::rust_link(
        icu::properties::script::ScriptWithExtensions::from_data,
        FnInStruct,
        hidden
    )]
    pub struct ICU4XScriptWithExtensions(pub script::ScriptWithExtensions);

    #[diplomat::opaque]
    /// A slightly faster ICU4XScriptWithExtensions object
    #[diplomat::rust_link(icu::properties::script::ScriptWithExtensionsBorrowed, Struct)]
    pub struct ICU4XScriptWithExtensionsBorrowed<'a>(pub script::ScriptWithExtensionsBorrowed<'a>);
    #[diplomat::opaque]
    /// An object that represents the Script_Extensions property for a single character
    #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet, Struct)]
    pub struct ICU4XScriptExtensionsSet<'a>(pub script::ScriptExtensionsSet<'a>);

    impl ICU4XScriptWithExtensions {
        #[diplomat::rust_link(icu::properties::script::script_with_extensions, Fn)]
        pub fn create(
            provider: &ICU4XDataProvider,
        ) -> Result<Box<ICU4XScriptWithExtensions>, ICU4XError> {
            Ok(Box::new(ICU4XScriptWithExtensions(call_constructor!(
                script::script_with_extensions [r => Ok(r.static_to_owned())],
                script::load_script_with_extensions_with_any_provider,
                script::load_script_with_extensions_with_buffer_provider,
                provider
            )?)))
        }

        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        pub fn get_script_val(&self, code_point: u32) -> u16 {
            self.0.as_borrowed().get_script_val(code_point).0
        }

        /// Check if the Script_Extensions property of the given code point covers the given script
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script,
            FnInStruct
        )]
        pub fn has_script(&self, code_point: u32, script: u16) -> bool {
            self.0.as_borrowed().has_script(code_point, Script(script))
        }

        /// Borrow this object for a slightly faster variant with more operations
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensions::as_borrowed,
            FnInStruct
        )]
        pub fn as_borrowed<'a>(&'a self) -> Box<ICU4XScriptWithExtensionsBorrowed<'a>> {
            Box::new(ICU4XScriptWithExtensionsBorrowed(self.0.as_borrowed()))
        }

        /// Get a list of ranges of code points that contain this script in their Script_Extensions values
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_ranges,
            FnInStruct
        )]
        pub fn iter_ranges_for_script<'a>(
            &'a self,
            script: u16,
        ) -> Box<CodePointRangeIterator<'a>> {
            Box::new(CodePointRangeIterator(Box::new(
                self.0
                    .as_borrowed()
                    .get_script_extensions_ranges(Script(script)),
            )))
        }
    }

    impl<'a> ICU4XScriptWithExtensionsBorrowed<'a> {
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_val,
            FnInStruct
        )]
        pub fn get_script_val(&self, code_point: u32) -> u16 {
            self.0.get_script_val(code_point).0
        }
        /// Get the Script property value for a code point
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_val,
            FnInStruct
        )]
        pub fn get_script_extensions_val(
            &self,
            code_point: u32,
        ) -> Box<ICU4XScriptExtensionsSet<'a>> {
            Box::new(ICU4XScriptExtensionsSet(
                self.0.get_script_extensions_val(code_point),
            ))
        }
        /// Check if the Script_Extensions property of the given code point covers the given script
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::has_script,
            FnInStruct
        )]
        pub fn has_script(&self, code_point: u32, script: u16) -> bool {
            self.0.has_script(code_point, Script(script))
        }

        /// Build the CodePointSetData corresponding to a codepoints matching a particular script
        /// in their Script_Extensions
        #[diplomat::rust_link(
            icu::properties::script::ScriptWithExtensionsBorrowed::get_script_extensions_set,
            FnInStruct
        )]
        pub fn get_script_extensions_set(&self, script: u16) -> Box<ICU4XCodePointSetData> {
            let list = self
                .0
                .get_script_extensions_set(Script(script))
                .into_owned();
            let set = CodePointSetData::from_code_point_inversion_list(list);
            Box::new(ICU4XCodePointSetData(set))
        }
    }
    impl<'a> ICU4XScriptExtensionsSet<'a> {
        /// Check if the Script_Extensions property of the given code point covers the given script
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::contains, FnInStruct)]
        pub fn contains(&self, script: u16) -> bool {
            self.0.contains(&Script(script))
        }

        /// Get the number of scripts contained in here
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        pub fn count(&self) -> usize {
            self.0.array_len()
        }

        /// Get script at index, returning an error if out of bounds
        #[diplomat::rust_link(icu::properties::script::ScriptExtensionsSet::iter, FnInStruct)]
        pub fn script_at(&self, index: usize) -> Result<u16, ()> {
            self.0.array_get(index).map(|x| x.0).ok_or(())
        }
    }
}
