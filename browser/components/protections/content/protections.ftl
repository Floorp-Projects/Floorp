# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

# Variables:
#   $count (Number) - Number of tracking events blocked.
graph-week-summary =
  { $count ->
     [one] { -brand-short-name } blocked  { $count } tracker over the past week
    *[other] { -brand-short-name } blocked { $count } trackers over the past week
  }

# Variables:
#   $count (Number) - Number of tracking events blocked.
#   $earliestDate (Number) - Unix timestamp in ms, representing a date. The
# earliest date recorded in the database.
graph-total-summary =
  { $count ->
     [one] { $count } tracker blocked since { DATETIME($earliestDate, day: "numeric", month: "long", year: "numeric") }
    *[other] { $count } trackers blocked since { DATETIME($earliestDate, day: "numeric", month: "long", year: "numeric") }
  }

# The terminology used to refer to categories of Content Blocking is also used in chrome/browser/browser.properties and should be translated consistently.
# "Standard" in this case is an adjective, meaning "default" or "normal".
# The category name in the <b> tag will be bold.
protection-header-details-standard = Protection Level is set to <b>Standard</b>
protection-header-details-strict = Protection Level is set to <b>Strict</b>
protection-header-details-custom = Protection Level is set to <b>Custom</b>
protection-report-page-title = Privacy Protections
protection-report-content-title = Privacy Protections

etp-card-title = Enhanced Tracking Protection
etp-card-content = Trackers follow you around online to collect information about your browsing habits and interests. { -brand-short-name } blocks many of these trackers and other malicious scripts.

# This string is used to label the X axis of a graph. Other days of the week are generated via Intl.DateTimeFormat,
# capitalization for this string should match the output for your locale.
graph-today = Today

# This string is used to describe the graph for screenreader users.
graph-legend-description = A graph containing the total number of each type of tracker blocked this week.

social-tab-title = Social Media Trackers
social-tab-contant = Social media like, post, and comment buttons on other websites can track you — even if you don’t use them. Logging in to sites using your Facebook or Twitter account is another way they can track what you do on those sites. We remove these trackers so Facebook and Twitter see less of what you do online. <a data-l10n-name="learn-more-link">Learn more</a>

cookie-tab-title = Cross-Site Tracking Cookies
cookie-tab-content = Cross-site tracking cookies follow you from site to site to collect data about your browsing habits. Advertisers and analytics companies gather this data to create a profile of your interests across many sites. Blocking them reduces the number of personalized ads that follow you around. <a data-l10n-name="learn-more-link">Learn more</a>

tracker-tab-title = Tracking Content
tracker-tab-content = Websites may load outside ads, videos, and other content that contain hidden trackers. Blocking tracking content can make websites load faster, but some buttons, forms, and login fields might not work. <a data-l10n-name="learn-more-link">Learn more</a>

fingerprinter-tab-title = Fingerprinters
fingerprinter-tab-content = Fingerprinting is a form of online tracking that’s different from your real fingerprints. Companies use it to create a unique profile of you using data about your browser, device, and other settings. We block ad trackers from fingerprinting your device. <a data-l10n-name="learn-more-link">Learn more</a>

cryptominer-tab-title = Cryptominers
cryptominer-tab-content = Some websites host hidden malware that secretly uses your system’s computing power to mine cryptocurrency, or digital money. It drains your battery, slows down your computer, and increases your energy bill. We block known cryptominers from using your computing resources to make money. <a data-l10n-name="learn-more-link">Learn more</a>

lockwise-title = Never forget a password again
lockwise-title-logged-in = { -lockwise-brand-name }
lockwise-header-content = { -lockwise-brand-name } securely stores your passwords in your browser.
lockwise-header-content-logged-in = Securely store and sync your passwords to all your devices.
open-about-logins-button = Open in { -brand-short-name }
lockwise-no-logins-content = Get the <a data-l10n-name="lockwise-inline-link">{ -lockwise-brand-name }</a> app to take your passwords everywhere.

# This string is displayed after a large numeral that indicates the total number
# of email addresses being monitored. Don’t add $count to
# your localization, because it would result in the number showing twice.
lockwise-passwords-stored =
  { $count ->
     [one] Password stored securely. <a data-l10n-name="lockwise-how-it-works">How it works</a>
    *[other] Passwords stored securely. <a data-l10n-name="lockwise-how-it-works">How it works</a>
  }

turn-on-sync = Turn on { -sync-brand-short-name }…
  .title = Go to sync preferences

# Variables:
#   $count (Number) - Number of devices connected with sync.
lockwise-sync-status =
  { $count ->
     [one] Syncing to { $count } other device.
    *[other] Syncing to { $count } other devices.
  }
lockwise-sync-not-syncing = Not syncing to other devices.

monitor-title = Look out for data breaches
monitor-link = How it works
monitor-header-content = Check { -monitor-brand-name } to see if you’ve been part of a data breach and get alerts about new breaches.
monitor-header-content-logged-in = { -monitor-brand-name } warns you if your info has appeared in a known data breach
monitor-sign-up = Sign up for Breach Alerts
auto-scan = Automatically scanned today

# This string is displayed after a large numeral that indicates the total number
# of email addresses being monitored. Don’t add $count to
# your localization, because it would result in the number showing twice.
info-monitored-addresses =
  { $count ->
     [one] Email address being monitored.
    *[other] Email addresses being monitored.
  }

# This string is displayed after a large numeral that indicates the total number
# of known data breaches. Don’t add $count to
# your localization, because it would result in the number showing twice.
info-known-breaches =
  { $count ->
     [one] Known data breach has exposed your information.
    *[other] Known data breaches have exposed your information.
  }

# This string is displayed after a large numeral that indicates the total number
# of exposed passwords. Don’t add $count to
# your localization, because it would result in the number showing twice.
info-exposed-passwords =
  { $count ->
     [one] Password exposed across all breaches.
    *[other] Passwords exposed across all breaches.
  }

full-report-link = View full report on <a data-l10n-name="monitor-inline-link">{ -monitor-brand-name }</a>

# This string is displayed after a large numeral that indicates the total number
# of saved logins which may have been exposed. Don’t add $count to
# your localization, because it would result in the number showing twice.
password-warning =
  { $count ->
     [one] Saved login may have been exposed in a data breach. Change this password for better online security. <a data-l10n-name="lockwise-link">View Saved Logins</a>
    *[other] Saved logins may have been exposed in a data breach. Change these passwords for better online security. <a data-l10n-name="lockwise-link">View Saved Logins</a>
  }

# This is the title attribute describing the graph report's link to about:settings#privacy
go-to-privacy-settings = Go to Privacy Settings

# This is the title attribute describing the Lockwise card's link to about:logins
go-to-saved-logins = Go to Saved Logins

bar-tooltip-social =
  .title = Social Media Trackers
bar-tooltip-cookie =
  .title = Cross-Site Tracking Cookies
bar-tooltip-tracker =
  .title = Tracking Content
bar-tooltip-fingerprinter =
  .title = Fingerprinters
bar-tooltip-cryptominer =
  .title = Cryptominers
