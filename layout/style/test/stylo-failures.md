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
  * test_bug418986-2.html: matchMedia support [3]
  * test_bug453896_deck.html: &lt;style media&gt; support [8]
  * test_media_queries.html [637]
  * test_media_queries_dynamic.html [11]
  * test_media_queries_dynamic_xbl.html [2]
  * test_webkit_device_pixel_ratio.html: -webkit-device-pixel-ratio [3]
  * browser_bug453896.js [8]
  * test_display_mode.html [7]
  * test_display_mode_reflow.html [2]
* test_all_shorthand.html: all shorthand servo/servo#15055 [*]
* Animation support:
  * test_animations.html [22]
  * test_animations_dynamic_changes.html [1]
  * test_bug716226.html [1]
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
  * CSS Timing Functions: Frames timing functions
    * test_value_storage.html `frames` [30]
  * Property parsing and computation:
    * test_property_syntax_errors.html `animation` [404]
    * test_value_storage.html `animation` [91]
* CSSOM support:
  * @import
    * test_bug221428.html [1]
    * test_css_eof_handling.html: also relies on \@-moz-document [1]
  * @keyframes
    * test_keyframes_rules.html [1]
    * test_rules_out_of_sheets.html [1]
  * @support
    * test_supports_rules.html [1]
* test_box_size_keywords.html: moz-prefixed intrinsic size keyword value [16]
* test_bug357614.html: case-insensitivity for old attrs in attr selector servo/servo#15006 [2]
* mapped attribute not supported
  * test_html_attribute_computed_values.html: also list-style-type [8]
* test_bug387615.html: servo/servo#15006 [1]
* test_bug397427.html: @import issue bug 1331291 and CSSOM support of @import [1]
* console support:
  * test_bug413958.html `monitorConsole` [3]
  * test_parser_diagnostics_unprintables.html [550]
* test_bug511909.html: @-moz-document and @media support [4]
* Transition support:
  * test_bug621351.html [4]
  * test_compute_data_with_start_struct.html `transition` [2]
  * test_transitions.html [63]
  * test_transitions_and_reframes.html [16]
  * test_transitions_and_restyles.html [3]
  * test_transitions_computed_value_combinations.html [145]
  * test_transitions_dynamic_changes.html [10]
  * test_transitions_step_functions.html [24]
  * test_value_storage.html `transition` [596]
  * Events:
    * test_animations_event_handler_attribute.html [10]
    * test_animations_event_order.html [11]
* test_bug798843_pref.html: conditional opentype svg support [7]
* test_computed_style.html `gradient`: -moz-prefixed radient value [9]
* ... `mask`: mask-image isn't set properly bug 1341667 [10]
* ... `fill`: svg paint should distinguish whether there is fallback bug 1347409 [2]
* ... `stroke`: svg paint should distinguish whether there is fallback bug 1347409 [2]
* character not properly escaped servo/servo#15947
  * test_parse_url.html [4]
  * test_bug829816.html [8]
* auto value for min-{width,height} servo/servo#15045
* test_compute_data_with_start_struct.html `timing-function`: incorrectly computing keywords to bezier function servo/servo#15086 [2]
* test_condition_text.html: @-moz-document, CSSOM support of @media, @support [7]
* @counter-style support:
  * test_counter_descriptor_storage.html [1]
  * test_counter_style.html [1]
  * test_rule_insertion.html `@counter-style` [4]
  * ... `cjk-decimal` [1]
  * test_value_storage.html `symbols(` [30]
  * ... `list-style-type` [60]
  * ... `'list-style'` [30]
  * ... `'content`: various value as list-style-type in counter functions [13]
* @page support:
  * test_bug887741_at-rules_in_declaration_lists.html `exception` [1]
* test_default_computed_style.html: support of getDefaultComputedStyle [1]
* Unimplemented \@font-face descriptors:
  * font-display bug 1355345
    * test_descriptor_storage.html `font-display` [5]
    * test_font_face_parser.html `font-display` [15]
  * test_font_face_parser.html `font-language-override`: bug 1355364 [8]
  * ... `font-feature-settings`: bug 1355366 [10]
* test_font_face_parser.html `font-weight`: keyword values should be preserved in \@font-face [4]
* unicode-range parsing bugs
  * servo/rust-cssparser#133
    * test_descriptor_storage.html `U+4????` [1]
    * test_font_face_parser.html `U+0121` [4]
  * test_font_face_parser.html `4E00`: servo/rust-cssparser#135 [2]
* @namespace support:
  * test_namespace_rule.html [17]
