``script::ffi``
===============

.. cpp:class:: ICU4XScriptExtensionsSet

    An object that represents the Script_Extensions property for a single character

    See the `Rust documentation for ScriptExtensionsSet <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html>`__ for more information.


    .. cpp:function:: bool contains(uint16_t script) const

        Check if the Script_Extensions property of the given code point covers the given script

        See the `Rust documentation for contains <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.contains>`__ for more information.


    .. cpp:function:: size_t count() const

        Get the number of scripts contained in here

        See the `Rust documentation for iter <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter>`__ for more information.


    .. cpp:function:: diplomat::result<uint16_t, std::monostate> script_at(size_t index) const

        Get script at index, returning an error if out of bounds

        See the `Rust documentation for iter <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter>`__ for more information.


.. cpp:class:: ICU4XScriptWithExtensions

    An ICU4X ScriptWithExtensions map object, capable of holding a map of codepoints to scriptextensions values

    See the `Rust documentation for ScriptWithExtensions <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensions.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XScriptWithExtensions, ICU4XError> create(const ICU4XDataProvider& provider)

        See the `Rust documentation for script_with_extensions <https://docs.rs/icu/latest/icu/properties/script/fn.script_with_extensions.html>`__ for more information.


    .. cpp:function:: uint16_t get_script_val(uint32_t code_point) const

        Get the Script property value for a code point

        See the `Rust documentation for get_script_val <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val>`__ for more information.


    .. cpp:function:: bool has_script(uint32_t code_point, uint16_t script) const

        Check if the Script_Extensions property of the given code point covers the given script

        See the `Rust documentation for has_script <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script>`__ for more information.


    .. cpp:function:: ICU4XScriptWithExtensionsBorrowed as_borrowed() const

        Borrow this object for a slightly faster variant with more operations

        See the `Rust documentation for as_borrowed <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensions.html#method.as_borrowed>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: CodePointRangeIterator iter_ranges_for_script(uint16_t script) const

        Get a list of ranges of code points that contain this script in their Script_Extensions values

        See the `Rust documentation for get_script_extensions_ranges <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_ranges>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


.. cpp:class:: ICU4XScriptWithExtensionsBorrowed

    A slightly faster ICU4XScriptWithExtensions object

    See the `Rust documentation for ScriptWithExtensionsBorrowed <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html>`__ for more information.


    .. cpp:function:: uint16_t get_script_val(uint32_t code_point) const

        Get the Script property value for a code point

        See the `Rust documentation for get_script_val <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val>`__ for more information.


    .. cpp:function:: ICU4XScriptExtensionsSet get_script_extensions_val(uint32_t code_point) const

        Get the Script property value for a code point

        See the `Rust documentation for get_script_extensions_val <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_val>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: bool has_script(uint32_t code_point, uint16_t script) const

        Check if the Script_Extensions property of the given code point covers the given script

        See the `Rust documentation for has_script <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script>`__ for more information.


    .. cpp:function:: ICU4XCodePointSetData get_script_extensions_set(uint16_t script) const

        Build the CodePointSetData corresponding to a codepoints matching a particular script in their Script_Extensions

        See the `Rust documentation for get_script_extensions_set <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_set>`__ for more information.

