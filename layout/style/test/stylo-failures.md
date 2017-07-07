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
  * test_media_queries_dynamic_xbl.html: xbl support bug 1290276 [1]
* Animation support:
  * SMIL Animation
    * test_restyles_in_smil_animation.html [2]
* console support bug 1352669
  * test_bug413958.html `monitorConsole` [3]
  * test_parser_diagnostics_unprintables.html [550]
* @namespace support:
  * test_namespace_rule.html: bug 1355715 [6]
* test_font_feature_values_parsing.html: \@font-feature-values support bug 1355721 [107]
* Grid support bug 1341802
  * test_grid_computed_values.html [4]
  * test_grid_container_shorthands.html [65]
  * test_grid_item_shorthands.html [23]
  * test_grid_shorthand_serialization.html [28]
  * test_value_storage.html `'grid` [21]
* Unsupported values
  * SVG-in-OpenType values not supported servo/servo#15211 bug 1355412
    * test_value_storage.html `context-` [7]
    * test_bug798843_pref.html [3]
* Incorrect parsing
  * different parsing bug 1364260
    * test_supports_rules.html [6]
    * test_condition_text.html [1]
* Incorrect serialization
  * place-{content,items,self} shorthands bug 1363971
    * test_align_shorthand_serialization.html [6]
  * system font serialization with subprop specified bug 1364286
    * test_system_font_serialization.html [3]
  * serialize subprops to -moz-use-system-font when using system font bug 1364289
    * test_value_storage.html `'font'` [240]
  * different serialization for gradient functions in computed value bug 1367274
    * test_computed_style.html `gradient` [13]
* Unsupported pseudo-elements or anon boxes
  * :-moz-tree bits bug 1348488
    * test_selectors.html `:-moz-tree` [10]
* Unit should be preserved after parsing servo/servo#15346
  * test_units_time.html [1]
* getComputedStyle style doesn't contain custom properties bug 1336891
  * test_variables.html `custom property name` [2]
* test_css_supports.html: issues around @supports syntax servo/servo#15482 [2]
* test_author_specified_style.html: support serializing color as author specified bug 1348165 [27]
* browser_newtab_share_rule_processors.js: agent style sheet sharing [1]
* :visited support (bug 1328509)
  * test_visited_reftests.html `inherit-keyword-1.xhtml` [2]
  * ... `mathml-links.html` [2]

## Assertions

## Need Gecko change

* Servo is correct but Gecko is wrong
  * Gecko rejects calc() in -webkit-gradient bug 1363349
    * test_property_syntax_errors.html `-webkit-gradient` [20]
* test_specified_value_serialization.html `-webkit-radial-gradient`: bug 1367299 [1]

## Unknown / Unsure

* test_selectors_on_anonymous_content.html: xbl and :nth-child [1]

## Ignore

* Ignore for now since should be mostly identical to test_value_storage.html
  * test_value_cloning.html [*]
  * test_value_computation.html [*]
