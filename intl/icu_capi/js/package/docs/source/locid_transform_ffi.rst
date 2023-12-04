``locid_transform::ffi``
========================

.. js:class:: ICU4XLocaleCanonicalizer

    A locale canonicalizer.

    See the `Rust documentation for LocaleCanonicalizer <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html>`__ for more information.


    .. js:function:: create(provider)

        Create a new :js:class:`ICU4XLocaleCanonicalizer`.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.new>`__ for more information.


    .. js:function:: create_extended(provider)

        Create a new :js:class:`ICU4XLocaleCanonicalizer` with extended data.

        See the `Rust documentation for new_with_expander <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.new_with_expander>`__ for more information.


    .. js:method:: canonicalize(locale)

        FFI version of ``LocaleCanonicalizer::canonicalize()``.

        See the `Rust documentation for canonicalize <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleCanonicalizer.html#method.canonicalize>`__ for more information.


.. js:class:: ICU4XLocaleExpander

    A locale expander.

    See the `Rust documentation for LocaleExpander <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html>`__ for more information.


    .. js:function:: create(provider)

        Create a new :js:class:`ICU4XLocaleExpander`.

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.new>`__ for more information.


    .. js:function:: create_extended(provider)

        Create a new :js:class:`ICU4XLocaleExpander` with extended data.

        See the `Rust documentation for new_extended <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.new_extended>`__ for more information.


    .. js:method:: maximize(locale)

        FFI version of ``LocaleExpander::maximize()``.

        See the `Rust documentation for maximize <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.maximize>`__ for more information.


    .. js:method:: minimize(locale)

        FFI version of ``LocaleExpander::minimize()``.

        See the `Rust documentation for minimize <https://docs.rs/icu/latest/icu/locid_transform/struct.LocaleExpander.html#method.minimize>`__ for more information.


.. js:class:: ICU4XTransformResult

    FFI version of ``TransformResult``.

    See the `Rust documentation for TransformResult <https://docs.rs/icu/latest/icu/locid_transform/enum.TransformResult.html>`__ for more information.

