``list::ffi``
=============

.. js:class:: ICU4XList

    A list of strings


    .. js:function:: create()

        Create a new list of strings


    .. js:function:: create_with_capacity(capacity)

        Create a new list of strings with preallocated space to hold at least ``capacity`` elements


    .. js:method:: push(val)

        Push a string to the list

        For C++ users, potentially invalid UTF8 will be handled via REPLACEMENT CHARACTERs


    .. js:method:: len()

        The number of elements in this list


.. js:class:: ICU4XListFormatter

    See the `Rust documentation for ListFormatter <https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html>`__ for more information.


    .. js:function:: create_and_with_length(provider, locale, length)

        Construct a new ICU4XListFormatter instance for And patterns

        See the `Rust documentation for try_new_and_with_length <https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.try_new_and_with_length>`__ for more information.


    .. js:function:: create_or_with_length(provider, locale, length)

        Construct a new ICU4XListFormatter instance for And patterns

        See the `Rust documentation for try_new_or_with_length <https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.try_new_or_with_length>`__ for more information.


    .. js:function:: create_unit_with_length(provider, locale, length)

        Construct a new ICU4XListFormatter instance for And patterns

        See the `Rust documentation for try_new_unit_with_length <https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.try_new_unit_with_length>`__ for more information.


    .. js:method:: format(list)

        See the `Rust documentation for format <https://docs.rs/icu/latest/icu/list/struct.ListFormatter.html#method.format>`__ for more information.


.. js:class:: ICU4XListLength

    See the `Rust documentation for ListLength <https://docs.rs/icu/latest/icu/list/enum.ListLength.html>`__ for more information.

