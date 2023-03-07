# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

aboutDialog-title =
    .title = About { -brand-full-name }

releaseNotes-link = What’s new

update-checkForUpdatesButton =
    .label = Check for updates
    .accesskey = C

update-updateButton =
    .label = Restart to Update { -brand-shorter-name }
    .accesskey = R

update-checkingForUpdates = Checking for updates…
update-downloading = <img data-l10n-name="icon"/>Downloading update — <label data-l10n-name="download-status"/>
update-downloading-message = Downloading update — <label data-l10n-name="download-status"/>
update-applying = Applying update…

update-failed = Update failed. <label data-l10n-name="failed-link">Download the latest version</label>
update-failed-main =
    Update failed. <a data-l10n-name="failed-link-main">Download the latest version</a>

update-adminDisabled = Updates disabled by your system administrator
update-noUpdatesFound = { -brand-short-name } is up to date
update-otherInstanceHandlingUpdates = { -brand-short-name } is being updated by another instance

update-manual = Updates available at <label data-l10n-name="manual-link"/>

update-unsupported = You can not perform further updates on this system. <label data-l10n-name="unsupported-link">Learn more</label>

update-restarting = Restarting…

channel-description = You are currently on the <label data-l10n-name="current-channel"></label> update channel.

warningDesc-version = { -brand-short-name } is experimental and may be unstable.

aboutdialog-help-user = { -brand-product-name } Help
aboutdialog-submit-feedback = Submit Feedback

about-floorp = <label data-l10n-name="floorp-browser-link">{ -brand-product-name }</label> is one of the domestic browsers developed in Japan. It is based on Firefox and continues to operate under <label data-l10n-name="ablaze-Link">{ -vendor-short-name }</label>, to improve the web. Want to help? <label data-l10n-name="helpus-donateLink">Make a donation</label>
icon-creator = Icon creator <label data-l10n-name="browser-logo-twitter">@CutterKnife_</label> and <label data-l10n-name="brand-logo-twitter">@mwxdxx.</label>
contributors = A list of <label data-l10n-name="about-contributor">contributors and Developers</label>
bottomLinks-license = Licensing Information
bottomLinks-rights = End-User Rights
bottomLinks-privacy = Privacy Policy

# Example of resulting string: 66.0.1 (64-bit)
# Variables:
#   $version (String): version of Firefox, e.g. 66.0.1
#   $bits (Number): bits of the architecture (32 or 64)
aboutDialog-version = { $version } ({ $bits }-bit)

# Example of resulting string: 66.0a1 (2019-01-16) (64-bit)
# Variables:
#   $version (String): version of Firefox for Nightly builds, e.g. 66.0a1
#   $isodate (String): date in ISO format, e.g. 2019-01-16
#   $bits (Number): bits of the architecture (32 or 64)
aboutDialog-version-nightly = { $version } ({ $isodate }) ({ $bits }-bit)
