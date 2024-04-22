# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.


# Text displayed in the right-click context menu for translating
# selected text to a yet-to-be-determined language.
main-context-menu-translate-selection =
    .label = Translate Selection…

# Text displayed in the right-click context menu for translating
# selected text to a target language.
#
# Variables:
#   $language (string) - The localized display name of the target language
main-context-menu-translate-selection-to-language =
    .label = Translate Selection to { $language }

# Text displayed in the right-click context menu for translating
# the text of a hyperlink to a yet-to-be-determined language.
main-context-menu-translate-link-text =
    .label = Translate Link Text…

# Text displayed in the right-click context menu for translating
# the text of a hyperlink to a target language.
#
# Variables:
#   $language (string) - The localized display name of the target language
main-context-menu-translate-link-text-to-language =
    .label = Translate Link Text to { $language }

# Text displayed in the select translations panel header.
select-translations-panel-header = Translation

# Text displayed above the from-language dropdown menu.
select-translations-panel-from-label = From

# Text displayed above the to-language dropdown menu.
select-translations-panel-to-label = To

# Text displayed above the try-another-source-language dropdown menu.
select-translations-panel-try-another-language-label = Try another source language

# Text displayed on the cancel button.
select-translations-panel-cancel-button =
    .label = Cancel

# Text displayed on the copy button before it is clicked.
select-translations-panel-copy-button =
    .label = Copy

# Text displayed on the copy button after it is clicked.
select-translations-panel-copy-button-copied =
    .label = Copied

# Text displayed on the done button.
select-translations-panel-done-button =
    .label = Done

# Text displayed on the translate-full-page button.
select-translations-panel-translate-full-page-button =
    .label = Translate full page

# Text displayed on the translate button.
select-translations-panel-translate-button =
    .label = Translate

# Text displayed on the try-again button.
select-translations-panel-try-again-button =
    .label = Try again

# Text displayed as a placeholder when the panel is idle.
select-translations-panel-idle-placeholder-text = Translated text will appear here.

# Text displayed as a placeholder when the panel is actively translating.
select-translations-panel-translating-placeholder-text = Translating…

select-translations-panel-init-failure-message =
    .message = Couldn’t load languages. Check your internet connection and try again.

# Text displayed when the translation fails to complete.
select-translations-panel-translation-failure-message =
    .message = There was a problem translating. Please try again.

# If your language requires declining the language name, a possible solution
# is to adapt the structure of the phrase, or use a support noun, e.g.
# `Sorry, we don't support the language yet: { $language }
#
# Variables:
#   $language (string) - The language of the document.
select-translations-panel-unsupported-language-message-known =
    .message = Sorry, we don’t support { $language } yet.
select-translations-panel-unsupported-language-message-unknown =
    .message = Sorry, we don’t support this language yet.

# Text displayed on the menuitem that opens the Translation Settings page.
select-translations-panel-open-translations-settings-menuitem =
    .label = Translation settings
