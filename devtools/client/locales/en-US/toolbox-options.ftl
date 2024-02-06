# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### Localization for Developer Tools options

## Default Developer Tools section

# The heading
options-select-default-tools-label = Default Developer Tools

# The label for the explanation of the * marker on a tool which is currently not supported
# for the target of the toolbox.
options-tool-not-supported-label = * Not supported for current toolbox target

# The label for the heading of group of checkboxes corresponding to the developer tools
# added by add-ons. This heading is hidden when there is no developer tool installed by add-ons.
options-select-additional-tools-label = Developer Tools installed by add-ons

# The label for the heading of group of checkboxes corresponding to the default developer
# tool buttons.
options-select-enabled-toolbox-buttons-label = Available Toolbox Buttons

# The label for the heading of the radiobox corresponding to the theme
options-select-dev-tools-theme-label = Themes

## Inspector section

# The heading
options-context-inspector = Inspector

# The label for the checkbox option to show user agent styles
options-show-user-agent-styles-label = Show Browser Styles
options-show-user-agent-styles-tooltip =
    .title = Turning this on will show default styles that are loaded by the browser.

# The label for the checkbox option to enable collapse attributes
options-collapse-attrs-label = Truncate DOM attributes
options-collapse-attrs-tooltip =
    .title = Truncate long attributes in the inspector

# The label for the checkbox option to enable the "drag to update" feature
options-inspector-draggable-properties-label = Click and drag to edit size values
options-inspector-draggable-properties-tooltip =
    .title = Click and drag to edit size values in the inspector rules view.

# The label for the checkbox option to enable simplified highlighting on page elements
# within the inspector for users who enabled prefers-reduced-motion = reduce
options-inspector-simplified-highlighters-label = Use simpler highlighters with prefers-reduced-motion
options-inspector-simplified-highlighters-tooltip =
    .title = Enables simplified highlighters when prefers-reduced-motion is enabled. Draws lines instead of filled rectangles around highlighted elements to avoid flashing effects.

# The label for the checkbox option to make the Enter key move the focus to the next input
# when editing a property name or value in the Inspector rules view
options-inspector-rules-focus-next-on-enter-label = Focus next input on <kbd>Enter</kbd>
options-inspector-rules-focus-next-on-enter-tooltip =
    .title = When enabled, hitting the Enter key when editing a selector, a property name or value will move the focus to the next input.

## "Default Color Unit" options for the Inspector

options-default-color-unit-label = Default color unit
options-default-color-unit-authored = As Authored
options-default-color-unit-hex = Hex
options-default-color-unit-hsl = HSL(A)
options-default-color-unit-rgb = RGB(A)
options-default-color-unit-hwb = HWB
options-default-color-unit-name = Color Names

## Style Editor section

# The heading
options-styleeditor-label = Style Editor

# The label for the checkbox that toggles autocompletion of css in the Style Editor
options-stylesheet-autocompletion-label = Autocomplete CSS
options-stylesheet-autocompletion-tooltip =
    .title = Autocomplete CSS properties, values and selectors in Style Editor as you type

## Screenshot section

# The heading
options-screenshot-label = Screenshot Behavior

# Label for the checkbox that toggles screenshot to clipboard feature
options-screenshot-clipboard-only-label = Screenshot to clipboard only
options-screenshot-clipboard-tooltip2 =
    .title = Saves the screenshot directly to the clipboard

# Label for the checkbox that toggles the camera shutter audio for screenshot tool
options-screenshot-audio-label = Play camera shutter sound
options-screenshot-audio-tooltip =
    .title = Enables the camera audio sound when taking screenshot

## Editor section

# The heading
options-sourceeditor-label = Editor Preferences

options-sourceeditor-detectindentation-tooltip =
    .title = Guess indentation based on source content
options-sourceeditor-detectindentation-label = Detect indentation
options-sourceeditor-autoclosebrackets-tooltip =
    .title = Automatically insert closing brackets
options-sourceeditor-autoclosebrackets-label = Autoclose brackets
options-sourceeditor-expandtab-tooltip =
    .title = Use spaces instead of the tab character
options-sourceeditor-expandtab-label = Indent using spaces
options-sourceeditor-tabsize-label = Tab size
options-sourceeditor-keybinding-label = Keybindings
options-sourceeditor-keybinding-default-label = Default

## Advanced section

# The heading (this item is also used in perftools.ftl)
options-context-advanced-settings = Advanced settings

# The label for the checkbox that toggles the HTTP cache on or off
options-disable-http-cache-label = Disable HTTP Cache (when toolbox is open)
options-disable-http-cache-tooltip =
    .title = Turning this option on will disable the HTTP cache for all tabs that have the toolbox open. Service Workers are not affected by this option.

# The label for checkbox that toggles JavaScript on or off
options-disable-javascript-label = Disable JavaScript *
options-disable-javascript-tooltip =
    .title = Turning this option on will disable JavaScript for the current tab. If the tab or the toolbox is closed then this setting will be forgotten.

# The label for checkbox that toggles chrome debugging, i.e. the devtools.chrome.enabled preference
options-enable-chrome-label = Enable browser chrome and add-on debugging toolboxes
options-enable-chrome-tooltip =
    .title = Turning this option on will allow you to use various developer tools in browser context (via Tools > Web Developer > Browser Toolbox) and debug add-ons from the Add-ons Manager

# The label for checkbox that toggles remote debugging, i.e. the devtools.debugger.remote-enabled preference
options-enable-remote-label = Enable remote debugging
options-enable-remote-tooltip2 =
    .title = Turning this option on will allow to debug this browser instance remotely

# The label for checkbox that enables F12 as a shortcut to open DevTools
options-enable-f12-label = Use the F12 key to open or close DevTools
options-enable-f12-tooltip =
    .title = Turning this option on will bind the F12 key to open or close the DevTools toolbox

# The label for checkbox that toggles custom formatters for objects
options-enable-custom-formatters-label = Enable custom formatters
options-enable-custom-formatters-tooltip =
    .title = Turning this option on will allow sites to define custom formatters for DOM objects

# The label for checkbox that toggles the service workers testing over HTTP on or off.
options-enable-service-workers-http-label = Enable Service Workers over HTTP (when toolbox is open)
options-enable-service-workers-http-tooltip =
    .title = Turning this option on will enable the service workers over HTTP for all tabs that have the toolbox open.

# The label for the checkbox that toggles source maps in all tools.
options-source-maps-label = Enable Source Maps
options-source-maps-tooltip =
    .title = If you enable this option sources will be mapped in the tools.

# The message shown for settings that trigger page reload
options-context-triggers-page-refresh = * Current session only, reloads the page
