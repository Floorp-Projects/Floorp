# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### UI strings for the simplified onboarding modal / about:welcome
### Various strings use a non-breaking space to avoid a single dangling /
### widowed word, so test on various window sizes if you also want this.

## These button action text can be split onto multiple lines, so use explicit
## newlines in translations to control where the line break appears (e.g., to
## avoid breaking quoted text).

onboarding-button-label-learn-more = Learn More
onboarding-button-label-get-started = Get Started

## Welcome modal dialog strings

onboarding-welcome-header = Welcome to { -brand-short-name }
onboarding-welcome-body = You’ve got the browser.<br/>Meet the rest of { -brand-product-name }.
onboarding-welcome-learn-more = Learn more about the benefits.
onboarding-welcome-modal-get-body = You’ve got the browser.<br/>Now get the most out of { -brand-product-name }.
onboarding-welcome-modal-supercharge-body = Supercharge your privacy protection.
onboarding-welcome-modal-privacy-body = You’ve got the browser. Let’s add more privacy protection.
onboarding-welcome-modal-family-learn-more = Learn about the { -brand-product-name } family of products.
onboarding-welcome-form-header = Start Here

onboarding-join-form-body = Enter your email address to get started.
onboarding-join-form-email =
    .placeholder = Enter email
onboarding-join-form-email-error = Valid email required
onboarding-join-form-legal = By proceeding, you agree to the <a data-l10n-name="terms">Terms of Service</a> and <a data-l10n-name="privacy">Privacy Notice</a>.
onboarding-join-form-continue = Continue

# This message is followed by a link using onboarding-join-form-signin ("Sign In") as text.
onboarding-join-form-signin-label = Already have an account?
# Text for link to submit the sign in form
onboarding-join-form-signin = Sign In

onboarding-start-browsing-button-label = Start Browsing
onboarding-cards-dismiss =
    .title = Dismiss
    .aria-label = Dismiss

## Welcome full page string

onboarding-fullpage-welcome-subheader = Let’s start exploring everything you can do.
onboarding-fullpage-form-email =
    .placeholder = Your email address…

## Firefox Sync modal dialog strings.

onboarding-sync-welcome-header = Take { -brand-product-name } with You
onboarding-sync-welcome-content = Get your bookmarks, history, passwords and other settings on all your devices.
onboarding-sync-welcome-learn-more-link = Learn more about Firefox Accounts

onboarding-sync-form-input =
    .placeholder = Email

onboarding-sync-form-continue-button = Continue
onboarding-sync-form-skip-login-button = Skip this step

## This is part of the line "Enter your email to continue to Firefox Sync"

onboarding-sync-form-header = Enter your email
onboarding-sync-form-sub-header = to continue to { -sync-brand-name }


## These are individual benefit messages shown with an image, title and
## description.

onboarding-benefit-products-text = Get things done with a family of tools that respects your privacy across your devices.

# "Personal Data Promise" is a concept that should be translated consistently
# across the product. It refers to a concept shown elsewhere to the user: "The
# Firefox Personal Data Promise is the way we honor your data in everything we
# make and do. We take less data. We keep it safe. And we make sure that we are
# transparent about how we use it."
onboarding-benefit-privacy-text = Everything we do honors our Personal Data Promise: Take less. Keep it safe. No secrets.

onboarding-benefit-sync-title = { -sync-brand-short-name }
onboarding-benefit-sync-text = Take your bookmarks, passwords, history, and more everywhere you use { -brand-product-name }.

onboarding-benefit-monitor-title = { -monitor-brand-short-name }
onboarding-benefit-monitor-text = Get notified when your personal info is in a known data breach.

onboarding-benefit-lockwise-title = { -lockwise-brand-short-name }
onboarding-benefit-lockwise-text = Manage passwords that are protected and portable.


## These strings belong to the individual onboarding messages.

