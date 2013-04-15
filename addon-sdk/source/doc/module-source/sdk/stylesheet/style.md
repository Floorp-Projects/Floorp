<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Module provides `Style` function that can be used to construct content style
modification via stylesheet files or CSS rules.

<api name="Style">
@class
<api name="Style">
@constructor
  The Style constructor creates an object that represents style modifications
  via stylesheet file(s) or/and CSS rules. Stylesheet file URL(s) are verified
  to be local to an add-on, while CSS rules are virified to be a string or
  array of strings.

  The style created can be applied to a content by calling `attach`,
  and removed using `detach`. Those functions are part of [content/mod](modules/sdk/content/mod.html) module.
@param options {object}
  Options for the style. All these options are optional. Although if you
  don't supply any stylesheet or CSS rules, your style won't be very useful.

  @prop uri {string,array}
    A string, or an array of strings, that represents local URI to stylesheet.
  @prop source {string,array}
    A string, or an array of strings, that contains CSS rules. Those rules
    are applied after the rules in the stylesheet specified with `uri` options,
    if provided.
  @prop [type="author"] {string}
    The type of the sheet. It accepts the following values: `"agent"`, `"user"`
    and `"author"`.
    If not provided, the default value is `"author"`.
</api>

<api name="source">
@property {string}
  An array of strings that contains the CSS rule(s) specified in the constructor's
  option; `null` if no `source` option was given to the constructor.
  This property is read-only.
</api>
<api name="uri">
@property {string}
  An array of strings that contains the stylesheet local URI(s) specified in the
  constructor's option; `null` if no `uri` option was given to the
  constructor.
  This property is read-only.
</api>
<api name="type">
  @property {string}
    The type of the sheet. If no type is provided in constructor's option,
    it returns the default value, `"author"`. This property is read-only.
</api>
</api>
