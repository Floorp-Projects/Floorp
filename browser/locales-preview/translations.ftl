# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The button for "Firefox Translations" in the url bar.
urlbar-translations-button =
  .tooltiptext = Translate this page

translations-panel-settings-button =
  .aria-label = Manage translation settings

# Text displayed on a language dropdown when the language is in beta
# Variables:
#   $language (string) - The localized display name of the detected language
translations-panel-displayname-beta =
  .label = { $language } BETA

## Options in the Firefox Translations settings.

translations-panel-settings-manage-languages =
  .label = Manage languages
translations-panel-settings-change-source-language =
  .label = Change source language
# TODO(Bug 1831341): We still need the link for this menu item.
translations-panel-settings-about = About translations in { -brand-shorter-name }

## The translation panel appears from the url bar, and this view is the default
## translation view.

translations-panel-default-header = Translate this page?

# If your language requires declining the language name, a possible solution
# is to adapt the structure of the phrase, or use a support noun, e.g.
# `Looks like this page is in another language ({ $pageLanguage }). Want to translate it?`
# Variables:
#   $pageLanguage (string) - The localized display name of the page's language
translations-panel-default-description = Looks like this page is in { $pageLanguage }. Want to translate it?

# This label is followed, on a new line, by a dropdown list of language names.
# If this structure is problematic for your locale, an alternative way is to
# translate this as `Target language:`
translations-panel-default-translate-to-label = Translate to
translations-panel-default-translate-button = Translate
translations-panel-default-translate-cancel = Not now

translations-panel-error-translating = There was a problem translating. Please try again.
translations-panel-error-load-languages = Couldn’t load languages
translations-panel-error-load-languages-hint = Check your internet connection and try again.

## The translation panel appears from the url bar, and this view is the "dual" translate
## view that lets you choose a source language and target language for translation

translations-panel-dual-header =
  .title = Translate this page?
translations-panel-dual-cancel-button = Cancel

## Each label is followed, on a new line, by a dropdown list of language names.
## If this structure is problematic for your locale, an alternative way is to
## translate them as `Source language:` and `Target language:`

translations-panel-dual-from-label = Translate from
translations-panel-dual-to-label = Translate to

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
translations-panel-revisit-header = The page is translated from { $fromLanguage } to { $toLanguage }
translations-panel-revisit-label = Want to try another language?
translations-panel-choose-language =
  .label = Choose a language
translations-panel-revisit-restore-button = Show original
translations-panel-revisit-translate-button = Translate

## Firefox Translations language management in about:preferences.

translations-manage-header = Translations
translations-manage-settings-button =
    .label = Settings…
    .accesskey = t
translations-manage-description = Download languages for offline translation.
translations-manage-all-language = All languages
translations-manage-download-button = Download
translations-manage-delete-button = Delete
translations-manage-error-download = There was a problem downloading the language files. Please try again.
translations-manage-error-delete = There was an error deleting the language files. Please try again.
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
