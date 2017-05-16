This file describes test failures we currently have for stylo.

Failure patterns are described in the following format:
* test_name.html [number]
* test_name.html `pattern` [number]
* test_name.html: description [number]
* test_name.html `pattern`: description [number]
* test_name.html asserts [number]
* test_name.html asserts: description [number]

In which
* test_name.html is the name of the test. It could be "..." which means
  the same name as the previous failure pattern.
* description is a comment for the supposed reason of the failure.
* [number] is the expected count of the failure, which can be a "*" meaning
  any positive number.
* `pattern` is a substring of the failure message. If there are multiple items
  for the same test file, a failure is captured by the first matched pattern.
  For example, if there is a failure with message "foo bar", and there is a
  pattern `foo` followed by a pattern `bar`, this failure would be counted in
  the pattern `foo`.
* "asserts" means it is for assertion. Number of assertions of a same test is
  accumulated, unlike `pattern`. And number of assertions cannot be "*".

Any line which doesn't follow the format above would be ignored like comment.

To use this file, you need to add `--failure-pattern-file=stylo-failures.md`
to mochitest command.

## Failures

* Media query support:
  * test_media_queries.html [38]
  * test_media_queries_dynamic.html [6]
  * test_media_queries_dynamic_xbl.html [2]
  * test_webkit_device_pixel_ratio.html: -webkit-device-pixel-ratio [3]
  * browser_bug453896.js [8]
* Animation support:
  * test_animations.html [1]
  * test_animations_dynamic_changes.html [1]
  * test_bug716226.html [1]
  * inserting keyframes rule doesn't trigger restyle bug 1364799:
    * test_rule_insertion.html `@keyframes` [36]
  * OMTA
    * test_animations_effect_timing_duration.html [1]
    * test_animations_effect_timing_enddelay.html [1]
    * test_animations_effect_timing_iterations.html [1]
    * test_animations_iterationstart.html [1]
    * test_animations_omta.html [1]
    * test_animations_omta_start.html [1]
    * test_animations_pausing.html [1]
    * test_animations_playbackrate.html [1]
    * test_animations_reverse.html [1]
  * SMIL Animation
    * test_restyles_in_smil_animation.html [2]
* CSSOM support:
  * \@import bug 1352968
    * test_bug221428.html [1]
    * test_css_eof_handling.html [1]
  * \@keyframes bug 1345697
    * test_keyframes_rules.html [1]
    * test_rules_out_of_sheets.html [1]
* test_bug357614.html: case-insensitivity for old attrs in attr selector servo/servo#15006 [2]
* test_bug387615.html: servo/servo#15006 [1]
* test_bug397427.html: @import issue bug 1331291 and CSSOM support of @import [1]
* console support bug 1352669
  * test_bug413958.html `monitorConsole` [3]
  * test_parser_diagnostics_unprintables.html [550]
* Transition support:
  * test_transitions.html: pseudo elements [12]
  * Events:
    * test_animations_event_order.html [2]
* test_computed_style.html `gradient`: -webkit-prefixed gradient values [13]
* test_bug829816.html: counter-{reset,increment} serialization difference bug 1363968 [8]
* \@counter-style support bug 1328319
  * test_counter_descriptor_storage.html [1]
  * test_counter_style.html [5]
  * test_value_storage.html `symbols(` [30]
  * ... `list-style-type` [60]
  * ... `'list-style'` [30]
  * ... `'content`: various value as list-style-type in counter functions [2]
  * test_html_attribute_computed_values.html `list-style-type` [8]
  * test_rule_insertion.html `counter` [47]
* @page support:
  * test_bug887741_at-rules_in_declaration_lists.html `exception` [1]
* Unimplemented \@font-face descriptors:
  * font-display bug 1355345
    * test_descriptor_storage.html `font-display` [5]
    * test_font_face_parser.html `font-display` [15]
  * test_font_face_parser.html `font-language-override`: bug 1355364 [8]
  * ... `font-feature-settings`: bug 1355366 [10]
* test_font_face_parser.html `font-weight`: keyword values should be preserved in \@font-face [4]
* @namespace support:
  * test_namespace_rule.html: bug 1355715 [16]
* test_dont_use_document_colors.html: support of disabling document color bug 1355716 [21]
* test_font_feature_values_parsing.html: \@font-feature-values support bug 1355721 [107]
* Grid support bug 1341802
  * test_grid_computed_values.html [4]
  * test_grid_container_shorthands.html [65]
  * test_grid_item_shorthands.html [23]
  * test_grid_shorthand_serialization.html [28]
  * test_compute_data_with_start_struct.html `grid-` [4]
  * test_inherit_computation.html `grid` [8]
  * test_inherit_storage.html `'grid` [15]
  * ... `"grid` [4]
  * test_initial_computation.html `grid` [16]
  * test_initial_storage.html `grid` [38]
  * test_property_syntax_errors.html `grid`: actually there are issues with this [548]
  * test_value_storage.html `'grid` [637]
  * test_exposed_prop_accessors.html `grid` [4]
* Unimplemented prefixed properties:
  * test_variables.html `var(--var6)`: -x-system-font [1]
* Unimplemented CSS properties:
  * font-variant-{alternates,east-asian,ligatures,numeric} properties servo/servo#15957
    * test_property_syntax_errors.html `font-variant-alternates` [2]
    * test_value_storage.html `font-variant` [167]
    * test_specified_value_serialization.html `bug-721136` [1]
