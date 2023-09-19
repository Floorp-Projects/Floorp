# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### This file is not in a locales directory to prevent it from
### being translated as the feature is still in heavy development
### and strings are likely to change often.

## Onboarding message strings.

shopping-onboarding-headline = Try our trusted guide to product reviews

## The "by" in "Fakespot by Mozilla" presents localization challenges.
## Once the term is updated in Bug 1847307, we can update its use
## here. (filed under Bug 1850432)

# Dynamic subtitle. Sites are limited to Amazon, Walmart or Best Buy.
# Variables:
#   $currentSite (str) - The current shopping page name
#   $secondSite (str) - A second shopping page name
#   $thirdSite (str) - A third shopping page name
shopping-onboarding-dynamic-subtitle = See how reliable product reviews are on <b>{ $currentSite }</b> before you buy. Review checker is built right into { -brand-product-name } — and it works on <b>{ $secondSite }</b> and <b>{ $thirdSite }</b>, too.

shopping-onboarding-body = Using the power of { -fakespot-brand-full-name }, we help you avoid biased and inauthentic reviews. Our AI model is always improving to protect you as you shop. <a data-l10n-name="learn_more">Learn more</a>
shopping-onboarding-opt-in-privacy-policy-and-terms-of-use = By selecting “{ shopping-onboarding-opt-in-button }“ you agree to { -fakespot-brand-full-name }’s <a data-l10n-name="privacy_policy">privacy policy</a> and <a data-l10n-name="terms_of_use">terms of use.</a>
shopping-onboarding-opt-in-button = Yes, try it
shopping-onboarding-not-now-button = Not now
