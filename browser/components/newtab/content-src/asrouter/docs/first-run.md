# First run on-boarding flow

First Run flow describes the entire experience users have after Firefox has successfully been installed up until the first instance of new tab is shown. 
First run help onboard new users by showing relevant messaging on about:welcome and about:newtab using triplets. 

### First Run Multistage
A full-page multistep experience that shows up on first run since Fx80 with browser.aboutwelcome.enabled pref as true.

Setting browser.aboutwelcome.enabled to false make first run looks like about:newtab and hides about:welcome

## Triplets
The cards that show up above the new tab content on the first instance of new tab. Setting browser.aboutwelcome.enabled to false and trailhead.firstrun.newtab.triplets to one of values below hides multistage welcome and takes user straight to triplets on opening about:welcome page

* supercharge - Shows Sync, Monitor and Mobile onboarding cards. Supported in 71+.
* payoff - Shows Monitor, Facbook Container and Firefox Send onboarding cards. Supported in 71 only.
* mutidevice - Shows Pocket, Send Tabs and Mobile onboarding cards. Supported in 71 only.
* privacy - Shows Private Browsing, Tracking Protection and Lockwise. Supported in 71 only.

In 72+
* static - same experience as ‘supercharge’ triplet - with Sync, Monitor and Mobile onboarding cards
* dynamic - Dynamic triplets showing three onboarding cards (Sync, Monitor and Private Browsing) that gets swapped with preselected list of cards that satisfies targeting rules. Preselected cards supported are Send Tab, Mobile and Lockwise.
* dynamic_chrome - Dynamic triplets showing three onboarding cards (Chrome switchers, Sync and Monitor) that gets swapped with preselected list of cards that satisfies targeting rules. Preselected cards supported are Private Browsing, Send Tab, Mobile and Lockwise.
