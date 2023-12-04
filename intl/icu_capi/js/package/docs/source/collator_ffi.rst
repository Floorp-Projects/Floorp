``collator::ffi``
=================

.. js:class:: ICU4XCollator

    See the `Rust documentation for Collator <https://docs.rs/icu/latest/icu/collator/struct.Collator.html>`__ for more information.


    .. js:function:: create_v1(provider, locale, options)

        Construct a new Collator instance.

        See the `Rust documentation for try_new <https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.try_new>`__ for more information.


    .. js:method:: compare(left, right)

        Compare potentially ill-formed UTF-8 strings.

        Ill-formed input is compared as if errors had been replaced with REPLACEMENT CHARACTERs according to the WHATWG Encoding Standard.

        See the `Rust documentation for compare_utf8 <https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf8>`__ for more information.


    .. js:method:: compare_valid_utf8(left, right)

        Compare guaranteed well-formed UTF-8 strings.

        Note: In C++, passing ill-formed UTF-8 strings is undefined behavior (and may be memory-unsafe to do so, too).

        See the `Rust documentation for compare <https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare>`__ for more information.


    .. js:method:: compare_utf16(left, right)

        Compare potentially ill-formed UTF-16 strings, with unpaired surrogates compared as REPLACEMENT CHARACTER.

        See the `Rust documentation for compare_utf16 <https://docs.rs/icu/latest/icu/collator/struct.Collator.html#method.compare_utf16>`__ for more information.

        - Note: ``left`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.

        - Note: ``right`` should be an ArrayBuffer or TypedArray corresponding to the slice type expected by Rust.


.. js:class:: ICU4XCollatorAlternateHandling

    See the `Rust documentation for AlternateHandling <https://docs.rs/icu/latest/icu/collator/enum.AlternateHandling.html>`__ for more information.


.. js:class:: ICU4XCollatorBackwardSecondLevel

    See the `Rust documentation for BackwardSecondLevel <https://docs.rs/icu/latest/icu/collator/enum.BackwardSecondLevel.html>`__ for more information.


.. js:class:: ICU4XCollatorCaseFirst

    See the `Rust documentation for CaseFirst <https://docs.rs/icu/latest/icu/collator/enum.CaseFirst.html>`__ for more information.


.. js:class:: ICU4XCollatorCaseLevel

    See the `Rust documentation for CaseLevel <https://docs.rs/icu/latest/icu/collator/enum.CaseLevel.html>`__ for more information.


.. js:class:: ICU4XCollatorMaxVariable

    See the `Rust documentation for MaxVariable <https://docs.rs/icu/latest/icu/collator/enum.MaxVariable.html>`__ for more information.


.. js:class:: ICU4XCollatorNumeric

    See the `Rust documentation for Numeric <https://docs.rs/icu/latest/icu/collator/enum.Numeric.html>`__ for more information.


.. js:class:: ICU4XCollatorOptionsV1

    See the `Rust documentation for CollatorOptions <https://docs.rs/icu/latest/icu/collator/struct.CollatorOptions.html>`__ for more information.


    .. js:attribute:: strength

    .. js:attribute:: alternate_handling

    .. js:attribute:: case_first

    .. js:attribute:: max_variable

    .. js:attribute:: case_level

    .. js:attribute:: numeric

    .. js:attribute:: backward_second_level

.. js:class:: ICU4XCollatorStrength

    See the `Rust documentation for Strength <https://docs.rs/icu/latest/icu/collator/enum.Strength.html>`__ for more information.

