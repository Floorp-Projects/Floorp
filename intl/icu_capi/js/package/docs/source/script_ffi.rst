``script::ffi``
===============

.. js:class:: ICU4XScriptExtensionsSet

    An object that represents the Script_Extensions property for a single character

    See the `Rust documentation for ScriptExtensionsSet <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html>`__ for more information.


    .. js:method:: contains(script)

        Check if the Script_Extensions property of the given code point covers the given script

        See the `Rust documentation for contains <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.contains>`__ for more information.


    .. js:method:: count()

        Get the number of scripts contained in here

        See the `Rust documentation for iter <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter>`__ for more information.


    .. js:method:: script_at(index)

        Get script at index, returning an error if out of bounds

        See the `Rust documentation for iter <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptExtensionsSet.html#method.iter>`__ for more information.


.. js:class:: ICU4XScriptWithExtensions

    An ICU4X ScriptWithExtensions map object, capable of holding a map of codepoints to scriptextensions values

    See the `Rust documentation for ScriptWithExtensions <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensions.html>`__ for more information.


    .. js:function:: create(provider)

        See the `Rust documentation for script_with_extensions <https://docs.rs/icu/latest/icu/properties/script/fn.script_with_extensions.html>`__ for more information.


    .. js:method:: get_script_val(code_point)

        Get the Script property value for a code point

        See the `Rust documentation for get_script_val <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val>`__ for more information.


    .. js:method:: has_script(code_point, script)

        Check if the Script_Extensions property of the given code point covers the given script

        See the `Rust documentation for has_script <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script>`__ for more information.


    .. js:method:: as_borrowed()

        Borrow this object for a slightly faster variant with more operations

        See the `Rust documentation for as_borrowed <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensions.html#method.as_borrowed>`__ for more information.


    .. js:method:: iter_ranges_for_script(script)

        Get a list of ranges of code points that contain this script in their Script_Extensions values

        See the `Rust documentation for get_script_extensions_ranges <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_ranges>`__ for more information.


.. js:class:: ICU4XScriptWithExtensionsBorrowed

    A slightly faster ICU4XScriptWithExtensions object

    See the `Rust documentation for ScriptWithExtensionsBorrowed <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html>`__ for more information.


    .. js:method:: get_script_val(code_point)

        Get the Script property value for a code point

        See the `Rust documentation for get_script_val <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val>`__ for more information.


    .. js:method:: get_script_extensions_val(code_point)

        Get the Script property value for a code point

        See the `Rust documentation for get_script_extensions_val <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_val>`__ for more information.


    .. js:method:: has_script(code_point, script)

        Check if the Script_Extensions property of the given code point covers the given script

        See the `Rust documentation for has_script <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script>`__ for more information.


    .. js:method:: get_script_extensions_set(script)

        Build the CodePointSetData corresponding to a codepoints matching a particular script in their Script_Extensions

        See the `Rust documentation for get_script_extensions_set <https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_set>`__ for more information.

