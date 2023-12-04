``casemap::ffi``
================

.. js:class:: ICU4XCaseMapCloser

    See the `Rust documentation for CaseMapCloser <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html>`__ for more information.


    .. js:function:: create(provider)

        Construct a new ICU4XCaseMapper instance

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html#method.new>`__ for more information.


    .. js:method:: add_case_closure_to(c, builder)

        Adds all simple case mappings and the full case folding for ``c`` to ``builder``. Also adds special case closure mappings.

        See the `Rust documentation for add_case_closure_to <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html#method.add_case_closure_to>`__ for more information.


    .. js:method:: add_string_case_closure_to(s, builder)

        Finds all characters and strings which may casemap to ``s`` as their full case folding string and adds them to the set.

        Returns true if the string was found

        See the `Rust documentation for add_string_case_closure_to <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapCloser.html#method.add_string_case_closure_to>`__ for more information.


.. js:class:: ICU4XCaseMapper

    See the `Rust documentation for CaseMapper <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html>`__ for more information.


    .. js:function:: create(provider)

        Construct a new ICU4XCaseMapper instance

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.new>`__ for more information.


    .. js:method:: lowercase(s, locale)

        Returns the full lowercase mapping of the given string

        See the `Rust documentation for lowercase <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.lowercase>`__ for more information.


    .. js:method:: uppercase(s, locale)

        Returns the full uppercase mapping of the given string

        See the `Rust documentation for uppercase <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.uppercase>`__ for more information.


    .. js:method:: titlecase_segment_with_only_case_data_v1(s, locale, options)

        Returns the full titlecase mapping of the given string, performing head adjustment without loading additional data. (if head adjustment is enabled in the options)

        The ``v1`` refers to the version of the options struct, which may change as we add more options

        See the `Rust documentation for titlecase_segment_with_only_case_data <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.titlecase_segment_with_only_case_data>`__ for more information.


    .. js:method:: fold(s)

        Case-folds the characters in the given string

        See the `Rust documentation for fold <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold>`__ for more information.


    .. js:method:: fold_turkic(s)

        Case-folds the characters in the given string using Turkic (T) mappings for dotted/dotless I.

        See the `Rust documentation for fold_turkic <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.fold_turkic>`__ for more information.


    .. js:method:: add_case_closure_to(c, builder)

        Adds all simple case mappings and the full case folding for ``c`` to ``builder``. Also adds special case closure mappings.

        In other words, this adds all characters that this casemaps to, as well as all characters that may casemap to this one.

        Note that since ICU4XCodePointSetBuilder does not contain strings, this will ignore string mappings.

        Identical to the similarly named method on ``ICU4XCaseMapCloser``, use that if you plan on using string case closure mappings too.

        See the `Rust documentation for add_case_closure_to <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.add_case_closure_to>`__ for more information.


    .. js:method:: simple_lowercase(ch)

        Returns the simple lowercase mapping of the given character.

        This function only implements simple and common mappings. Full mappings, which can map one char to a string, are not included. For full mappings, use ``ICU4XCaseMapper::lowercase``.

        See the `Rust documentation for simple_lowercase <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_lowercase>`__ for more information.


    .. js:method:: simple_uppercase(ch)

        Returns the simple uppercase mapping of the given character.

        This function only implements simple and common mappings. Full mappings, which can map one char to a string, are not included. For full mappings, use ``ICU4XCaseMapper::uppercase``.

        See the `Rust documentation for simple_uppercase <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_uppercase>`__ for more information.


    .. js:method:: simple_titlecase(ch)

        Returns the simple titlecase mapping of the given character.

        This function only implements simple and common mappings. Full mappings, which can map one char to a string, are not included. For full mappings, use ``ICU4XCaseMapper::titlecase_segment``.

        See the `Rust documentation for simple_titlecase <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_titlecase>`__ for more information.


    .. js:method:: simple_fold(ch)

        Returns the simple casefolding of the given character.

        This function only implements simple folding. For full folding, use ``ICU4XCaseMapper::fold``.

        See the `Rust documentation for simple_fold <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_fold>`__ for more information.


    .. js:method:: simple_fold_turkic(ch)

        Returns the simple casefolding of the given character in the Turkic locale

        This function only implements simple folding. For full folding, use ``ICU4XCaseMapper::fold_turkic``.

        See the `Rust documentation for simple_fold_turkic <https://docs.rs/icu/latest/icu/casemap/struct.CaseMapper.html#method.simple_fold_turkic>`__ for more information.


.. js:class:: ICU4XLeadingAdjustment

    See the `Rust documentation for LeadingAdjustment <https://docs.rs/icu/latest/icu/casemap/titlecase/enum.LeadingAdjustment.html>`__ for more information.


.. js:class:: ICU4XTitlecaseMapper

    See the `Rust documentation for TitlecaseMapper <https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html>`__ for more information.


    .. js:function:: create(provider)

        Construct a new ``ICU4XTitlecaseMapper`` instance

        See the `Rust documentation for new <https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.new>`__ for more information.


    .. js:method:: titlecase_segment_v1(s, locale, options)

        Returns the full titlecase mapping of the given string

        The ``v1`` refers to the version of the options struct, which may change as we add more options

        See the `Rust documentation for titlecase_segment <https://docs.rs/icu/latest/icu/casemap/struct.TitlecaseMapper.html#method.titlecase_segment>`__ for more information.


.. js:class:: ICU4XTitlecaseOptionsV1

    See the `Rust documentation for TitlecaseOptions <https://docs.rs/icu/latest/icu/casemap/titlecase/struct.TitlecaseOptions.html>`__ for more information.


    .. js:attribute:: leading_adjustment

    .. js:attribute:: trailing_case

    .. js:function:: default_options()

        See the `Rust documentation for default <https://docs.rs/icu/latest/icu/casemap/titlecase/struct.TitlecaseOptions.html#method.default>`__ for more information.


.. js:class:: ICU4XTrailingCase

    See the `Rust documentation for TrailingCase <https://docs.rs/icu/latest/icu/casemap/titlecase/enum.TrailingCase.html>`__ for more information.

