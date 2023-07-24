# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

permissions-window2 =
    .title = Exceptions
    .style = min-width: 45em

permissions-close-key =
    .key = w

permissions-address = Address of website
    .accesskey = d

permissions-block =
    .label = Block
    .accesskey = B

permissions-disable-etp =
    .label = Add Exception
    .accesskey = E

permissions-session =
    .label = Allow for Session
    .accesskey = S

permissions-allow =
    .label = Allow
    .accesskey = A

permissions-button-off =
    .label = Turn Off
    .accesskey = O

permissions-button-off-temporarily =
    .label = Turn Off Temporarily
    .accesskey = T

permissions-site-name =
    .label = Website

permissions-status =
    .label = Status

permissions-remove =
    .label = Remove Website
    .accesskey = R

permissions-remove-all =
    .label = Remove All Websites
    .accesskey = e

permission-dialog =
    .buttonlabelaccept = Save Changes
    .buttonaccesskeyaccept = S

permissions-autoplay-menu = Default for all websites:

permissions-searchbox =
    .placeholder = Search Website

permissions-capabilities-autoplay-allow =
    .label = Allow Audio and Video
permissions-capabilities-autoplay-block =
    .label = Block Audio
permissions-capabilities-autoplay-blockall =
    .label = Block Audio and Video

permissions-capabilities-allow =
    .label = Allow
permissions-capabilities-block =
    .label = Block
permissions-capabilities-prompt =
    .label = Always Ask

permissions-capabilities-listitem-allow =
    .value = Allow
permissions-capabilities-listitem-block =
    .value = Block
permissions-capabilities-listitem-allow-session =
    .value = Allow for Session

permissions-capabilities-listitem-off =
    .value = Off
permissions-capabilities-listitem-off-temporarily =
    .value = Off temporarily

## Invalid Hostname Dialog

permissions-invalid-uri-title = Invalid Hostname Entered
permissions-invalid-uri-label = Please enter a valid hostname

## Exceptions - Tracking Protection

permissions-exceptions-etp-window2 =
    .title = Exceptions for Enhanced Tracking Protection
    .style = { permissions-window2.style }
permissions-exceptions-manage-etp-desc = You can specify which websites have Enhanced Tracking Protection turned off. Type the exact address of the site you want to manage and then click Add Exception.

## Exceptions - Cookies

permissions-exceptions-cookie-window2 =
    .title = Exceptions - Cookies and Site Data
    .style = { permissions-window2.style }
permissions-exceptions-cookie-desc = You can specify which websites are always or never allowed to use cookies and site data.  Type the exact address of the site you want to manage and then click Block, Allow for Session, or Allow.

## Exceptions - HTTPS-Only Mode

permissions-exceptions-https-only-window2 =
    .title = Exceptions - HTTPS-Only Mode
    .style = { permissions-window2.style }
permissions-exceptions-https-only-desc2 = You can turn off HTTPS-Only Mode for specific websites. { -brand-short-name } won’t attempt to upgrade the connection to secure HTTPS for those sites.

## Exceptions - Pop-ups

permissions-exceptions-popup-window2 =
    .title = Allowed Websites - Pop-ups
    .style = { permissions-window2.style }
permissions-exceptions-popup-desc = You can specify which websites are allowed to open pop-up windows. Type the exact address of the site you want to allow and then click Allow.

## Exceptions - Saved Logins

permissions-exceptions-saved-logins-window2 =
    .title = Exceptions - Saved Logins
    .style = { permissions-window2.style }
permissions-exceptions-saved-logins-desc = Logins for the following websites will not be saved

## Exceptions - Add-ons

permissions-exceptions-addons-window2 =
    .title = Allowed Websites - Add-ons Installation
    .style = { permissions-window2.style }
permissions-exceptions-addons-desc = You can specify which websites are allowed to install add-ons. Type the exact address of the site you want to allow and then click Allow.

## Site Permissions - Autoplay

permissions-site-autoplay-window2 =
    .title = Settings - Autoplay
    .style = { permissions-window2.style }
permissions-site-autoplay-desc = You can manage the sites that do not follow your default autoplay settings here.

## Site Permissions - Notifications

permissions-site-notification-window2 =
    .title = Settings - Notification Permissions
    .style = { permissions-window2.style }
permissions-site-notification-desc = The following websites have requested to send you notifications. You can specify which websites are allowed to send you notifications. You can also block new requests asking to allow notifications.
permissions-site-notification-disable-label =
    .label = Block new requests asking to allow notifications
permissions-site-notification-disable-desc = This will prevent any websites not listed above from requesting permission to send notifications. Blocking notifications may break some website features.

## Site Permissions - Location

permissions-site-location-window2 =
    .title = Settings - Location Permissions
    .style = { permissions-window2.style }
permissions-site-location-desc = The following websites have requested to access your location. You can specify which websites are allowed to access your location. You can also block new requests asking to access your location.
permissions-site-location-disable-label =
    .label = Block new requests asking to access your location
permissions-site-location-disable-desc = This will prevent any websites not listed above from requesting permission to access your location. Blocking access to your location may break some website features.

## Site Permissions - Virtual Reality

permissions-site-xr-window2 =
    .title = Settings - Virtual Reality Permissions
    .style = { permissions-window2.style }
permissions-site-xr-desc = The following websites have requested to access your virtual reality devices. You can specify which websites are allowed to access your virtual reality devices. You can also block new requests asking to access your virtual reality devices.
permissions-site-xr-disable-label =
    .label = Block new requests asking to access your virtual reality devices
permissions-site-xr-disable-desc = This will prevent any websites not listed above from requesting permission to access your virtual reality devices. Blocking access to your virtual reality devices may break some website features.

## Site Permissions - Camera

permissions-site-camera-window2 =
    .title = Settings - Camera Permissions
    .style = { permissions-window2.style }
permissions-site-camera-desc = The following websites have requested to access your camera. You can specify which websites are allowed to access your camera. You can also block new requests asking to access your camera.
permissions-site-camera-disable-label =
    .label = Block new requests asking to access your camera
permissions-site-camera-disable-desc = This will prevent any websites not listed above from requesting permission to access your camera. Blocking access to your camera may break some website features.

## Site Permissions - Microphone

permissions-site-microphone-window2 =
    .title = Settings - Microphone Permissions
    .style = { permissions-window2.style }
permissions-site-microphone-desc = The following websites have requested to access your microphone. You can specify which websites are allowed to access your microphone. You can also block new requests asking to access your microphone.
permissions-site-microphone-disable-label =
    .label = Block new requests asking to access your microphone
permissions-site-microphone-disable-desc = This will prevent any websites not listed above from requesting permission to access your microphone. Blocking access to your microphone may break some website features.

## Site Permissions - Speaker
##
## "Speaker" refers to an audio output device.

permissions-site-speaker-window =
    .title = Settings - Speaker Permissions
    .style = { permissions-window2.style }
permissions-site-speaker-desc = The following websites have requested to select an audio output device. You can specify which websites are allowed to select an audio output device.

permissions-exceptions-doh-window =
    .title = Website Exceptions for DNS over HTTPS
    .style = { permissions-window2.style }
permissions-exceptions-manage-doh-desc = { -brand-short-name } won’t use secure DNS on these sites and their subdomains.

permissions-doh-entry-field = Enter website domain name
    .accesskey = d

permissions-doh-add-exception =
    .label = Add
    .accesskey = A

permissions-doh-col =
    .label = Domain

permissions-doh-remove =
    .label = Remove
    .accesskey = R

permissions-doh-remove-all =
    .label = Remove All
    .accesskey = e
