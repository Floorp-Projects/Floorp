``properties_sets::ffi``
========================

.. cpp:class:: ICU4XCodePointSetData

    An ICU4X Unicode Set Property object, capable of querying whether a code point is contained in a set based on a Unicode property.

    See the `Rust documentation for properties <https://docs.rs/icu/latest/icu/properties/index.html>`__ for more information.

    See the `Rust documentation for CodePointSetData <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetData.html>`__ for more information.

    See the `Rust documentation for CodePointSetDataBorrowed <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html>`__ for more information.


    .. cpp:function:: bool contains(char32_t cp) const

        Checks whether the code point is in the set.

        See the `Rust documentation for contains <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.contains>`__ for more information.


    .. cpp:function:: bool contains32(uint32_t cp) const

        Checks whether the code point (specified as a 32 bit integer, in UTF-32) is in the set.


    .. cpp:function:: CodePointRangeIterator iter_ranges() const

        Produces an iterator over ranges of code points contained in this set

        See the `Rust documentation for iter_ranges <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.iter_ranges>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: CodePointRangeIterator iter_ranges_complemented() const

        Produces an iterator over ranges of code points not contained in this set

        See the `Rust documentation for iter_ranges_complemented <https://docs.rs/icu/latest/icu/properties/sets/struct.CodePointSetDataBorrowed.html#method.iter_ranges_complemented>`__ for more information.

        Lifetimes: ``this`` must live at least as long as the output.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_for_general_category_group(const ICU4XDataProvider& provider, uint32_t group)

        which is a mask with the same format as the ``U_GC_XX_MASK`` mask in ICU4C

        See the `Rust documentation for for_general_category_group <https://docs.rs/icu/latest/icu/properties/sets/fn.for_general_category_group.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_ascii_hex_digit(const ICU4XDataProvider& provider)

        See the `Rust documentation for ascii_hex_digit <https://docs.rs/icu/latest/icu/properties/sets/fn.ascii_hex_digit.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_alnum(const ICU4XDataProvider& provider)

        See the `Rust documentation for alnum <https://docs.rs/icu/latest/icu/properties/sets/fn.alnum.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_alphabetic(const ICU4XDataProvider& provider)

        See the `Rust documentation for alphabetic <https://docs.rs/icu/latest/icu/properties/sets/fn.alphabetic.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_bidi_control(const ICU4XDataProvider& provider)

        See the `Rust documentation for bidi_control <https://docs.rs/icu/latest/icu/properties/sets/fn.bidi_control.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_bidi_mirrored(const ICU4XDataProvider& provider)

        See the `Rust documentation for bidi_mirrored <https://docs.rs/icu/latest/icu/properties/sets/fn.bidi_mirrored.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_blank(const ICU4XDataProvider& provider)

        See the `Rust documentation for blank <https://docs.rs/icu/latest/icu/properties/sets/fn.blank.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_cased(const ICU4XDataProvider& provider)

        See the `Rust documentation for cased <https://docs.rs/icu/latest/icu/properties/sets/fn.cased.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_case_ignorable(const ICU4XDataProvider& provider)

        See the `Rust documentation for case_ignorable <https://docs.rs/icu/latest/icu/properties/sets/fn.case_ignorable.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_full_composition_exclusion(const ICU4XDataProvider& provider)

        See the `Rust documentation for full_composition_exclusion <https://docs.rs/icu/latest/icu/properties/sets/fn.full_composition_exclusion.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_changes_when_casefolded(const ICU4XDataProvider& provider)

        See the `Rust documentation for changes_when_casefolded <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_casefolded.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_changes_when_casemapped(const ICU4XDataProvider& provider)

        See the `Rust documentation for changes_when_casemapped <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_casemapped.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_changes_when_nfkc_casefolded(const ICU4XDataProvider& provider)

        See the `Rust documentation for changes_when_nfkc_casefolded <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_nfkc_casefolded.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_changes_when_lowercased(const ICU4XDataProvider& provider)

        See the `Rust documentation for changes_when_lowercased <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_lowercased.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_changes_when_titlecased(const ICU4XDataProvider& provider)

        See the `Rust documentation for changes_when_titlecased <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_titlecased.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_changes_when_uppercased(const ICU4XDataProvider& provider)

        See the `Rust documentation for changes_when_uppercased <https://docs.rs/icu/latest/icu/properties/sets/fn.changes_when_uppercased.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_dash(const ICU4XDataProvider& provider)

        See the `Rust documentation for dash <https://docs.rs/icu/latest/icu/properties/sets/fn.dash.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_deprecated(const ICU4XDataProvider& provider)

        See the `Rust documentation for deprecated <https://docs.rs/icu/latest/icu/properties/sets/fn.deprecated.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_default_ignorable_code_point(const ICU4XDataProvider& provider)

        See the `Rust documentation for default_ignorable_code_point <https://docs.rs/icu/latest/icu/properties/sets/fn.default_ignorable_code_point.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_diacritic(const ICU4XDataProvider& provider)

        See the `Rust documentation for diacritic <https://docs.rs/icu/latest/icu/properties/sets/fn.diacritic.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_emoji_modifier_base(const ICU4XDataProvider& provider)

        See the `Rust documentation for emoji_modifier_base <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_modifier_base.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_emoji_component(const ICU4XDataProvider& provider)

        See the `Rust documentation for emoji_component <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_component.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_emoji_modifier(const ICU4XDataProvider& provider)

        See the `Rust documentation for emoji_modifier <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_modifier.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_emoji(const ICU4XDataProvider& provider)

        See the `Rust documentation for emoji <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_emoji_presentation(const ICU4XDataProvider& provider)

        See the `Rust documentation for emoji_presentation <https://docs.rs/icu/latest/icu/properties/sets/fn.emoji_presentation.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_extender(const ICU4XDataProvider& provider)

        See the `Rust documentation for extender <https://docs.rs/icu/latest/icu/properties/sets/fn.extender.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_extended_pictographic(const ICU4XDataProvider& provider)

        See the `Rust documentation for extended_pictographic <https://docs.rs/icu/latest/icu/properties/sets/fn.extended_pictographic.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_graph(const ICU4XDataProvider& provider)

        See the `Rust documentation for graph <https://docs.rs/icu/latest/icu/properties/sets/fn.graph.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_grapheme_base(const ICU4XDataProvider& provider)

        See the `Rust documentation for grapheme_base <https://docs.rs/icu/latest/icu/properties/sets/fn.grapheme_base.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_grapheme_extend(const ICU4XDataProvider& provider)

        See the `Rust documentation for grapheme_extend <https://docs.rs/icu/latest/icu/properties/sets/fn.grapheme_extend.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_grapheme_link(const ICU4XDataProvider& provider)

        See the `Rust documentation for grapheme_link <https://docs.rs/icu/latest/icu/properties/sets/fn.grapheme_link.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_hex_digit(const ICU4XDataProvider& provider)

        See the `Rust documentation for hex_digit <https://docs.rs/icu/latest/icu/properties/sets/fn.hex_digit.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_hyphen(const ICU4XDataProvider& provider)

        See the `Rust documentation for hyphen <https://docs.rs/icu/latest/icu/properties/sets/fn.hyphen.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_id_continue(const ICU4XDataProvider& provider)

        See the `Rust documentation for id_continue <https://docs.rs/icu/latest/icu/properties/sets/fn.id_continue.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_ideographic(const ICU4XDataProvider& provider)

        See the `Rust documentation for ideographic <https://docs.rs/icu/latest/icu/properties/sets/fn.ideographic.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_id_start(const ICU4XDataProvider& provider)

        See the `Rust documentation for id_start <https://docs.rs/icu/latest/icu/properties/sets/fn.id_start.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_ids_binary_operator(const ICU4XDataProvider& provider)

        See the `Rust documentation for ids_binary_operator <https://docs.rs/icu/latest/icu/properties/sets/fn.ids_binary_operator.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_ids_trinary_operator(const ICU4XDataProvider& provider)

        See the `Rust documentation for ids_trinary_operator <https://docs.rs/icu/latest/icu/properties/sets/fn.ids_trinary_operator.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_join_control(const ICU4XDataProvider& provider)

        See the `Rust documentation for join_control <https://docs.rs/icu/latest/icu/properties/sets/fn.join_control.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_logical_order_exception(const ICU4XDataProvider& provider)

        See the `Rust documentation for logical_order_exception <https://docs.rs/icu/latest/icu/properties/sets/fn.logical_order_exception.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_lowercase(const ICU4XDataProvider& provider)

        See the `Rust documentation for lowercase <https://docs.rs/icu/latest/icu/properties/sets/fn.lowercase.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_math(const ICU4XDataProvider& provider)

        See the `Rust documentation for math <https://docs.rs/icu/latest/icu/properties/sets/fn.math.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_noncharacter_code_point(const ICU4XDataProvider& provider)

        See the `Rust documentation for noncharacter_code_point <https://docs.rs/icu/latest/icu/properties/sets/fn.noncharacter_code_point.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_nfc_inert(const ICU4XDataProvider& provider)

        See the `Rust documentation for nfc_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfc_inert.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_nfd_inert(const ICU4XDataProvider& provider)

        See the `Rust documentation for nfd_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfd_inert.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_nfkc_inert(const ICU4XDataProvider& provider)

        See the `Rust documentation for nfkc_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfkc_inert.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_nfkd_inert(const ICU4XDataProvider& provider)

        See the `Rust documentation for nfkd_inert <https://docs.rs/icu/latest/icu/properties/sets/fn.nfkd_inert.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_pattern_syntax(const ICU4XDataProvider& provider)

        See the `Rust documentation for pattern_syntax <https://docs.rs/icu/latest/icu/properties/sets/fn.pattern_syntax.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_pattern_white_space(const ICU4XDataProvider& provider)

        See the `Rust documentation for pattern_white_space <https://docs.rs/icu/latest/icu/properties/sets/fn.pattern_white_space.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_prepended_concatenation_mark(const ICU4XDataProvider& provider)

        See the `Rust documentation for prepended_concatenation_mark <https://docs.rs/icu/latest/icu/properties/sets/fn.prepended_concatenation_mark.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_print(const ICU4XDataProvider& provider)

        See the `Rust documentation for print <https://docs.rs/icu/latest/icu/properties/sets/fn.print.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_quotation_mark(const ICU4XDataProvider& provider)

        See the `Rust documentation for quotation_mark <https://docs.rs/icu/latest/icu/properties/sets/fn.quotation_mark.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_radical(const ICU4XDataProvider& provider)

        See the `Rust documentation for radical <https://docs.rs/icu/latest/icu/properties/sets/fn.radical.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_regional_indicator(const ICU4XDataProvider& provider)

        See the `Rust documentation for regional_indicator <https://docs.rs/icu/latest/icu/properties/sets/fn.regional_indicator.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_soft_dotted(const ICU4XDataProvider& provider)

        See the `Rust documentation for soft_dotted <https://docs.rs/icu/latest/icu/properties/sets/fn.soft_dotted.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_segment_starter(const ICU4XDataProvider& provider)

        See the `Rust documentation for segment_starter <https://docs.rs/icu/latest/icu/properties/sets/fn.segment_starter.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_case_sensitive(const ICU4XDataProvider& provider)

        See the `Rust documentation for case_sensitive <https://docs.rs/icu/latest/icu/properties/sets/fn.case_sensitive.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_sentence_terminal(const ICU4XDataProvider& provider)

        See the `Rust documentation for sentence_terminal <https://docs.rs/icu/latest/icu/properties/sets/fn.sentence_terminal.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_terminal_punctuation(const ICU4XDataProvider& provider)

        See the `Rust documentation for terminal_punctuation <https://docs.rs/icu/latest/icu/properties/sets/fn.terminal_punctuation.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_unified_ideograph(const ICU4XDataProvider& provider)

        See the `Rust documentation for unified_ideograph <https://docs.rs/icu/latest/icu/properties/sets/fn.unified_ideograph.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_uppercase(const ICU4XDataProvider& provider)

        See the `Rust documentation for uppercase <https://docs.rs/icu/latest/icu/properties/sets/fn.uppercase.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_variation_selector(const ICU4XDataProvider& provider)

        See the `Rust documentation for variation_selector <https://docs.rs/icu/latest/icu/properties/sets/fn.variation_selector.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_white_space(const ICU4XDataProvider& provider)

        See the `Rust documentation for white_space <https://docs.rs/icu/latest/icu/properties/sets/fn.white_space.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_xdigit(const ICU4XDataProvider& provider)

        See the `Rust documentation for xdigit <https://docs.rs/icu/latest/icu/properties/sets/fn.xdigit.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_xid_continue(const ICU4XDataProvider& provider)

        See the `Rust documentation for xid_continue <https://docs.rs/icu/latest/icu/properties/sets/fn.xid_continue.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_xid_start(const ICU4XDataProvider& provider)

        See the `Rust documentation for xid_start <https://docs.rs/icu/latest/icu/properties/sets/fn.xid_start.html>`__ for more information.


    .. cpp:function:: static diplomat::result<ICU4XCodePointSetData, ICU4XError> load_for_ecma262(const ICU4XDataProvider& provider, const std::string_view property_name)

        Loads data for a property specified as a string as long as it is one of the `ECMA-262 binary properties <https://tc39.es/ecma262/#table-binary-unicode-properties>`__ (not including Any, ASCII, and Assigned pseudoproperties).

        Returns ``ICU4XError::PropertyUnexpectedPropertyNameError`` in case the string does not match any property in the list

        See the `Rust documentation for for_ecma262 <https://docs.rs/icu/latest/icu/properties/sets/fn.for_ecma262.html>`__ for more information.

