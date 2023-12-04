``locale::ffi``
===============

.. js:class:: ICU4XLocale

    An ICU4X Locale, capable of representing strings like ``"en-US"``.

    See the `Rust documentation for Locale <https://docs.rs/icu/latest/icu/locid/struct.Locale.html>`__ for more information.


    .. js:function:: create_from_string(name)

        Construct an :js:class:`ICU4XLocale` from an locale identifier.

        This will run the complete locale parsing algorithm. If code size and performance are critical and the locale is of a known shape (such as ``aa-BB``) use ``create_und``, ``set_language``, ``set_script``, and ``set_region``.

        See the `Rust documentation for try_from_bytes <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.try_from_bytes>`__ for more information.


    .. js:function:: create_und()

        Construct a default undefined :js:class:`ICU4XLocale` "und".

        See the `Rust documentation for UND <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#associatedconstant.UND>`__ for more information.


    .. js:method:: clone()

        Clones the :js:class:`ICU4XLocale`.

        See the `Rust documentation for Locale <https://docs.rs/icu/latest/icu/locid/struct.Locale.html>`__ for more information.


    .. js:method:: basename()

        Write a string representation of the ``LanguageIdentifier`` part of :js:class:`ICU4XLocale` to ``write``.

        See the `Rust documentation for id <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#structfield.id>`__ for more information.


    .. js:method:: get_unicode_extension(bytes)

        Write a string representation of the unicode extension to ``write``

        See the `Rust documentation for extensions <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#structfield.extensions>`__ for more information.


    .. js:method:: language()

        Write a string representation of :js:class:`ICU4XLocale` language to ``write``

        See the `Rust documentation for id <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#structfield.id>`__ for more information.


    .. js:method:: set_language(bytes)

        Set the language part of the :js:class:`ICU4XLocale`.

        See the `Rust documentation for try_from_bytes <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.try_from_bytes>`__ for more information.


    .. js:method:: region()

        Write a string representation of :js:class:`ICU4XLocale` region to ``write``

        See the `Rust documentation for id <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#structfield.id>`__ for more information.


    .. js:method:: set_region(bytes)

        Set the region part of the :js:class:`ICU4XLocale`.

        See the `Rust documentation for try_from_bytes <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.try_from_bytes>`__ for more information.


    .. js:method:: script()

        Write a string representation of :js:class:`ICU4XLocale` script to ``write``

        See the `Rust documentation for id <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#structfield.id>`__ for more information.


    .. js:method:: set_script(bytes)

        Set the script part of the :js:class:`ICU4XLocale`. Pass an empty string to remove the script.

        See the `Rust documentation for try_from_bytes <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.try_from_bytes>`__ for more information.


    .. js:function:: canonicalize(bytes)

        Best effort locale canonicalizer that doesn't need any data

        Use ICU4XLocaleCanonicalizer for better control and functionality

        See the `Rust documentation for canonicalize <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.canonicalize>`__ for more information.


    .. js:method:: to_string()

        Write a string representation of :js:class:`ICU4XLocale` to ``write``

        See the `Rust documentation for write_to <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.write_to>`__ for more information.


    .. js:method:: normalizing_eq(other)

        See the `Rust documentation for normalizing_eq <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.normalizing_eq>`__ for more information.


    .. js:method:: strict_cmp(other)

        See the `Rust documentation for strict_cmp <https://docs.rs/icu/latest/icu/locid/struct.Locale.html#method.strict_cmp>`__ for more information.


    .. js:function:: create_en()

        Deprecated

        Use `create_from_string("en").


    .. js:function:: create_bn()

        Deprecated

        Use `create_from_string("bn").

