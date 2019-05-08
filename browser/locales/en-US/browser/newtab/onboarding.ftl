# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

## UI strings for the simplified onboarding modal

onboarding-button-label-learn-more = Learn More
onboarding-button-label-try-now = Try It Now
onboarding-button-label-get-started = Get Started

onboarding-welcome-header = Welcome to { -brand-short-name }
onboarding-welcome-body = You’ve got the browser.<br/>Meet the rest of { -brand-product-name }.
onboarding-welcome-learn-more = Learn more about the benefits.

onboarding-join-form-header = Join { -brand-product-name }
onboarding-join-form-body = Enter your email address to get started.
onboarding-join-form-email =
    .placeholder = Enter email
onboarding-join-form-email-error = Valid email required
onboarding-join-form-legal = By proceeding, you agree to the <a data-l10n-name="terms">Terms of Service</a> and <a data-l10n-name="privacy">Privacy Notice</a>.
onboarding-join-form-continue = Continue

onboarding-start-browsing-button-label = Start Browsing

## These are individual benefit messages shown with an image, title and
## description.

onboarding-benefit-products-title = Useful Products
onboarding-benefit-products-text = Get things done with a family of tools that respects your privacy across your devices.

onboarding-benefit-knowledge-title = Practical Knowledge
onboarding-benefit-knowledge-text = Learn everything you need to know to stay smarter and safer online.

onboarding-benefit-privacy-title = True Privacy
# "Personal Data Promise" is a concept that should be translated consistently
# across the product. It refers to a concept shown elsewhere to the user: "The
# Firefox Personal Data Promise is the way we honor your data in everything we
# make and do. We take less data. We keep it safe. And we make sure that we are
# transparent about how we use it."
onboarding-benefit-privacy-text = Everything we do honors our Personal Data Promise: Take less. Keep it safe. No secrets.


## These strings belong to the individual onboarding messages.

## Each message has a title and a description of what the browser feature is.
## Each message also has an associated button for the user to try the feature.
## The string for the button is found above, in the UI strings section
onboarding-private-browsing-title = Private Browsing
onboarding-private-browsing-text = Browse by yourself. Private Browsing with Content Blocking blocks online trackers that follow you around the web.

onboarding-screenshots-title = Screenshots
onboarding-screenshots-text = Take, save and share screenshots - without leaving { -brand-short-name }. Capture a region or an entire page as you browse. Then save to the web for easy access and sharing.

onboarding-addons-title = Add-ons
onboarding-addons-text = Add even more features that make { -brand-short-name } work harder for you. Compare prices, check the weather or express your personality with a custom theme.

onboarding-ghostery-title = Ghostery
onboarding-ghostery-text = Browse faster, smarter, or safer with extensions like Ghostery, which lets you block annoying ads.

# Note: "Sync" in this case is a generic verb, as in "to synchronize"
onboarding-fxa-title = Sync
onboarding-fxa-text = Sign up for a { -fxaccount-brand-name } and sync your bookmarks, passwords, and open tabs everywhere you use { -brand-short-name }.

onboarding-tracking-protection-title = Control How You’re Tracked
onboarding-tracking-protection-text = Don’t like when ads follow you around? { -brand-short-name } helps you control how advertisers track your activity online.
# "Update" is a verb, as in "Update the existing settings", not "Options about
# updates".
onboarding-tracking-protection-button = { PLATFORM() ->
  [windows] Update Options
 *[other] Update Preferences
}

onboarding-data-sync-title = Take Your Settings with You
# "Sync" is short for synchronize.
onboarding-data-sync-text = Sync your bookmarks and passwords everywhere you use { -brand-product-name }.
onboarding-data-sync-button = Turn on { -sync-brand-short-name }

onboarding-firefox-monitor-title = Stay Alert to Data Breaches
onboarding-firefox-monitor-text = { -monitor-brand-name } monitors if your email has appeared in a data breach and alerts you if it appears in a new breach.
onboarding-firefox-monitor-button = Sign up for Alerts

onboarding-browse-privately-title = Browse Privately
onboarding-browse-privately-text = Private Browsing clears your search and browsing history to keep it secret from anyone who uses your computer.
onboarding-browse-privately-button = Open a Private Window

onboarding-firefox-send-title = Keep Your Shared Files Private
onboarding-firefox-send-text = { -send-brand-name } protects the files you share with end-to-end encryption and a link that automatically expires.
onboarding-firefox-send-button = Try { -send-brand-name }

onboarding-mobile-phone-title = Get { -brand-product-name } on Your Phone
onboarding-mobile-phone-text = Download { -brand-product-name } for iOS or Android and sync your data across devices.
# "Mobile" is short for mobile/cellular phone, "Browser" is short for web
# browser.
onboarding-mobile-phone-button = Download Mobile Browser

onboarding-send-tabs-title = Instantly Send Yourself Tabs
# "Send Tabs" refers to "Send Tab to Device" feature that appears when opening a
# tab's context menu.
onboarding-send-tabs-text = Send Tabs instantly shares pages between your devices without having to copy, paste, or leave the browser.
onboarding-send-tabs-button = Start Using Send Tabs

onboarding-pocket-anywhere-title = Read and Listen Anywhere
# "downtime" refers to the user's free/spare time.
onboarding-pocket-anywhere-text = { -pocket-brand-name } saves your favorite stories so you can read, listen, and watch during your downtime, even if you’re offline.
onboarding-pocket-anywhere-button = Try { -pocket-brand-name }

onboarding-lockwise-passwords-title = Take Your Passwords Everywhere
onboarding-lockwise-passwords-text = { -lockwise-brand-name } saves your passwords in a secure place so you can easily log in to your accounts.
onboarding-lockwise-passwords-button = Get { -lockwise-brand-name }

onboarding-facebook-container-title = Set Boundaries with Facebook
onboarding-facebook-container-text = { -facebook-container-brand-name } keeps your Facebook identity separate from everything else, making it harder to track you across the web.
onboarding-facebook-container-button = Add the Extension


## Message strings belonging to the Return to AMO flow
return-to-amo-sub-header = Great, you’ve got { -brand-short-name }

# <icon></icon> will be replaced with the icon belonging to the extension
#
# Variables:
#   $addon-name (String) - Name of the add-on
return-to-amo-addon-header = Now let’s get you <icon></icon><b>{ $addon-name }.</b>
return-to-amo-extension-button = Add the Extension
return-to-amo-get-started-button = Get Started with { -brand-short-name }
