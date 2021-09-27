# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

screenshot-toolbarbutton =
  .label = Screenshot
  .tooltiptext = Take a screenshot

screenshots-instructions = Drag or click on the page to select a region. Press ESC to cancel.
screenshots-cancel-button = Cancel
screenshots-save-visible-button = Save visible
screenshots-save-page-button = Save full page
screenshots-download-button = Download
screenshots-download-button-tooltip = Download screenshot
screenshots-copy-button = Copy
screenshots-copy-button-tooltip = Copy screenshot to clipboard

screenshots-meta-key = {
  PLATFORM() ->
    [macos] ⌘
   *[other] Ctrl
}
screenshots-notification-link-copied-title = Link Copied
screenshots-notification-link-copied-details = The link to your shot has been copied to the clipboard. Press {screenshots-meta-key}-V to paste.

screenshots-notification-image-copied-title = Shot Copied
screenshots-notification-image-copied-details = Your shot has been copied to the clipboard. Press {screenshots-meta-key}-V to paste.

screenshots-request-error-title = Out of order.
screenshots-request-error-details = Sorry! We couldn’t save your shot. Please try again later.

screenshots-connection-error-title = We can’t connect to your screenshots.
screenshots-connection-error-details = Please check your Internet connection. If you are able to connect to the Internet, there may be a temporary problem with the { -screenshots-brand-name } service.

screenshots-login-error-details = We couldn’t save your shot because there is a problem with the { -screenshots-brand-name } service. Please try again later.

screenshots-unshootable-page-error-title = We can’t screenshot this page.
screenshots-unshootable-page-error-details = This isn’t a standard Web page, so you can’t take a screenshot of it.

screenshots-empty-selection-error-title = Your selection is too small

screenshots-private-window-error-title = { -screenshots-brand-name } is disabled in Private Browsing Mode
screenshots-private-window-error-details = Sorry for the inconvenience. We are working on this feature for future releases.

screenshots-generic-error-title = Whoa! { -screenshots-brand-name } went haywire.
screenshots-generic-error-details = We’re not sure what just happened. Care to try again or take a shot of a different page?
