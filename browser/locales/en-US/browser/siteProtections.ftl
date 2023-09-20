# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

content-blocking-trackers-view-empty = None detected on this site

content-blocking-cookies-blocking-trackers-label = Cross-Site Tracking Cookies
content-blocking-cookies-blocking-third-party-label = Third-Party Cookies
content-blocking-cookies-blocking-unvisited-label = Unvisited Site Cookies
content-blocking-cookies-blocking-all-label = All Cookies

content-blocking-cookies-view-first-party-label = From This Site
content-blocking-cookies-view-trackers-label = Cross-Site Tracking Cookies
content-blocking-cookies-view-third-party-label = Third-Party Cookies

# This label is shown next to a cookie origin in the cookies subview.
# It forms the end of the (imaginary) sentence "www.example.com [was] Allowed"
content-blocking-cookies-view-allowed-label =
    .value = Allowed
# This label is shown next to a cookie origin in the cookies subview.
# It forms the end of the (imaginary) sentence "www.example.com [was] Blocked"
content-blocking-cookies-view-blocked-label =
    .value = Blocked

# Variables:
#   $domain (String): the domain of the site.
content-blocking-cookies-view-remove-button =
    .tooltiptext = Clear cookie exception for { $domain }

tracking-protection-icon-active = Blocking social media trackers, cross-site tracking cookies, and fingerprinters.
tracking-protection-icon-active-container =
    .aria-label = { tracking-protection-icon-active }
tracking-protection-icon-disabled = Enhanced Tracking Protection is OFF for this site.
tracking-protection-icon-disabled-container =
    .aria-label = { tracking-protection-icon-disabled }
tracking-protection-icon-no-trackers-detected = No trackers known to { -brand-short-name } were detected on this page.
tracking-protection-icon-no-trackers-detected-container =
    .aria-label = { tracking-protection-icon-no-trackers-detected }

## Variables:
##   $host (String): the site's hostname

# Header of the Protections Panel.
protections-header = Protections for { $host }

## Blocking and Not Blocking sub-views in the Protections Panel

protections-blocking-fingerprinters =
    .title = Fingerprinters Blocked
protections-blocking-cryptominers =
    .title = Cryptominers Blocked
protections-blocking-cookies-trackers =
    .title = Cross-Site Tracking Cookies Blocked
protections-blocking-cookies-third-party =
    .title = Third-Party Cookies Blocked
protections-blocking-cookies-all =
    .title = All Cookies Blocked
protections-blocking-cookies-unvisited =
    .title = Unvisited Site Cookies Blocked
protections-blocking-tracking-content =
    .title = Tracking Content Blocked
protections-blocking-social-media-trackers =
    .title = Social Media Trackers Blocked
protections-not-blocking-fingerprinters =
    .title = Not Blocking Fingerprinters
protections-not-blocking-cryptominers =
    .title = Not Blocking Cryptominers
protections-not-blocking-cookies-third-party =
    .title = Not Blocking Third-Party Cookies
protections-not-blocking-cookies-all =
    .title = Not Blocking Cookies
protections-not-blocking-cross-site-tracking-cookies =
    .title = Not Blocking Cross-Site Tracking Cookies
protections-not-blocking-tracking-content =
    .title = Not Blocking Tracking Content
protections-not-blocking-social-media-trackers =
    .title = Not Blocking Social Media Trackers

## Footer and Milestones sections in the Protections Panel
## Variables:
##   $trackerCount (Number): number of trackers blocked
##   $date (Date): the date on which we started counting

# This text indicates the total number of trackers blocked on all sites.
# In its tooltip, we show the date when we started counting this number.
protections-footer-blocked-tracker-counter =
    { $trackerCount ->
        [one] { $trackerCount } Blocked
       *[other] { $trackerCount } Blocked
    }
    .tooltiptext = Since { DATETIME($date, year: "numeric", month: "long", day: "numeric") }
# This text indicates the total number of trackers blocked on all sites.
# It should be the same as protections-footer-blocked-tracker-counter;
# this message is used to leave out the tooltip when the date is not available.
protections-footer-blocked-tracker-counter-no-tooltip =
    { $trackerCount ->
        [one] { $trackerCount } Blocked
       *[other] { $trackerCount } Blocked
    }

# In English this looks like "Firefox blocked over 10,000 trackers since October 2019"
protections-milestone =
    { $trackerCount ->
        [one] { -brand-short-name } blocked { $trackerCount } tracker since { DATETIME($date, year: "numeric", month: "long") }
       *[other] { -brand-short-name } blocked over { $trackerCount } trackers since { DATETIME($date, year: "numeric", month: "long") }
    }
