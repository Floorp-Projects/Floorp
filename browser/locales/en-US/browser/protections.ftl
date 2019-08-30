# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Variables:
#   $count (Number) - Number of tracking events blocked.
graph-week-summary =
  { $count ->
     [one] { -brand-short-name } blocked { $count } tracker over the past week
    *[other] { -brand-short-name } blocked { $count } trackers over the past week
  }

# Variables:
#   $count (Number) - Number of tracking events blocked.
#   $earliestDate (Number) - Unix timestamp in ms, representing a date. The
# earliest date recorded in the database.
graph-total-tracker-summary =
  { $count ->
     [one] <b>{ $count }</b> tracker blocked since { DATETIME($earliestDate, day: "numeric", month: "long", year: "numeric") }
    *[other] <b>{ $count }</b> trackers blocked since { DATETIME($earliestDate, day: "numeric", month: "long", year: "numeric") }
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
social-tab-contant = Social networks place trackers on other websites to follow what you do, see, and watch online. This allows social media companies to learn more about you beyond what you share on your social media profiles. <a data-l10n-name="learn-more-link">Learn more</a>

cookie-tab-title = Cross-Site Tracking Cookies
cookie-tab-content = These cookies follow you from site to site to gather data about what you do online. They are set by third parties such as advertisers and analytics companies. Blocking cross-site tracking cookies reduces the number of ads that follow you around. <a data-l10n-name="learn-more-link">Learn more</a>

tracker-tab-title = Tracking Content
tracker-tab-description = Websites may load external ads, videos, and other content with tracking code. Blocking tracking content can help sites load faster, but some buttons, forms, and login fields might not work. <a data-l10n-name="learn-more-link">Learn more</a>

fingerprinter-tab-title = Fingerprinters
fingerprinter-tab-content = Fingerprinters collect settings from your browser and computer to create a profile of you. Using this digital fingerprint, they can track you across different websites. <a data-l10n-name="learn-more-link">Learn more</a>

cryptominer-tab-title = Cryptominers
cryptominer-tab-content = Cryptominers use your system’s computing power to mine digital money. Cryptomining scripts drain your battery, slow down your computer, and can increase your energy bill. <a data-l10n-name="learn-more-link">Learn more</a>

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
     [one] Password stored securely <a data-l10n-name="lockwise-how-it-works">How it works</a>
    *[other] Passwords stored securely <a data-l10n-name="lockwise-how-it-works">How it works</a>
  }

turn-on-sync = Turn on { -sync-brand-short-name }…
  .title = Go to sync preferences

manage-devices = Manage devices

# Variables:
#   $count (Number) - Number of devices connected with sync.
lockwise-sync-status =
  { $count ->
     [one] Syncing to { $count } other device
    *[other] Syncing to { $count } other devices
  }
lockwise-sync-not-syncing-devices = Not syncing to other devices

monitor-title = Look out for data breaches
monitor-link = How it works
monitor-header-content-no-account = Check { -monitor-brand-name } to see if you’ve been part of a known data breach, and get alerts about new breaches.
monitor-header-content-signed-in = { -monitor-brand-name } warns you if your info has appeared in a known data breach.
monitor-sign-up = Sign Up for Breach Alerts
auto-scan = Automatically scanned today

# This string is displayed after a large numeral that indicates the total number
# of email addresses being monitored. Don’t add $count to
# your localization, because it would result in the number showing twice.
info-monitored-emails =
  { $count ->
     [one] Email address being monitored
    *[other] Email addresses being monitored
  }

# This string is displayed after a large numeral that indicates the total number
# of known data breaches. Don’t add $count to
# your localization, because it would result in the number showing twice.
info-known-breaches-found =
  { $count ->
     [one] Known data breach has exposed your information
    *[other] Known data breaches have exposed your information
  }

# This string is displayed after a large numeral that indicates the total number
# of exposed passwords. Don’t add $count to
# your localization, because it would result in the number showing twice.
info-exposed-passwords-found =
  { $count ->
     [one] Password exposed across all breaches
    *[other] Passwords exposed across all breaches
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

## The title attribute is used to display the type of protection.
## The aria-label is spoken by screen readers to make the visual graph accessible to blind users.
##
## Variables:
##   $count (Number) - Number of specific trackers
##   $percentage (Number) - Percentage this type of tracker contributes to the whole graph

bar-tooltip-social =
  .title = Social Media Trackers
  .aria-label =
    { $count ->
       [one] { $count } social media tracker ({ $percentage }%)
      *[other] { $count } social media trackers ({ $percentage }%)
    }
bar-tooltip-cookie =
  .title = Cross-Site Tracking Cookies
  .aria-label =
    { $count ->
       [one] { $count } cross-site tracking cookie ({ $percentage }%)
      *[other] { $count } cross-site tracking cookies ({ $percentage }%)
    }
bar-tooltip-tracker =
  .title = Tracking Content
  .aria-label =
    { $count ->
       [one] { $count } tracking content ({ $percentage }%)
      *[other] { $count } tracking content ({ $percentage }%)
    }
bar-tooltip-fingerprinter =
  .title = Fingerprinters
  .aria-label =
    { $count ->
       [one] { $count } fingerprinter ({ $percentage }%)
      *[other] { $count } fingerprinters ({ $percentage }%)
    }
bar-tooltip-cryptominer =
  .title = Cryptominers
  .aria-label =
    { $count ->
       [one] { $count } cryptominer ({ $percentage }%)
      *[other] { $count } cryptominers ({ $percentage }%)
    }