* test_dont_use_document_colors.html: support of disabling document color [21]
* test_exposed_prop_accessors.html: mainly various unsupported properties [*]
* test_extra_inherit_initial.html: CSS-wide keywords are accepted as part of value servo/servo#15054 [980]
* test_font_feature_values_parsing.html: @font-feature-values support [107]
* Grid support:
  * test_grid_computed_values.html [4]
  * test_grid_container_shorthands.html [65]
  * test_grid_item_shorthands.html [23]
  * test_grid_shorthand_serialization.html [28]
  * test_compute_data_with_start_struct.html `grid-` [*]
  * test_inherit_computation.html `grid` [*]
  * test_inherit_storage.html `'grid` [*]
  * ... `"grid` [*]
  * test_initial_computation.html `grid` [*]
  * test_initial_storage.html `grid` [*]
  * test_property_syntax_errors.html `grid`: actually there are issues with this [*]
  * test_value_storage.html `'grid` [*]
* test_hover_quirk.html: hover quirks [6]
* url value from decl setter bug 1330503
  * test_compute_data_with_start_struct.html `border-image-source` [2]
  * test_inherit_computation.html `border-image` [2]
  * test_initial_computation.html `border-image` [4]
* Unimplemented prefixed properties:
  * -moz-border-*-colors bug 1348173
    * test_compute_data_with_start_struct.html `-colors` [8]
    * test_inherit_computation.html `-colors` [8]
    * test_inherit_storage.html `-colors` [12]
    * test_initial_computation.html `-colors` [16]
    * test_initial_storage.html `-colors` [24]
    * test_value_storage.html `-colors` [96]
    * test_shorthand_property_getters.html `-colors` [1]
  * -moz-force-broken-image-icon servo/servo#16001
    * test_compute_data_with_start_struct.html `-moz-force-broken-image-icon` [2]
    * test_inherit_computation.html `-moz-force-broken-image-icon` [2]
    * test_inherit_storage.html `-moz-force-broken-image-icon` [2]
    * test_initial_computation.html `-moz-force-broken-image-icon` [4]
    * test_initial_storage.html `-moz-force-broken-image-icon` [4]
    * test_value_storage.html `-moz-force-broken-image-icon` [4]
  * -moz-transform: need different parsing rules servo/servo#16003
    * test_inherit_computation.html `-moz-transform`: need different parsing rules [2]
    * test_inherit_storage.html `transform`: for -moz-transform [3]
    * test_initial_computation.html `-moz-transform`: need different parsing rules [4]
    * test_initial_storage.html `transform`: for -moz-transform [6]
    * test_value_storage.html `-moz-transform`: need different parsing rules [284]
    * test_specified_value_serialization.html `bug-721136` [26]
    * test_units_angle.html [3]
  * test_variables.html `var(--var6)`: -x-system-font [1]
* Unimplemented CSS properties:
  * place-{content,items,self} shorthands servo/servo#15954
    * test_align_shorthand_serialization.html [8]
    * test_inherit_computation.html `place-` [6]
    * test_inherit_storage.html `place-` [6]
    * test_initial_computation.html `place-` [12]
    * test_initial_storage.html `place-` [12]
    * test_value_storage.html `place-` [132]
  * font-variant-{alternates,east-asian,ligatures,numeric} properties servo/servo#15957
    * test_compute_data_with_start_struct.html `font-variant` [8]
    * test_inherit_computation.html `font-variant` [20]
    * test_inherit_storage.html `font-variant` [36]
    * test_initial_computation.html `font-variant` [10]
    * test_initial_storage.html `font-variant` [18]
    * test_value_storage.html `font-variant` [332]
  * shape-outside property servo/servo#15958
    * test_compute_data_with_start_struct.html `shape-outside` [2]
    * test_inherit_computation.html `shape-outside` [2]
    * test_inherit_storage.html `shape-outside` [2]
    * test_initial_computation.html `shape-outside` [4]
    * test_initial_storage.html `shape-outside` [4]
    * test_value_storage.html `shape-outside` [121]
  * touch-action property
    * test_compute_data_with_start_struct.html `touch-action` [2]
    * test_inherit_computation.html `touch-action` [2]
    * test_inherit_storage.html `touch-action` [2]
    * test_initial_computation.html `touch-action` [4]
    * test_initial_storage.html `touch-action` [4]
    * test_value_storage.html `touch-action` [14]
* Unimplemented SVG properties:
  * stroke properties
    * test_value_storage.html `on 'stroke` [6]
    * test_compute_data_with_start_struct.html `initial and other values of stroke-dasharray are different` [2]
