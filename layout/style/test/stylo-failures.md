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
  * "layout.css.prefixes.device-pixel-ratio-webkit" support bug 1366956
    * test_media_queries.html `-device-pixel-ratio` [27]
    * test_webkit_device_pixel_ratio.html [3]
  * test_media_queries_dynamic.html `restyle`: bug 1357461 [4]
  * test_media_queries_dynamic_xbl.html: xbl support bug 1290276 [2]
* Animation support:
  * SMIL Animation
    * test_restyles_in_smil_animation.html [2]
* console support bug 1352669
  * test_bug413958.html `monitorConsole` [3]
  * test_parser_diagnostics_unprintables.html [550]
* Transition support:
  * test_transitions.html: pseudo elements [4]
  * test_transitions_and_reframes.html `pseudo-element`: bug 1366422 [4]
  * Events:
    * test_animations_event_order.html [2]
* Unimplemented \@font-face descriptors:
  * test_font_face_parser.html `font-language-override`: bug 1355364 [8]
* keyword values should be preserved in \@font-face bug 1355368
  * test_font_face_parser.html `font-weight` [4]
  * test_font_loading_api.html `weight` [1]
* @namespace support:
  * test_namespace_rule.html: bug 1355715 [6]
* test_font_feature_values_parsing.html: \@font-feature-values support bug 1355721 [107]
* Grid support bug 1341802
  * test_grid_computed_values.html [4]
  * test_grid_container_shorthands.html [65]
  * test_grid_item_shorthands.html [23]
  * test_grid_shorthand_serialization.html [28]
  * test_inherit_computation.html `grid` [2]
  * test_initial_computation.html `grid` [4]
  * test_property_syntax_errors.html `grid`: actually there are issues with this [8]
  * test_value_storage.html `'grid` [195]
* Unimplemented CSS properties:
  * font-variant shorthand bug 1356134
    * test_value_storage.html `'font-variant'` [65]
  * font-variant-alternates property bug 1355721
    * test_property_syntax_errors.html `font-variant-alternates` [2]
    * test_value_storage.html `font-variant-alternates` [22]
    * test_specified_value_serialization.html `bug-721136` [1]
* Unsupported prefixed values
  * moz-prefixed gradient functions bug 1337655
    * test_value_storage.html `-moz-linear-gradient` [322]
    * ... `-moz-radial-gradient` [309]
    * ... `-moz-repeating-` [298]
  * -webkit-{flex,inline-flex} for display servo/servo#15400
    * test_webkit_flex_display.html [4]
* Unsupported values
  * SVG-in-OpenType values not supported servo/servo#15211 bug 1355412
    * test_value_storage.html `context-` [7]
    * test_bug798843_pref.html [7]
* Incorrect parsing
  * different parsing bug 1364260
    * test_supports_rules.html [6]
    * test_condition_text.html [1]
* Incorrect serialization
  * color value not canonicalized servo/servo#15397
    * test_shorthand_property_getters.html `should condense to canonical case` [2]
  * place-{content,items,self} shorthands bug 1363971
    * test_align_shorthand_serialization.html [6]
  * system font serialization with subprop specified bug 1364286
    * test_system_font_serialization.html [5]
  * serialize subprops to -moz-use-system-font when using system font bug 1364289
    * test_value_storage.html `'font'` [224]
  * different serialization for gradient functions in computed value bug 1367274
    * test_computed_style.html `gradient` [13]
* Unsupported pseudo-elements or anon boxes
  * :-moz-tree bits bug 1348488
    * test_selectors.html `:-moz-tree` [10]
* Unsupported pseudo-classes
  * :-moz-locale-dir is internal bug 1367310
    * test_selectors.html `:-moz-locale-dir` [15]
  * :-moz-lwtheme-* bug 1367312
    * test_selectors.html `:-moz-lwtheme` [3]
  * :-moz-window-inactive bug 1348489
    * test_selectors.html `:-moz-window-inactive` [2]
  * :dir case-sensitivity and syntax bug 1367315
    * test_selectors.html `:dir` [11]
* Quirks mode support
  * test_hover_quirk.html: hover quirks bug 1355724 [6]
* Unit should be preserved after parsing servo/servo#15346
  * test_units_time.html [1]
* getComputedStyle style doesn't contain custom properties bug 1336891
  * test_variable_serialization_computed.html [35]
  * test_variables.html `custom property name` [2]
* test_css_supports.html: issues around @supports syntax servo/servo#15482 [8]
* test_author_specified_style.html: support serializing color as author specified bug 1348165 [27]
* browser_newtab_share_rule_processors.js: agent style sheet sharing [1]
* :visited support (bug 1328509)
  * test_visited_reftests.html `selector-descendant-2.xhtml` [2]
  * ... `selector-child-2.xhtml` [2]
  * ... `color-on-bullets-1.html` [2]
  * ... `inherit-keyword-1.xhtml` [2]
  * ... `mathml-links.html` [2]
  * ... `caret-color-on-visited-1.html` [2]

## Assertions

## Need Gecko change

* Servo is correct but Gecko is wrong
  * Gecko rejects calc() in -webkit-gradient bug 1363349
    * test_property_syntax_errors.html `-webkit-gradient` [20]
* test_specified_value_serialization.html `-webkit-radial-gradient`: bug 1367299 [1]
* test_variables.html `var(--var6)`: irrelevant test for stylo bug 1367306 [1]

## Unknown / Unsure

* test_selectors_on_anonymous_content.html: xbl and :nth-child [1]
* test_parse_rule.html `rgb(0, 128, 0)`: color properties not getting computed [5]
* test_selectors.html `:nth-child`: &lt;an+b&gt; parsing difference bug 1364009 [14]

## Ignore

* Ignore for now since should be mostly identical to test_value_storage.html
  * test_value_cloning.html [*]
  * test_value_computation.html [*]
