# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## These 2 strings are used to construct a sentence that contains a dropdown
## showing the detected language of the current web page.
## In en-US it looks like this:
##   This page is in [detected language] Translate this page?
## "detected language" here is a language name coming from the
## toolkit/intl/languageNames.ftl file; for some locales it may not be in
## the correct grammar case to keep the same structure of the original
## sentence.

translation-notification-this-page-is-in =
    .value = This page is in
translation-notification-translate-this-page =
    .value = Translate this page?

##

translation-notification-translate-button =
    .label = Translate
translation-notification-not-now-button =
    .label = Not Now
translation-notification-translating-content =
    .value = Translating page content…

## These 3 strings are used to construct a sentence that contains 2 dropdowns
## showing the source and target language of a translated web page.
## In en-US it looks like this:
##   This page has been translated from [from language] to [to language]
## "from language" and "to language" here are language names coming from the
## toolkit/intl/languageNames.ftl file; for some locales they may not be in
## the correct grammar case to keep the same structure of the original
## sentence.

translation-notification-translated-from =
    .value = This page has been translated from
translation-notification-translated-to =
    .value = to
# This string (empty in en-US) is for locales that need to display some text
# after the second drop down for the sentence to be grammatically correct.
translation-notification-translated-to-suffix =
    .value = {""}

##

translation-notification-show-original-button =
    .label = Show Original
translation-notification-show-translation-button =
    .label = Show Translation
translation-notification-error-translating =
    .value = There has been an error translating this page.
translation-notification-try-again-button =
    .label = Try Again
translation-notification-service-unavailable =
    .value = Translation is not available at the moment. Please try again later.
translation-notification-options-menu =
    .label = Options

## The accesskey values used here should not clash with each other

# Variables:
#  $langName (String): a language name coming from the toolkit/intl/languageNames.ftl file.
translation-notification-options-never-for-language =
    .label = Never translate { $langName }
    .accesskey = N
translation-notification-options-never-for-site =
    .label = Never translate this site
    .accesskey = e
translation-notification-options-preferences =
    .label = Translation preferences
    .accesskey = T
