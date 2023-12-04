``collections_sets::ffi``
=========================

.. cpp:class:: ICU4XCodePointSetBuilder

    See the `Rust documentation for CodePointInversionListBuilder <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html>`__ for more information.


    .. cpp:function:: static ICU4XCodePointSetBuilder create()

        Make a new set builder containing nothing

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.new>`__ for more information.


    .. cpp:function:: ICU4XCodePointSetData build()

        Build this into a set

        This object is repopulated with an empty builder

        See the `Rust documentation for build <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.build>`__ for more information.


    .. cpp:function:: void complement()

        Complements this set

        (Elements in this set are removed and vice versa)

        See the `Rust documentation for complement <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement>`__ for more information.


    .. cpp:function:: bool is_empty() const

        Returns whether this set is empty

        See the `Rust documentation for is_empty <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.is_empty>`__ for more information.


    .. cpp:function:: void add_char(char32_t ch)

        Add a single character to the set

        See the `Rust documentation for add_char <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_char>`__ for more information.


    .. cpp:function:: void add_u32(uint32_t ch)

        Add a single u32 value to the set

        See the `Rust documentation for add_u32 <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_u32>`__ for more information.


    .. cpp:function:: void add_inclusive_range(char32_t start, char32_t end)

        Add an inclusive range of characters to the set

        See the `Rust documentation for add_range <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range>`__ for more information.


    .. cpp:function:: void add_inclusive_range_u32(uint32_t start, uint32_t end)

        Add an inclusive range of u32s to the set

        See the `Rust documentation for add_range_u32 <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_range_u32>`__ for more information.


    .. cpp:function:: void add_set(const ICU4XCodePointSetData& data)

        Add all elements that belong to the provided set to the set

        See the `Rust documentation for add_set <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.add_set>`__ for more information.


    .. cpp:function:: void remove_char(char32_t ch)

        Remove a single character to the set

        See the `Rust documentation for remove_char <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_char>`__ for more information.


    .. cpp:function:: void remove_inclusive_range(char32_t start, char32_t end)

        Remove an inclusive range of characters from the set

        See the `Rust documentation for remove_range <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_range>`__ for more information.


    .. cpp:function:: void remove_set(const ICU4XCodePointSetData& data)

        Remove all elements that belong to the provided set from the set

        See the `Rust documentation for remove_set <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.remove_set>`__ for more information.


    .. cpp:function:: void retain_char(char32_t ch)

        Removes all elements from the set except a single character

        See the `Rust documentation for retain_char <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_char>`__ for more information.


    .. cpp:function:: void retain_inclusive_range(char32_t start, char32_t end)

        Removes all elements from the set except an inclusive range of characters f

        See the `Rust documentation for retain_range <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_range>`__ for more information.


    .. cpp:function:: void retain_set(const ICU4XCodePointSetData& data)

        Removes all elements from the set except all elements in the provided set

        See the `Rust documentation for retain_set <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.retain_set>`__ for more information.


    .. cpp:function:: void complement_char(char32_t ch)

        Complement a single character to the set

        (Characters which are in this set are removed and vice versa)

        See the `Rust documentation for complement_char <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_char>`__ for more information.


    .. cpp:function:: void complement_inclusive_range(char32_t start, char32_t end)

        Complement an inclusive range of characters from the set

        (Characters which are in this set are removed and vice versa)

        See the `Rust documentation for complement_range <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_range>`__ for more information.


    .. cpp:function:: void complement_set(const ICU4XCodePointSetData& data)

        Complement all elements that belong to the provided set from the set

        (Characters which are in this set are removed and vice versa)

        See the `Rust documentation for complement_set <https://docs.rs/icu/latest/icu/collections/codepointinvlist/struct.CodePointInversionListBuilder.html#method.complement_set>`__ for more information.

