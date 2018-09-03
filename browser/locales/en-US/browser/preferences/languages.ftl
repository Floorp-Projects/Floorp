# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

webpage-languages-window =
    .title = Webpage Language Settings
    .style = width: 40em

languages-close-key =
    .key = w

languages-description = Web pages are sometimes offered in more than one language. Choose languages for displaying these web pages, in order of preference

languages-customize-spoof-english =
    .label = Request English versions of web pages for enhanced privacy

languages-customize-moveup =
    .label = Move Up
    .accesskey = U

languages-customize-movedown =
    .label = Move Down
    .accesskey = D

languages-customize-remove =
    .label = Remove
    .accesskey = R

languages-customize-select-language =
    .placeholder = Select a language to add…

languages-customize-add =
    .label = Add
    .accesskey = A

# The pattern used to generate strings presented to the user in the
# locale selection list.
#
# Example:
#   Icelandic [is]
#   Spanish (Chile) [es-CL]
#
# Variables:
#   $locale (String) - A name of the locale (for example: "Icelandic", "Spanish (Chile)")
#   $code (String) - Locale code of the locale (for example: "is", "es-CL")
languages-code-format =
    .label = { $locale } [{ $code }]

languages-active-code-format =
    .value = { languages-code-format.label }

browser-languages-window =
    .title = { -brand-short-name } Language Settings
    .style = width: 40em

browser-languages-description = { -brand-short-name } will display the first language as your default and will display alternate languages if necessary in the order they appear.
