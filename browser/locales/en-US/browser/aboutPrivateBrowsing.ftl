# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

privatebrowsingpage-open-private-window-label = Open a Private Window
    .accesskey = P
about-private-browsing-search-placeholder = Search the web
about-private-browsing-info-title = You’re in a Private Window
about-private-browsing-search-btn =
    .title = Search the web
# Variables
#  $engine (String): the name of the user's default search engine
about-private-browsing-handoff =
    .title = Search with { $engine } or enter address
about-private-browsing-handoff-no-engine =
    .title = Search or enter address
# Variables
#  $engine (String): the name of the user's default search engine
about-private-browsing-handoff-text = Search with { $engine } or enter address
about-private-browsing-handoff-text-no-engine = Search or enter address
about-private-browsing-not-private = You are currently not in a private window.
about-private-browsing-info-description-private-window = Private window: { -brand-short-name } clears your search and browsing history when you close all private windows. This doesn’t make you anonymous.
about-private-browsing-info-description-simplified = { -brand-short-name } clears your search and browsing history when you close all private windows, but this doesn’t make you anonymous.
about-private-browsing-learn-more-link = Learn more

about-private-browsing-hide-activity = Hide your activity and location, everywhere you browse
about-private-browsing-get-privacy = Get privacy protections everywhere you browse
about-private-browsing-hide-activity-1 = Hide browsing activity and location with { -mozilla-vpn-brand-name }. One click creates a secure connection, even on public Wi-Fi.
about-private-browsing-prominent-cta = Stay private with { -mozilla-vpn-brand-name }

about-private-browsing-focus-promo-cta = Download { -focus-brand-name }
about-private-browsing-focus-promo-header = { -focus-brand-name }: Private browsing on-the-go
about-private-browsing-focus-promo-text = Our dedicated private browsing mobile app clears your history and cookies every time.

## The following strings will be used for experiments in Fx99 and Fx100

about-private-browsing-focus-promo-header-b = Take private browsing to your phone
about-private-browsing-focus-promo-text-b = Use { -focus-brand-name } for those private searches you don’t want your main mobile browser to see.
about-private-browsing-focus-promo-header-c = Next-level privacy on mobile
about-private-browsing-focus-promo-text-c = { -focus-brand-name } clears your history every time while blocking ads and trackers.

# This string is the title for the banner for search engine selection
# in a private window.
# Variables:
#   $engineName (String) - The engine name that will currently be used for the private window.
about-private-browsing-search-banner-title = { $engineName } is your default search engine in Private Windows
about-private-browsing-search-banner-description = {
  PLATFORM() ->
     [windows] To select a different search engine go to <a data-l10n-name="link-options">Options</a>
    *[other] To select a different search engine go to <a data-l10n-name="link-options">Preferences</a>
  }
about-private-browsing-search-banner-close-button =
    .aria-label = Close

about-private-browsing-promo-close-button =
  .title = Close

## Strings used in a “pin promotion” message, which prompts users to pin a private window

about-private-browsing-pin-promo-header = Private browsing freedom in one click
about-private-browsing-pin-promo-link-text = { PLATFORM() ->
    [macos] Keep in Dock
   *[other] Pin to taskbar
}
about-private-browsing-pin-promo-title = No saved cookies or history, right from your desktop. Browse like no one’s watching.
