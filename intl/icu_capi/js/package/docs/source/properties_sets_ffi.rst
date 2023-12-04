``properties_sets::ffi``
========================

.. js:class:: ICU4XCodePointSetData

    An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.

    See the `Rust documentation for properties <https://docs.rs/icu/latest/icu/properties/index.html>`__ for more information.

    See the `Rust documentation for CodePointSetData <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetData.html>`__ for more information.

    See the `Rust documentation for CodePointSetDataBorrowed <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html>`__ for more information.


    .. js:method:: contains(cp)

        Checks whether the code point is in the set.

        See the `Rust documentation for contains <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.contains>`__ for more information.


    .. js:method:: contains32(cp)

        Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.


    .. js:method:: iter_ranges()

        Produces an iterator over ranges of code points contained in this set

        See the `Rust documentation for iter_ranges <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.iter_ranges>`__ for more information.


    .. js:method:: iter_ranges_complemented()

        Produces an iterator over ranges of code points not contained in this set

        See the `Rust documentation for iter_ranges_complemented <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.iter_ranges_complemented>`__ for more information.


    .. js:function:: load_for_general_category_group(provider, group)

        which is a mask with the same format as the ``U_GC_XX_MASK`` mask in ICU4C

        See the `Rust documentation for for_general_category_group <https://docs.rs/icu/latest/icu/properties/sets/fn.for_general_category_group.html>`__ for more information.


    .. js:function:: load_ascii_hex_digit(provider)

        See the `Rust documentation for ascii_hex_digit <https://docs.rs/icu/latest/icu/properties/sets/fn.ascii_hex_digit.html>`__ for more information.


    .. js:function:: load_alnum(provider)

        See the `Rust documentation for alnum <https://docs.rs/icu/latest/icu/properties/sets/fn.alnum.html>`__ for more information.


    .. js:function:: load_alphabetic(provider)

        See the `Rust documentation for alphabetic <https://docs.rs/icu/latest/icu/properties/sets/fn.alphabetic.html>`__ for more information.


    .. js:function:: load_bidi_control(provider)

        See the `Rust documentation for bidi_control <https://docs.rs/icu/latest/icu/properties/sets/fn.bidi_control.html>`__ for more information.


    .. js:function:: load_bidi_mirrored(provider)

        See the `Rust documentation for bidi_mirrored <https://docs.rs/icu/latest/icu/properties/sets/fn.bidi_mirrored.html>`__ for more information.


    .. js:function:: load_blank(provider)

        See the `Rust documentation for blank <https://docs.rs/icu/latest/icu/properties/sets/fn.blank.html>`__ for more information.


    .. js:function:: load_cased(provider)

        See the `Rust documentation for cased <https://docs.rs/icu/latest/icu/properties/sets/fn.cased.html>`__ for more information.


    .. js:function:: load_case_ignorable(provider)

        See the `Rust documentation for case_ignorable <https://docs.rs/icu/latest/icu/properties/sets/fn.case_ignorable.html>`__ for more information.


    .. js:function:: load_full_composition_exclusion(provider)

        See the `Rust documentation for full_composition_exclusion <https://docs.rs/icu/latest/icu/properties/sets/fn.full_composition_exclusion.html>`__ for more information.


    .. js:function:: load_changes_when_casefolded(provider)

        See the `Rust documentation for changes_when_casefolded <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_casefolded.html>`__ for more information.


    .. js:function:: load_changes_when_casemapped(provider)

        See the `Rust documentation for changes_when_casemapped <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_casemapped.html>`__ for more information.


    .. js:function:: load_changes_when_nfkc_casefolded(provider)

        See the `Rust documentation for changes_when_nfkc_casefolded <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_nfkc_casefolded.html>`__ for more information.


    .. js:function:: load_changes_when_lowercased(provider)

        See the `Rust documentation for changes_when_lowercased <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_lowercased.html>`__ for more information.


    .. js:function:: load_changes_when_titlecased(provider)

        See the `Rust documentation for changes_when_titlecased <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_titlecased.html>`__ for more information.


    .. js:function:: load_changes_when_uppercased(provider)

        See the `Rust documentation for changes_when_uppercased <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_uppercased.html>`__ for more information.


    .. js:function:: load_dash(provider)

        See the `Rust documentation for dash <https://docs.rs/icu/latest/icu/properties/sets/fn.dash.html>`__ for more information.


    .. js:function:: load_deprecated(provider)

        See the `Rust documentation for deprecated <https://docs.rs/icu/latest/icu/properties/sets/fn.deprecated.html>`__ for more information.


    .. js:function:: load_default_ignorable_code_point(provider)

        See the `Rust documentation for default_ignorable_code_point <https://docs.rs/icu/latest/icu/properties/sets/fn.default_ignorable_code_point.html>`__ for more information.


    .. js:function:: load_diacritic(provider)

        See the `Rust documentation for diacritic <https://docs.rs/icu/latest/icu/properties/sets/fn.diacritic.html>`__ for more information.


    .. js:function:: load_emoji_modifier_base(provider)

        See the `Rust documentation for emoji_modifier_base <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_modifier_base.html>`__ for more information.


    .. js:function:: load_emoji_component(provider)

        See the `Rust documentation for emoji_component <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_component.html>`__ for more information.


    .. js:function:: load_emoji_modifier(provider)

        See the `Rust documentation for emoji_modifier <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_modifier.html>`__ for more information.


    .. js:function:: load_emoji(provider)

        See the `Rust documentation for emoji <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji.html>`__ for more information.


    .. js:function:: load_emoji_presentation(provider)

        See the `Rust documentation for emoji_presentation <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_presentation.html>`__ for more information.


    .. js:function:: load_extender(provider)

        See the `Rust documentation for extender <https://docs.rs/icu/latest/icu/properties/sets/fn.extender.html>`__ for more information.


    .. js:function:: load_extended_pictographic(provider)

        See the `Rust documentation for extended_pictographic <https://docs.rs/icu/latest/icu/properties/sets/fn.extended_pictographic.html>`__ for more information.


    .. js:function:: load_graph(provider)

        See the `Rust documentation for graph <https://docs.rs/icu/latest/icu/properties/sets/fn.graph.html>`__ for more information.


    .. js:function:: load_grapheme_base(provider)

        See the `Rust documentation for grapheme_base <https://docs.rs/icu/latest/icu/properties/sets/fn.grapheme_base.html>`__ for more information.


    .. js:function:: load_grapheme_extend(provider)

        See the `Rust documentation for grapheme_extend <https://docs.rs/icu/latest/icu/properties/sets/fn.grapheme_extend.html>`__ for more information.


    .. js:function:: load_grapheme_link(provider)

        See the `Rust documentation for grapheme_link <https://docs.rs/icu/latest/icu/properties/sets/fn.grapheme_link.html>`__ for more information.


    .. js:function:: load_hex_digit(provider)

        See the `Rust documentation for hex_digit <https://docs.rs/icu/latest/icu/properties/sets/fn.hex_digit.html>`__ for more information.


    .. js:function:: load_hyphen(provider)

        See the `Rust documentation for hyphen <https://docs.rs/icu/latest/icu/properties/sets/fn.hyphen.html>`__ for more information.


    .. js:function:: load_id_continue(provider)

        See the `Rust documentation for id_continue <https://docs.rs/icu/latest/icu/properties/sets/fn.id_continue.html>`__ for more information.


    .. js:function:: load_ideographic(provider)

        See the `Rust documentation for ideographic <https://docs.rs/icu/latest/icu/properties/sets/fn.ideographic.html>`__ for more information.


    .. js:function:: load_id_start(provider)

        See the `Rust documentation for id_start <https://docs.rs/icu/latest/icu/properties/sets/fn.id_start.html>`__ for more information.


    .. js:function:: load_ids_binary_operator(provider)

        See the `Rust documentation for ids_binary_operator <https://docs.rs/icu/latest/icu/properties/sets/fn.ids_binary_operator.html>`__ for more information.


    .. js:function:: load_ids_trinary_operator(provider)

        See the `Rust documentation for ids_trinary_operator <https://docs.rs/icu/latest/icu/properties/sets/fn.ids_trinary_operator.html>`__ for more information.


    .. js:function:: load_join_control(provider)

        See the `Rust documentation for join_control <https://docs.rs/icu/latest/icu/properties/sets/fn.join_control.html>`__ for more information.


    .. js:function:: load_logical_order_exception(provider)

        See the `Rust documentation for logical_order_exception <https://docs.rs/icu/latest/icu/properties/sets/fn.logical_order_exception.html>`__ for more information.


    .. js:function:: load_lowercase(provider)

        See the `Rust documentation for lowercase <https://docs.rs/icu/latest/icu/properties/sets/fn.lowercase.html>`__ for more information.


    .. js:function:: load_math(provider)

        See the `Rust documentation for math <https://docs.rs/icu/latest/icu/properties/sets/fn.math.html>`__ for more information.


    .. js:function:: load_noncharacter_code_point(provider)

        See the `Rust documentation for noncharacter_code_point <https://docs.rs/icu/latest/icu/properties/sets/fn.noncharacter_code_point.html>`__ for more information.


    .. js:function:: load_nfc_inert(provider)

        See the `Rust documentation for nfc_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfc_inert.html>`__ for more information.


    .. js:function:: load_nfd_inert(provider)

        See the `Rust documentation for nfd_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfd_inert.html>`__ for more information.


    .. js:function:: load_nfkc_inert(provider)

        See the `Rust documentation for nfkc_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfkc_inert.html>`__ for more information.


    .. js:function:: load_nfkd_inert(provider)

        See the `Rust documentation for nfkd_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfkd_inert.html>`__ for more information.


    .. js:function:: load_pattern_syntax(provider)

        See the `Rust documentation for pattern_syntax <https://docs.rs/icu/latest/icu/properties/sets/fn.pattern_syntax.html>`__ for more information.


    .. js:function:: load_pattern_white_space(provider)

        See the `Rust documentation for pattern_white_space <https://docs.rs/icu/latest/icu/properties/sets/fn.pattern_white_space.html>`__ for more information.


    .. js:function:: load_prepended_concatenation_mark(provider)

        See the `Rust documentation for prepended_concatenation_mark <https://docs.rs/icu/latest/icu/properties/sets/fn.prepended_concatenation_mark.html>`__ for more information.


    .. js:function:: load_print(provider)

        See the `Rust documentation for print <https://docs.rs/icu/latest/icu/properties/sets/fn.print.html>`__ for more information.


    .. js:function:: load_quotation_mark(provider)

        See the `Rust documentation for quotation_mark <https://docs.rs/icu/latest/icu/properties/sets/fn.quotation_mark.html>`__ for more information.


    .. js:function:: load_radical(provider)

        See the `Rust documentation for radical <https://docs.rs/icu/latest/icu/properties/sets/fn.radical.html>`__ for more information.


    .. js:function:: load_regional_indicator(provider)

        See the `Rust documentation for regional_indicator <https://docs.rs/icu/latest/icu/properties/sets/fn.regional_indicator.html>`__ for more information.


    .. js:function:: load_soft_dotted(provider)

        See the `Rust documentation for soft_dotted <https://docs.rs/icu/latest/icu/properties/sets/fn.soft_dotted.html>`__ for more information.


    .. js:function:: load_segment_starter(provider)

        See the `Rust documentation for segment_starter <https://docs.rs/icu/latest/icu/properties/sets/fn.segment_starter.html>`__ for more information.


    .. js:function:: load_case_sensitive(provider)

        See the `Rust documentation for case_sensitive <https://docs.rs/icu/latest/icu/properties/sets/fn.case_sensitive.html>`__ for more information.


    .. js:function:: load_sentence_terminal(provider)

        See the `Rust documentation for sentence_terminal <https://docs.rs/icu/latest/icu/properties/sets/fn.sentence_terminal.html>`__ for more information.


    .. js:function:: load_terminal_punctuation(provider)

        See the `Rust documentation for terminal_punctuation <https://docs.rs/icu/latest/icu/properties/sets/fn.terminal_punctuation.html>`__ for more information.


    .. js:function:: load_unified_ideograph(provider)

        See the `Rust documentation for unified_ideograph <https://docs.rs/icu/latest/icu/properties/sets/fn.unified_ideograph.html>`__ for more information.


    .. js:function:: load_uppercase(provider)

        See the `Rust documentation for uppercase <https://docs.rs/icu/latest/icu/properties/sets/fn.uppercase.html>`__ for more information.


    .. js:function:: load_variation_selector(provider)

        See the `Rust documentation for variation_selector <https://docs.rs/icu/latest/icu/properties/sets/fn.variation_selector.html>`__ for more information.


    .. js:function:: load_white_space(provider)

        See the `Rust documentation for white_space <https://docs.rs/icu/latest/icu/properties/sets/fn.white_space.html>`__ for more information.


    .. js:function:: load_xdigit(provider)

        See the `Rust documentation for xdigit <https://docs.rs/icu/latest/icu/properties/sets/fn.xdigit.html>`__ for more information.


    .. js:function:: load_xid_continue(provider)

        See the `Rust documentation for xid_continue <https://docs.rs/icu/latest/icu/properties/sets/fn.xid_continue.html>`__ for more information.


    .. js:function:: load_xid_start(provider)

        See the `Rust documentation for xid_start <https://docs.rs/icu/latest/icu/properties/sets/fn.xid_start.html>`__ for more information.


    .. js:function:: load_for_ecma262(provider, property_name)

        Loads data for a property specified as a string as long as it is one of the `ECMA-262 binary properties <https://tc39.es/ecma262/#table-binary-unicode-properties>`__ (not including Any, ASCII, and Assigned pseudoproperties).

        Returns ``ICU4XError::PropertyUnexpectedPropertyNameError`` in case the string does not match any property in the list

        See the `Rust documentation for for_ecma262 <https://docs.rs/icu/latest/icu/properties/sets/fn.for_ecma262.html>`__ for more information.

