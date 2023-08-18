# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The button for "Firefox Translations" in the url bar. Note that here "Beta" should
# not be translated, as it is a reflection of the un-localized BETA icon that is in the
# panel.
urlbar-translations-button2 =
  .tooltiptext = Translate this page - Beta

# Note that here "Beta" should not be translated, as it is a reflection of the
# un-localized BETA icon that is in the panel.
urlbar-translations-button-intro =
  .tooltiptext = Try private translations in { -brand-shorter-name } - Beta

# If your language requires declining the language name, a possible solution
# is to adapt the structure of the phrase, or use a support noun, e.g.
# `Page translated from: { $fromLanguage }. Current target language: { $toLanguage }`
#
# Variables:
#   $fromLanguage (string) - The original language of the document.
#   $toLanguage (string) - The target language of the translation.
urlbar-translations-button-translated =
  .tooltiptext = Page translated from { $fromLanguage } to { $toLanguage }

urlbar-translations-button-loading =
  .tooltiptext = Translation in progress

translations-panel-settings-button =
  .aria-label = Manage translation settings

## Options in the Firefox Translations settings.

translations-panel-settings-manage-languages =
  .label = Manage languages
translations-panel-settings-about2 =
  .label = About translations in { -brand-shorter-name }

# Text displayed for the option to always translate a given language
# Variables:
#   $language (string) - The localized display name of the detected language
translations-panel-settings-always-translate-language =
  .label = Always translate { $language }
translations-panel-settings-always-translate-unknown-language =
  .label = Always translate this language
translations-panel-settings-always-offer-translation =
  .label = Always offer to translate

# Text displayed for the option to never translate a given language
# Variables:
#   $language (string) - The localized display name of the detected language
translations-panel-settings-never-translate-language =
  .label = Never translate { $language }
translations-panel-settings-never-translate-unknown-language =
  .label = Never translate this language

# Text displayed for the option to never translate this website
translations-panel-settings-never-translate-site =
  .label = Never translate this site

## The translation panel appears from the url bar, and this view is the default
## translation view.

translations-panel-header = Translate this page?
translations-panel-translate-button =
  .label = Translate
translations-panel-translate-button-loading =
  .label = Please wait…
translations-panel-translate-cancel =
  .label = Cancel
translations-panel-learn-more-link = Learn more

translations-panel-intro-header = Try private translations in { -brand-shorter-name }
translations-panel-intro-description = For your privacy, translations never leave your device. New languages and improvements coming soon!

translations-panel-error-translating = There was a problem translating. Please try again.
translations-panel-error-load-languages = Couldn’t load languages
translations-panel-error-load-languages-hint = Check your internet connection and try again.
translations-panel-error-load-languages-hint-button =
  .label = Try again

translations-panel-error-unsupported = Translation isn’t available for this page
translations-panel-error-dismiss-button =
  .label = Got it
translations-panel-error-change-button =
  .label = Change source language
# If your language requires declining the language name, a possible solution
# is to adapt the structure of the phrase, or use a support noun, e.g.
# `Sorry, we don't support the language yet: { $language }
#
# Variables:
#   $language (string) - The language of the document.
translations-panel-error-unsupported-hint-known = Sorry, we don’t support { $language } yet.
translations-panel-error-unsupported-hint-unknown = Sorry, we don’t support this language yet.

## Each label is followed, on a new line, by a dropdown list of language names.
## If this structure is problematic for your locale, an alternative way is to
## translate them as `Source language:` and `Target language:`

translations-panel-from-label = Translate from
translations-panel-to-label = Translate to

## The translation panel appears from the url bar, and this view is the "restore" view
## that lets a user restore a page to the original language, or translate into another
## language.

# If your language requires declining the language name, a possible solution
# is to adapt the structure of the phrase, or use a support noun, e.g.
# `The page is translated from: { $fromLanguage }. Current target language: { $toLanguage }`
#
# Variables:
#   $fromLanguage (string) - The original language of the document.
#   $toLanguage (string) - The target language of the translation.
translations-panel-revisit-header = This page is translated from { $fromLanguage } to { $toLanguage }
translations-panel-choose-language =
  .label = Choose a language
translations-panel-restore-button =
  .label = Show original

## Firefox Translations language management in about:preferences.

translations-manage-header = Translations
translations-manage-settings-button =
    .label = Settings…
    .accesskey = t
translations-manage-intro = Set your language and site translation preferences and manage languages installed for offline translation.
translations-manage-install-description = Install languages for offline translation
translations-manage-language-install-button =
    .label = Install
translations-manage-language-install-all-button =
    .label = Install all
    .accesskey = I
translations-manage-language-remove-button =
    .label = Remove
translations-manage-language-remove-all-button =
    .label = Remove all
    .accesskey = e
translations-manage-error-install = There was a problem installing the language files. Please try again.
translations-manage-error-remove = There was an error removing the language files. Please try again.
translations-manage-error-list = Failed to get the list of available languages for translation. Refresh the page to try again.

translations-settings-title =
    .title = Translations Settings
    .style = min-width: 36em
translations-settings-close-key =
    .key = w
translations-settings-always-translate-langs-description = Translation will happen automatically for the following languages
translations-settings-never-translate-langs-description = Translation will not be offered for the following languages
translations-settings-never-translate-sites-description = Translation will not be offered for the following sites
translations-settings-languages-column =
    .label = Languages
translations-settings-remove-language-button =
    .label = Remove Language
    .accesskey = R
translations-settings-remove-all-languages-button =
    .label = Remove All Languages
    .accesskey = e
translations-settings-sites-column =
    .label = Websites
translations-settings-remove-site-button =
    .label = Remove Site
    .accesskey = S
translations-settings-remove-all-sites-button =
    .label = Remove All Sites
    .accesskey = m
translations-settings-close-dialog =
    .buttonlabelaccept = Close
    .buttonaccesskeyaccept = C
