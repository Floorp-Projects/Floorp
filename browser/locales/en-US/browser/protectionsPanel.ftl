# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

protections-panel-sendreportview-error = There was an error sending the report. Please try again later.

# A link shown when ETP is disabled for a site. Opens the breakage report subview when clicked.
protections-panel-sitefixedsendreport-label = Site fixed? Send report

## These strings are used to define the different levels of
## Enhanced Tracking Protection.

protections-popup-footer-protection-label-strict = Strict
  .label = Strict
protections-popup-footer-protection-label-custom = Custom
  .label = Custom
protections-popup-footer-protection-label-standard = Standard
  .label = Standard

##

# The text a screen reader speaks when focused on the info button.
protections-panel-etp-more-info =
  .aria-label = More information about Enhanced Tracking Protection

protections-panel-etp-on-header = Enhanced Tracking Protection is ON for this site
protections-panel-etp-off-header = Enhanced Tracking Protection is OFF for this site

# The link to be clicked to open the sub-panel view
protections-panel-site-not-working = Site not working?

# The heading/title of the sub-panel view
protections-panel-site-not-working-view =
  .title = Site Not Working?

## The "Allowed" header also includes a "Why?" link that, when hovered, shows
## a tooltip explaining why these items were not blocked in the page.

protections-panel-not-blocking-why-label = Why?
protections-panel-not-blocking-why-etp-on-tooltip = Blocking these could break elements of some websites. Without trackers, some buttons, forms, and login fields might not work.
protections-panel-not-blocking-why-etp-off-tooltip = All trackers on this site have been loaded because protections are turned off.

##

protections-panel-no-trackers-found = No trackers known to { -brand-short-name } were detected on this page.

protections-panel-content-blocking-tracking-protection = Tracking Content

protections-panel-content-blocking-socialblock = Social Media Trackers
protections-panel-content-blocking-cryptominers-label = Cryptominers
protections-panel-content-blocking-fingerprinters-label = Fingerprinters

## In the protections panel, Content Blocking category items are in three sections:
##   "Blocked" for categories being blocked in the current page,
##   "Allowed" for categories detected but not blocked in the current page, and
##   "None Detected" for categories not detected in the current page.
##   These strings are used in the header labels of each of these sections.

protections-panel-blocking-label = Blocked
protections-panel-not-blocking-label = Allowed
protections-panel-not-found-label = None Detected

##

protections-panel-settings-label = Protection Settings
protections-panel-protectionsdashboard-label = Protections Dashboard

## In the Site Not Working? view, we suggest turning off protections if
## the user is experiencing issues with any of a variety of functionality.

# The header of the list
protections-panel-site-not-working-view-header = Turn off protections if you’re having issues with:

# The list items, shown in a <ul>
protections-panel-site-not-working-view-issue-list-login-fields = Login fields
protections-panel-site-not-working-view-issue-list-forms = Forms
protections-panel-site-not-working-view-issue-list-payments = Payments
protections-panel-site-not-working-view-issue-list-comments = Comments
protections-panel-site-not-working-view-issue-list-videos = Videos

protections-panel-site-not-working-view-send-report = Send a report

##

protections-panel-cross-site-tracking-cookies = These cookies follow you from site to site to gather data about what you do online. They are set by third parties such as advertisers and analytics companies.
protections-panel-cryptominers = Cryptominers use your system’s computing power to mine digital money. Cryptomining scripts drain your battery, slow down your computer, and can increase your energy bill.
protections-panel-fingerprinters = Fingerprinters collect settings from your browser and computer to create a profile of you. Using this digital fingerprint, they can track you across different websites.
protections-panel-tracking-content = Websites may load external ads, videos, and other content with tracking code. Blocking tracking content can help sites load faster, but some buttons, forms, and login fields might not work.
protections-panel-social-media-trackers = Social networks place trackers on other websites to follow what you do, see, and watch online. This allows social media companies to learn more about you beyond what you share on your social media profiles.

protections-panel-description-shim-allowed = Some trackers marked below have been partially unblocked on this page because you interacted with them.
protections-panel-description-shim-allowed-learn-more = Learn more
protections-panel-shim-allowed-indicator =
  .tooltiptext = Tracker partially unblocked

protections-panel-content-blocking-manage-settings =
  .label = Manage Protection Settings
  .accesskey = M

protections-panel-content-blocking-breakage-report-view =
  .title = Report a Broken Site
protections-panel-content-blocking-breakage-report-view-description = Blocking certain trackers can cause problems with some websites. Reporting these problems helps make { -brand-short-name } better for everyone. Sending this report will send a URL and information about your browser settings to Mozilla. <label data-l10n-name="learn-more">Learn more</label>
protections-panel-content-blocking-breakage-report-view-collection-url = URL
protections-panel-content-blocking-breakage-report-view-collection-url-label =
  .aria-label = URL
protections-panel-content-blocking-breakage-report-view-collection-comments = Optional: Describe the problem
protections-panel-content-blocking-breakage-report-view-collection-comments-label =
  .aria-label = Optional: Describe the problem
protections-panel-content-blocking-breakage-report-view-cancel =
  .label = Cancel
protections-panel-content-blocking-breakage-report-view-send-report =
  .label = Send Report
