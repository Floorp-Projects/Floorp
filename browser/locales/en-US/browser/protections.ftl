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

# Text displayed instead of the graph when in Private Mode
graph-private-window = { -brand-short-name } continues to  block trackers in Private Windows, but does not keep a record of what was blocked.
# Weekly summary of the graph when the graph is empty in Private Mode
graph-week-summary-private-window = Trackers { -brand-short-name } blocked this week

# The terminology used to refer to categories of Content Blocking is also used in chrome/browser/browser.properties and should be translated consistently.
# "Standard" in this case is an adjective, meaning "default" or "normal".
# The category name in the <b> tag will be bold.
protection-report-header-details-standard = Protection Level is set to <b>Standard</b>
  .title = Go to Privacy Settings
protection-report-header-details-strict = Protection Level is set to <b>Strict</b>
  .title = Go to Privacy Settings
protection-report-header-details-custom = Protection Level is set to <b>Custom</b>
  .title = Go to Privacy Settings
protection-report-page-title = Privacy Protections
protection-report-content-title = Privacy Protections

etp-card-title = Enhanced Tracking Protection
etp-card-content = Trackers follow you around online to collect information about your browsing habits and interests. { -brand-short-name } blocks many of these trackers and other malicious scripts.
protection-report-etp-card-content-custom-not-blocking = All protections are currently turned off. Choose which trackers to block by managing your { -brand-short-name } protections settings.
protection-report-manage-protections = Manage Settings

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

protections-close-button2 =
  .aria-label = Close
  .title = Close
  
mobile-app-title = Block ad trackers across more devices
mobile-app-card-content = Use the mobile browser with built-in protection against ad tracking. 
mobile-app-links = { -brand-product-name } Browser for <a data-l10n-name="android-mobile-inline-link">Android</a> and <a data-l10n-name="ios-mobile-inline-link">iOS</a>

lockwise-title = Never forget a password again
lockwise-title-logged-in2 = Password Management
lockwise-header-content = { -lockwise-brand-name } securely stores your passwords in your browser.
lockwise-header-content-logged-in = Securely store and sync your passwords to all your devices.
protection-report-save-passwords-button = Save Passwords
  .title = Save Passwords on { -lockwise-brand-short-name }
protection-report-manage-passwords-button = Manage Passwords
  .title = Manage Passwords on { -lockwise-brand-short-name }
lockwise-mobile-app-title = Take your passwords everywhere
lockwise-no-logins-card-content = Use passwords saved in { -brand-short-name } on any device.
lockwise-app-links = { -lockwise-brand-name } for <a data-l10n-name="lockwise-android-inline-link">Android</a> and <a data-l10n-name="lockwise-ios-inline-link">iOS</a>

# Variables:
# $count (Number) - Number of passwords exposed in data breaches.
lockwise-scanned-text-breached-logins = 
  { $count ->
      [one] 1 password may have been exposed in a data breach.
     *[other] { $count } passwords may have been exposed in a data breach.
  }

# While English doesn't use the number in the plural form, you can add $count to your language
# if needed for grammatical reasons.
# Variables:
# $count (Number) - Number of passwords stored in Lockwise.
lockwise-scanned-text-no-breached-logins =
  { $count ->
     [one] 1 password stored securely.
    *[other] Your passwords are being stored securely.
  }
lockwise-how-it-works-link = How it works

turn-on-sync = Turn on { -sync-brand-short-name }…
  .title = Go to sync preferences

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