* Unsupported prefixed values
  * serialization of prefixed gradient functions bug 1358710
    * test_specified_value_serialization.html `-webkit-radial-gradient` [1]
  * moz-prefixed intrinsic width values bug 1355402
    * test_box_size_keywords.html [16]
    * test_flexbox_flex_shorthand.html `-moz-fit-content` [4]
    * test_value_storage.html `-moz-max-content` [46]
    * ... `-moz-min-content` [6]
    * ... `-moz-fit-content` [6]
    * ... `-moz-available` [4]
  * -webkit-{flex,inline-flex} for display servo/servo#15400
    * test_webkit_flex_display.html [4]
* Unsupported values
  * SVG-in-OpenType values not supported servo/servo#15211 bug 1355412
    * test_value_storage.html `context-` [7]
    * test_bug798843_pref.html [7]
* Incorrect parsing
  * Incorrect bounds
    * test_bug664955.html `font size is larger than max font size` [2]
  * -moz-alt-content parsing is wrong: servo/servo#15726
    * test_property_syntax_errors.html `-moz-alt-content` [4]
  * mask shorthand servo/servo#15772
    * test_property_syntax_errors.html `mask'` [76]
  * different parsing bug 1364260
    * test_supports_rules.html [6]
    * test_condition_text.html [1]
* Incorrect serialization
  * color value not canonicalized servo/servo#15397
    * test_shorthand_property_getters.html `should condense to canonical case` [2]
  * :not(*) doesn't serialize properly servo/servo#16017
    * test_selectors.html `:not()` [8]
    * ... `:not(html|)` [1]
  * "*|a" gets serialized as "a" when it should not servo/servo#16020
    * test_selectors.html `reserialization of *|a` [6]
  * place-{content,items,self} shorthands bug 1363971
    * test_align_shorthand_serialization.html [6]
  * system font serialization with subprop specified bug 1364286
    * test_system_font_serialization.html [5]
  * serialize subprops to -moz-use-system-font when using system font bug 1364289
    * test_value_storage.html `'font'` [144]
* Unsupported pseudo-elements or anon boxes
  * :-moz-tree bits bug 1348488
    * test_selectors.html `:-moz-tree` [10]
  * :-moz-placeholder bug 1348490
    * test_selectors.html `:-moz-placeholder` [1]
* Unsupported pseudo-classes
  * :-moz-locale-dir
    * test_selectors.html `:-moz-locale-dir` [15]
  * :-moz-lwtheme-*
    * test_selectors.html `:-moz-lwtheme` [3]
  * :-moz-window-inactive bug 1348489
    * test_selectors.html `:-moz-window-inactive` [2]
  * :dir
    * test_selectors.html `:dir` [11]
* clamp negative value from calc() servo/servo#15296
  * test_value_storage.html `font-size: calc(` [3]
  * ... `font-size: var(--a)` [3]
* Quirks mode support
  * hashless color servo/servo#15341
    * test_property_syntax_errors.html `color: 000000` [22]
    * ... `color: 96ed2a` [22]
    * ... `color: fff` [4]
  * unitless length servo/servo#15342
    * test_property_syntax_errors.html ` 20 ` [6]
    * ... `: 10 ` [6]
    * ... ` 2 ` [26]
    * ... `: 5 ` [84]
    * ... `border-spacing: ` [6]
    * ... `rect(1, ` [2]
  * test_hover_quirk.html: hover quirks bug 1355724 [6]
* Unit should be preserved after parsing servo/servo#15346
  * test_units_time.html [1]
* getComputedStyle style doesn't contain custom properties bug 1336891
  * test_variable_serialization_computed.html [35]
  * test_variables.html `custom property name` [2]
* test_css_supports.html: issues around @supports syntax servo/servo#15482 [8]
* test_author_specified_style.html: support serializing color as author specified bug 1348165 [27]
* browser_newtab_share_rule_processors.js: agent style sheet sharing [1]
* test_selectors.html `this_better_be_unvisited`: visited handling [1]

## Assertions

## Need Gecko change

* Servo is correct but Gecko is wrong
  * flex-basis should be 0px when omitted in flex shorthand bug 1331530
    * test_flexbox_flex_shorthand.html `flex-basis` [10]
  * should reject whole value bug 1355352
    * test_descriptor_storage.html `unicode-range` [1]
    * test_font_face_parser.html `U+A5` [4]
  * Gecko clamps rather than rejects invalid unicode range bug 1355356
    * test_font_face_parser.html `U+??????` [2]
    * ... `12FFFF` [2]
  * Gecko rejects calc() in -webkit-gradient bug 1363349
    * test_property_syntax_errors.html `-webkit-gradient` [20]
* test_property_syntax_errors.html `linear-gradient(0,`: unitless zero as degree [10]

## Spec Unclear

* test_property_syntax_errors.html `'background'`: whether background shorthand should accept "text" [200]

## Unknown / Unsure

* test_selectors_on_anonymous_content.html: xbl and :nth-child [1]
* test_parse_rule.html `rgb(0, 128, 0)`: color properties not getting computed [5]
* test_selectors.html `:nth-child`: &lt;an+b&gt; parsing difference bug 1364009 [14]

## Ignore

* Ignore for now since should be mostly identical to test_value_storage.html
  * test_value_cloning.html [*]
  * test_value_computation.html [*]