* Properties implemented but not in geckolib:
  * contain longhand property bug 1354998
    * test_contain_formatting_context.html [1]
    * test_compute_data_with_start_struct.html `contain` [2]
    * test_inherit_computation.html `contain` [2]
    * test_inherit_storage.html `contain` [2]
    * test_initial_computation.html `contain` [4]
    * test_initial_storage.html `contain` [4]
    * test_value_storage.html `'contain'` [30]
  * font-feature-settings property servo/servo#15975
    * test_compute_data_with_start_struct.html `font-feature-settings` [2]
    * test_inherit_computation.html `font-feature-settings` [8]
    * test_inherit_storage.html `font-feature-settings` [12]
    * test_initial_computation.html `font-feature-settings` [4]
    * test_initial_storage.html `font-feature-settings` [6]
    * test_value_storage.html `font-feature-settings` [112]
  * image-orientation property
    * test_value_storage.html `image-orientation` [40]
  * flexbox / grid position properties servo/servo#15001
    * test_inherit_storage.html `align-` [3]
    * ... `justify-` [3]
    * test_initial_storage.html `align-` [6]
    * ... `justify-` [6]
    * test_value_storage.html `align-` [9]
    * ... `justify-` [14]
* Stylesheet cloning is somehow busted bug 1348481
  * test_selectors.html `cloned correctly` [157]
  * ... `matched clone` [204]
* Unsupported prefixed values
  * moz-prefixed gradient functions bug 1337655
    * test_value_storage.html `-moz-linear-gradient` [322]
    * ... `-moz-radial-gradient` [309]
    * ... `-moz-repeating-` [298]
  * webkit-prefixed gradient functions servo/servo#15441
    * test_value_storage.html `-webkit-gradient` [225]
    * ... `-webkit-linear-gradient` [40]
    * ... `-webkit-radial-gradient` [105]
    * ... `-webkit-repeating-` [35]
  * moz-prefixed intrinsic width values
    * test_flexbox_flex_shorthand.html `-moz-fit-content` [4]
    * test_value_storage.html `-moz-max-content` [46]
    * ... `-moz-min-content` [6]
    * ... `-moz-fit-content` [6]
    * ... `-moz-available` [4]
  * -moz-anchor-decoration value on text-decoration
    * test_value_storage.html `-moz-anchor-decoration` [10]
  * several prefixed values in cursor property
    * test_value_storage.html `cursor` [4]
  * moz-prefixed values of overflow shorthand bug 1330888
    * test_bug319381.html [8]
    * test_value_storage.html `'overflow` [8]
  * -moz-middle-with-baseline on vertical-align
    * test_value_storage.html `-moz-middle-with-baseline` [1]
  * -moz-pre-space on white-space
    * test_value_storage.html `-moz-pre-space` [1]
  * -webkit-{flex,inline-flex} for display servo/servo#15400
    * test_webkit_flex_display.html [4]
  * test_pixel_lengths.html `mozmm`: mozmm unit [3]
* Unsupported values
  * SVG-only values of pointer-events not recognized
    * test_compute_data_with_start_struct.html `pointer-events` [2]
    * test_inherit_computation.html `pointer-events` [4]
    * test_initial_computation.html `pointer-events` [2]
    * test_pointer-events.html [2]
    * test_value_storage.html `pointer-events` [8]
  * new syntax of rgba?() and hsla?() functions servo/rust-cssparser#113
    * test_value_storage.html `'color'` [35]
    * ... `rgb(100, 100.0, 100)` [1]
    * test_computed_style.html `css-color-4` [8]
    * test_specified_value_serialization.html `css-color-4` [8]
  * color interpolation hint not supported servo/servo#15166
    * test_value_storage.html `'linear-gradient` [50]
  * SVG-in-OpenType values not supported servo/servo#15211
    * test_value_storage.html `context-` [2]
  * writing-mode: sideways-{lr,rl} and SVG values servo/servo#15213
    * test_logical_properties.html `sideways` [1224]
    * test_value_storage.html `writing-mode` [8]
  * -moz-box-orient: {block,inline}-axis bug 1355005
    * test_value_storage.html `box-orient` [6]
* Incorrect parsing
  * Incorrect bounds
    * test_bug664955.html `font size is larger than max font size` [2]
  * calc() doesn't support dividing expression servo/servo#15192
    * test_value_storage.html `calc(50px/` [7]
    * ... `calc(2em / ` [9]
  * calc(number) is simplifed eagerly bug 1355014
    * test_value_storage.html `calc(-2.5)` [1]
  * size part of shorthand background/mask always desires two values servo/servo#15199
    * test_value_storage.html `'background'` [20]
    * ... `/ auto none` [38]
    * ... `/ auto repeat` [19]
  * border shorthands do not reset border-image servo/servo#15202
    * test_shorthand_property_getters.html `border-image` [1]
    * test_inherit_storage.html `for property 'border-image-` [5]
    * test_initial_storage.html `for property 'border-image-` [10]
    * test_value_storage.html `(for 'border-image-` [60]
  * -moz-alt-content parsing is wrong: servo/servo#15726
    * test_property_syntax_errors.html `-moz-alt-content` [4]
  * {transform,perspective}-origin fail to parse 'center left' and 'center right' servo/servo#15750
    * test_value_storage.html `'center left'` [8]
    * ... `'center right'` [8]
  * mask shorthand servo/servo#15772
    * test_property_syntax_errors.html `mask'` [76]
