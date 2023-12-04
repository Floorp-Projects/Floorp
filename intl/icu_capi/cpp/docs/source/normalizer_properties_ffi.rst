``normalizer_properties::ffi``
==============================

.. cpp:class:: ICU4XCanonicalCombiningClassMap

    Lookup of the Canonical_Combining_Class Unicode property

    See the `Rust documentation for CanonicalCombiningClassMap <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCanonicalCombiningClassMap, ICU4XError> create(const ICU4XDataProvider& provider)

        Construct a new ICU4XCanonicalCombiningClassMap instance for NFC

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.new>`__ for more information.


    .. cpp:function:: uint8_t get(char32_t ch) const

        See the `Rust documentation for get <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.get>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/properties/properties/struct.CanonicalCombiningClass.html>`__


    .. cpp:function:: uint8_t get32(uint32_t ch) const

        See the `Rust documentation for get32 <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalCombiningClassMap.html#method.get32>`__ for more information.

        Additional information: `1 <https://docs.rs/icu/latest/icu/properties/properties/struct.CanonicalCombiningClass.html>`__


.. cpp:class:: ICU4XCanonicalComposition

    The raw canonical composition operation.

    Callers should generally use ICU4XComposingNormalizer unless they specifically need raw composition operations

    See the `Rust documentation for CanonicalComposition <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCanonicalComposition, ICU4XError> create(const ICU4XDataProvider& provider)

        Construct a new ICU4XCanonicalComposition instance for NFC

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html#method.new>`__ for more information.


    .. cpp:function:: char32_t compose(char32_t starter, char32_t second) const

        Performs canonical composition (including Hangul) on a pair of characters or returns NUL if these characters don’t compose. Composition exclusions are taken into account.

        See the `Rust documentation for compose <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalComposition.html#method.compose>`__ for more information.


.. cpp:class:: ICU4XCanonicalDecomposition

    The raw (non-recursive) canonical decomposition operation.

    Callers should generally use ICU4XDecomposingNormalizer unless they specifically need raw composition operations

    See the `Rust documentation for CanonicalDecomposition <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalDecomposition.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCanonicalDecomposition, ICU4XError> create(const ICU4XDataProvider& provider)

        Construct a new ICU4XCanonicalDecomposition instance for NFC

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalDecomposition.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XDecomposed decompose(char32_t c) const

        Performs non-recursive canonical decomposition (including for Hangul).

        See the `Rust documentation for decompose <https://docs.rs/icu/latest/icu/normalizer/properties/struct.CanonicalDecomposition.html#method.decompose>`__ for more information.


.. cpp:struct:: ICU4XDecomposed

    The outcome of non-recursive canonical decomposition of a character. ``second`` will be NUL when the decomposition expands to a single character (which may or may not be the original one)

    See the `Rust documentation for Decomposed <https://docs.rs/icu/latest/icu/normalizer/properties/enum.Decomposed.html>`__ for more information.


    .. cpp:member:: char32_t first

    .. cpp:member:: char32_t second
