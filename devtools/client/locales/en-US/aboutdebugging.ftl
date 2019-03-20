# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings are used inside the about:debugging UI.

# Page Title strings

# Page title (ie tab title) for the Setup page
about-debugging-page-title-setup-page = Debugging - Setup

# Page title (ie tab title) for the Runtime page
# { $selectedRuntimeId } is the id of the current runtime, such as "this-firefox", "localhost:6080", ...
about-debugging-page-title-runtime-page = Debugging - Runtime / { $selectedRuntimeId }

# Sidebar strings

# Display name of the runtime "This Firefox". Reused as the sidebar name for This Firefox
# (about-debugging-sidebar-this-firefox.name). Not displayed elsewhere in the application
# at the moment.
# This should the same string as the part outside of the parentheses in toolbox.properties
# toolbox.debugTargetInfo.runtimeLabel.thisFirefox. See Bug 1520525.
about-debugging-this-firefox-runtime-name = This Firefox

# Sidebar heading for selecting the currently running instance of Firefox
about-debugging-sidebar-this-firefox =
  .name = { about-debugging-this-firefox-runtime-name }

# Sidebar heading for connecting to some remote source
about-debugging-sidebar-setup =
  .name = Setup

# Text displayed in the about:debugging sidebar when USB devices discovery is enabled.
about-debugging-sidebar-usb-enabled = USB enabled

# Text displayed in the about:debugging sidebar when USB devices discovery is disabled
# (for instance because the mandatory ADB extension is not installed).
about-debugging-sidebar-usb-disabled = USB disabled

# Connection status (connected) for runtime items in the sidebar
aboutdebugging-sidebar-runtime-connection-status-connected = Connected
# Connection status (disconnected) for runtime items in the sidebar
aboutdebugging-sidebar-runtime-connection-status-disconnected = Disconnected

# Text displayed in the about:debugging sidebar when no device was found.
about-debugging-sidebar-no-devices = No devices discovered

# Text displayed in buttons found in sidebar items representing remote runtimes.
# Clicking on the button will attempt to connect to the runtime.
about-debugging-sidebar-item-connect-button = Connect

# Temporary text displayed in sidebar items representing remote runtimes after
# successfully connecting to them. Temporary UI, do not localize.
about-debugging-sidebar-item-connected-label = Connected

# Text displayed in sidebar items for remote devices where a compatible runtime (eg
# Firefox) has not been detected yet. Typically, Android phones connected via USB with
# USB debugging enabled, but where Firefox is not started.
about-debugging-sidebar-runtime-item-waiting-for-runtime = Waiting for runtime…

# Title for runtime sidebar items that are related to a specific device (USB, WiFi).
about-debugging-sidebar-runtime-item-name =
  .title = { $displayName } ({ $deviceName })
# Title for runtime sidebar items where we cannot get device information (network
# locations).
about-debugging-sidebar-runtime-item-name-no-device =
  .title = { $displayName }

# Text displayed in a sidebar button to refresh the list of USB devices. Clicking on it
# will attempt to update the list of devices displayed in the sidebar.
about-debugging-refresh-usb-devices-button = Refresh devices

# Setup Page strings

# Title of the Setup page.
about-debugging-setup-title = Setup

# Introduction text in the Setup page to explain how to configure remote debugging.
about-debugging-setup-intro = Configure the connection method you wish to remotely debug your device with.

# Link displayed in the Setup page that leads to MDN page with list of supported devices.
# Temporarily leads to https://support.mozilla.org/en-US/kb/will-firefox-work-my-mobile-device#w_android-devices
about-debugging-setup-link-android-devices = View list of supported Android devices

# Explanatory text in the Setup page about what the 'This Firefox' page is for
about-debugging-setup-this-firefox = Use <a>This Firefox</a> to debug tabs, extensions and service workers on this version of Firefox.

# Title of the heading Connect section of the Setup page.
about-debugging-setup-connect-heading = Connect a Device

# USB section of the Setup page
about-debugging-setup-usb-title = USB

# Explanatory text displayed in the Setup page when USB debugging is disabled
about-debugging-setup-usb-disabled = Enabling this will download and add the required Android USB debugging components to Firefox.

# Text of the button displayed in the USB section of the setup page when USB debugging is disabled.
# Clicking on it will download components needed to debug USB Devices remotely.
about-debugging-setup-usb-enable-button = Enable USB Devices

# Text of the button displayed in the USB section of the setup page when USB debugging is enabled.
about-debugging-setup-usb-disable-button = Disable USB Devices

# Text of the button displayed in the USB section of the setup page while USB debugging
# components are downloaded and installed.
about-debugging-setup-usb-updating-button = Updating…

# USB section of the Setup page (USB status)
about-debugging-setup-usb-status-enabled = Enabled
about-debugging-setup-usb-status-disabled = Disabled
about-debugging-setup-usb-status-updating = Updating…

# USB section step by step guide
about-debugging-setup-usb-step-enable-dev-menu = Enable Developer menu on your Android device. <a>Learn how</a>

# USB section step by step guide
about-debugging-setup-usb-step-enable-debug = Enable USB Debugging in the Android Developer Menu. <a>Learn how</a>

# USB section step by step guide
about-debugging-setup-usb-step-enable-debug-firefox = Enable USB Debugging in Firefox on the Android device. <a>Learn how</a>

# USB section step by step guide
about-debugging-setup-usb-step-plug-device = Connect the Android device to your computer.

# Network section of the Setup page
about-debugging-setup-network =
  .title = Network Location

# Text of a button displayed after the network locations "Host" input.
# Clicking on it will add the new network location to the list.
about-debugging-network-locations-add-button = Add

# Text to display when there are no locations to show.
about-debugging-network-locations-empty-text = No network locations have been added yet.

# Text of the label for the text input that allows users to add new network locations in
# the Connect page. A host is a hostname and a port separated by a colon, as suggested by
# the input's placeholder "localhost:6080".
about-debugging-network-locations-host-input-label = Host

# Text of a button displayed next to existing network locations in the Connect page.
# Clicking on it removes the network location from the list.
about-debugging-network-locations-remove-button = Remove