* Incorrect serialization
  * border-radius and -moz-outline-radius shorthand servo/servo#15169
    * test_priority_preservation.html `border-radius` [4]
    * test_value_storage.html `border-radius:` [64]
    * ... `-moz-outline-radius:` [31]
    * test_shorthand_property_getters.html `should condense to shortest possible` [6]
  * background-position is serialized to invalid value sometimes bug 1355017
    * test_shorthand_property_getters.html `background-position` [1]
  * color value not canonicalized servo/servo#15397
    * test_shorthand_property_getters.html `should condense to canonical case` [2]
  * background-position invalid 3-value form **issue to be filed**
    * test_shorthand_property_getters.html `should serialize to 4-value` [2]
  * test_variables.html `--weird`: name of custom property is not escaped properly servo/servo#15399 [1]
  * :not(*) doesn't serialize properly servo/servo#16017
    * test_selectors.html `:not()` [8]
    * ... `:not(html|)` [1]
  * "*|a" gets serialized as "a" when it should not servo/servo#16020
    * test_selectors.html `reserialization of *|a` [6]
* Unsupported pseudo-elements or anon boxes
  * :-moz-tree bits bug 1348488
    * test_selectors.html `:-moz-tree` [10]
  * :-moz-placeholder bug 1348490
    * test_selectors.html `:-moz-placeholder` [1]
  * ::-moz-color-swatch bug 1348492
    * test_selectors.html `::-moz-color-swatch` [1]
* Unsupported pseudo-classes
  * :-moz-locale-dir
    * test_selectors.html `:-moz-locale-dir` [15]
  * :-moz-lwtheme-*
    * test_selectors.html `:-moz-lwtheme` [3]
  * :-moz-window-inactive bug 1348489
    * test_selectors.html `:-moz-window-inactive` [2]
  * :dir
    * test_selectors.html `:dir` [18]
* issues arround font shorthand bug 1349417
  * test_bug377947.html [1]
  * test_value_storage.html `'font'` [160]
  * test_shorthand_property_getters.html `font shorthand` [1]
  * test_system_font_serialization.html [10]
* clamp negative value from calc() servo/servo#15296
  * test_value_storage.html `font-size: calc(` [3]
  * ... `font-size: var(--a)` [3]
* Negative value should be rejected
  * test_property_syntax_errors.html `transition-duration`: servo/servo#15343 [20]
  * ... `'text-shadow'`: third length of text-shadow servo/servo#15999 [2]
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
* test_pseudoelement_parsing.html: support parsing some pseudo-classes on some pseudo-elements [5]
* Unit should be preserved after parsing servo/servo#15346
  * test_units_time.html [1]
* insertRule / deleteRule don't work bug 1336863
  * test_rule_insertion.html [5]
* @-moz-document support
  * test_rule_serialization.html [2]
  * test_moz_document_rules.html [13]
* getComputedStyle style doesn't contain custom properties bug 1336891
  * test_variable_serialization_computed.html [35]
  * test_variables.html `custom property name` [2]
* test_css_supports.html: issues around @supports syntax servo/servo#15482 [8]
* test_author_specified_style.html: support serializing color as author specified bug 1348165 [27]
* browser_newtab_share_rule_processors.js: agent style sheet sharing [1]
* test_selectors.html `this_better_be_unvisited`: visited handling [2]

## Assertions

## Need Gecko change

* Servo is correct but Gecko is wrong
  * flex-basis should be 0px when omitted in flex shorthand bug 1331530
    * test_flexbox_flex_shorthand.html `flex-basis` [10]
  * should reject whole value bug 1355352
    * test_descriptor_storage.html `U+100-17F,U+200-17F` [1]
    * test_font_face_parser.html `U+90-30` [2]
    * ... `U+220043` [2]
  * Gecko clamps rather than rejects invalid unicode range bug 1355356
    * test_font_face_parser.html `U+??????` [2]
    * ... `12FFFF` [2]

## Spec Unclear

* test_property_syntax_errors.html `'background'`: whether background shorthand should accept "text" [200]

## Unknown / Unsure

* test_additional_sheets.html: one sub-test cascade order is wrong [1]
* test_selectors.html `:nth-child`: &lt;an+b&gt; parsing difference [14]
* test_selectors_on_anonymous_content.html: xbl and :nth-child [1]
* test_parse_rule.html `rgb(0, 128, 0)`: color properties not getting computed [6]

## Ignore

* Ignore for now since should be mostly identical to test_value_storage.html
  * test_value_cloning.html [*]
  * test_value_computation.html [*]
