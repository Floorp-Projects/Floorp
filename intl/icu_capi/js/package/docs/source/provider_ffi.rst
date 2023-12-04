``provider::ffi``
=================

.. js:class:: ICU4XDataProvider

    An ICU4X data provider, capable of loading ICU4X data keys from some source.

    See the `Rust documentation for icu_provider <https://docs.rs/icu_provider/latest/icu_provider/index.html>`__ for more information.


    .. js:function:: create_compiled()

        Constructs an :js:class:`ICU4XDataProvider` that uses compiled data.

        Requires the ``compiled_data`` feature.

        This provider cannot be modified or combined with other providers, so ``enable_fallback``, ``enabled_fallback_with``, ``fork_by_locale``, and ``fork_by_key`` will return ``Err``s.


    .. js:function:: create_fs(path)

        Constructs an ``FsDataProvider`` and returns it as an :js:class:`ICU4XDataProvider`. Requires the ``provider_fs`` Cargo feature. Not supported in WASM.

        See the `Rust documentation for FsDataProvider <https://docs.rs/icu_provider_fs/latest/icu_provider_fs/struct.FsDataProvider.html>`__ for more information.


    .. js:function:: create_test()

        Deprecated

        Use ``create_compiled()``.


    .. js:function:: create_from_byte_slice(blob)

        Constructs a ``BlobDataProvider`` and returns it as an :js:class:`ICU4XDataProvider`.

        See the `Rust documentation for BlobDataProvider <https://docs.rs/icu_provider_blob/latest/icu_provider_blob/struct.BlobDataProvider.html>`__ for more information.

        - Note: ``blob`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

        - Warning: This method leaks memory. The parameter `blob` will not be freed as it is required to live for the duration of the program.


    .. js:function:: create_empty()

        Constructs an empty :js:class:`ICU4XDataProvider`.

        See the `Rust documentation for EmptyDataProvider <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/empty/struct.EmptyDataProvider.html>`__ for more information.


    .. js:method:: fork_by_key(other)

        Creates a provider that tries the current provider and then, if the current provider doesn't support the data key, another provider ``other``.

        This takes ownership of the ``other`` provider, leaving an empty provider in its place.

        The providers must be the same type (Any or Buffer). This condition is satisfied if both providers originate from the same constructor, such as ``create_from_byte_slice`` or ``create_fs``. If the condition is not upheld, a runtime error occurs.

        See the `Rust documentation for ForkByKeyProvider <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fork/type.ForkByKeyProvider.html>`__ for more information.


    .. js:method:: fork_by_locale(other)

        Same as ``fork_by_key`` but forks by locale instead of key.

        See the `Rust documentation for MissingLocalePredicate <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fork/predicates/struct.MissingLocalePredicate.html>`__ for more information.


    .. js:method:: enable_locale_fallback()

        Enables locale fallbacking for data requests made to this provider.

        Note that the test provider (from ``create_test``) already has fallbacking enabled.

        See the `Rust documentation for try_new <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.try_new>`__ for more information.

        Additional information: `1 <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html>`__


    .. js:method:: enable_locale_fallback_with(fallbacker)

        See the `Rust documentation for new_with_fallbacker <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html#method.new_with_fallbacker>`__ for more information.

        Additional information: `1 <https://docs.rs/icu_provider_adapters/latest/icu_provider_adapters/fallback/struct.LocaleFallbackProvider.html>`__