## Each message has a title and a description of what the browser feature is.
## Each message also has an associated button for the user to try the feature.
## The string for the button is found above, in the UI strings section

onboarding-tracking-protection-title2 = Protection From Tracking
onboarding-tracking-protection-text2 = { -brand-short-name } helps stop websites from tracking you online, making it harder for ads to follow you around the web.
onboarding-tracking-protection-button2 = How it Works

onboarding-data-sync-title = Take Your Settings with You
# "Sync" is short for synchronize.
onboarding-data-sync-text2 = Sync your bookmarks, passwords, and more everywhere you use { -brand-product-name }.
onboarding-data-sync-button2 = Sign in to { -sync-brand-short-name }

onboarding-firefox-monitor-title = Stay Alert to Data Breaches
onboarding-firefox-monitor-text2 = { -monitor-brand-name } monitors if your email has appeared in a known data breach and alerts you if it appears in a new breach.
onboarding-firefox-monitor-button = Sign Up for Alerts

onboarding-browse-privately-title = Browse Privately
onboarding-browse-privately-text = Private Browsing clears your search and browsing history to keep it secret from anyone who uses your computer.
onboarding-browse-privately-button = Open a Private Window

onboarding-firefox-send-title = Keep Your Shared Files Private
onboarding-firefox-send-text2 = Upload your files to { -send-brand-name } to share them with end-to-end encryption and a link that automatically expires.
onboarding-firefox-send-button = Try { -send-brand-name }

onboarding-mobile-phone-title = Get { -brand-product-name } on Your Phone
onboarding-mobile-phone-text = Download { -brand-product-name } for iOS or Android and sync your data across devices.
# "Mobile" is short for mobile/cellular phone, "Browser" is short for web
# browser.
onboarding-mobile-phone-button = Download Mobile Browser

onboarding-send-tabs-title = Instantly Send Yourself Tabs
# "Send Tabs" refers to "Send Tab to Device" feature that appears when opening a
# tab's context menu.
onboarding-send-tabs-text2 = Easily share pages between your devices without having to copy links or leave the browser.
onboarding-send-tabs-button = Start Using Send Tabs

onboarding-pocket-anywhere-title = Read and Listen Anywhere
onboarding-pocket-anywhere-text2 = Save your favorite content offline with the { -pocket-brand-name } App and read, listen, and watch whenever it’s convenient for you.
onboarding-pocket-anywhere-button = Try { -pocket-brand-name }

onboarding-lockwise-strong-passwords-title = Create and Store Strong Passwords
onboarding-lockwise-strong-passwords-text = { -lockwise-brand-name } creates strong passwords on the spot and saves all of them in one place.
onboarding-lockwise-strong-passwords-button = Manage Your Logins

onboarding-facebook-container-title = Set Boundaries with Facebook
onboarding-facebook-container-text2 = { -facebook-container-brand-name } keeps your profile separate from everything else, making it harder for Facebook to target you with ads.
onboarding-facebook-container-button = Add the Extension

onboarding-import-browser-settings-title = Import Your Bookmarks, Passwords, and More
onboarding-import-browser-settings-text = Dive right in—easily bring your Chrome sites and settings with you.
onboarding-import-browser-settings-button = Import Chrome Data

onboarding-personal-data-promise-title = Private by Design
onboarding-personal-data-promise-text = { -brand-product-name } treats your data with respect by taking less of it, protecting it, and being clear about how we use it.
onboarding-personal-data-promise-button = Read our Promise

## Message strings belonging to the Return to AMO flow

return-to-amo-sub-header = Great, you’ve got { -brand-short-name }

# <icon></icon> will be replaced with the icon belonging to the extension
#
# Variables:
#   $addon-name (String) - Name of the add-on
return-to-amo-addon-header = Now let’s get you <icon></icon><b>{ $addon-name }.</b>
return-to-amo-extension-button = Add the Extension
return-to-amo-get-started-button = Get Started with { -brand-short-name }
