# First run on-boarding flow

First Run flow describes the entire experience users have after Firefox has successfully been installed up until the first instance of new tab is shown. 
First run help onboard new users by showing relevant messaging on about:welcome and about:newtab using interrupts and triplets. 

## Interrupts
A first run experience shown on about:welcome page and decide UI based on messaging template provided. In Firefox 72, interrupt can be one of below three types

### First Run Modal
A modal that shows up on first run, usually the first stage.

In 71+, below modal interrupts are supported:
* join - purple first run modal with "Meet Firefox + Products / Knowledge Privacy + Join Firefox" messaging
* sync - purple first run modal but with 70 fxa messaging
* modal_control - First Run Modal control with same messaging as “join” modal
* modal_variant_a - First Run Modal with "Get the most + Sync/Monitor/Lockwise  + Start Here" messaging.
* modal_variant_b - First Run Modal with "Supercharge privacy + Sync/Monitor/Lockwise  + Start Here" messaging.
* modal_variant_c - First Run Modal with "Add Privacy + Sync/Monitor/Lockwise  + Start Here" messaging.
* modal_variant_f - First Run Modal with "Meet Firefox + Products / Knowledge Privacy + Start Here" messaging.


### First Run Takeover
A full-page experience that shows up on first run, usually the first stage (a previous variant of this was the blue FxA Sync sign-in page).

A modal less page showing signup form and triplet messaging together on the same page.
* full_page_d - FxA signup form on top with triplet messaging on bottom
* full_page_e - FxA signup form on bottom with triplet messaging on top

### First Run Return to AMO
Part of a custom First Run Flow for users that installed Firefox after attempting to add an add-on from another browser. This is a full-page experience on first run.

Please Note: This is unique interrupt experience shown on about:welcome and not controlled by interrupt value of pref 'trailhead.firstrun.branches' and instead uses attribution targeting condition below

``` "attributionData.campaign == 'non-fx-button' && attributionData.source == 'addons.mozilla.org'"```

## Triplets
The cards that show up above the new tab content on the first instance of new tab, usually the second stage.

* supercharge - Shows Sync, Monitor and Mobile onboarding cards. Supported in 71+.
* payoff - Shows Monitor, Facbook Container and Firefox Send onboarding cards. Supported in 71 only.
* mutidevice - Shows Pocket, Send Tabs and Mobile onboarding cards. Supported in 71 only.
* privacy - Shows Private Browsing, Tracking Protection and Lockwise. Supported in 71 only.

In 72+
* static - same experience as ‘supercharge’ triplet - with Sync, Monitor and Mobile onboarding cards
* dynamic - Dynamic triplets showing three onboarding cards (Sync, Monitor and Private Browsing) that gets swapped with preselected list of cards that satisfies targeting rules. Preselected cards supported are Send Tab, Mobile and Lockwise.
* dynamic_chrome - Dynamic triplets showing three onboarding cards (Chrome switchers, Sync and Monitor) that gets swapped with preselected list of cards that satisfies targeting rules. Preselected cards supported are Private Browsing, Send Tab, Mobile and Lockwise.

## Misc
Below experiences are controlled by using following interrupt values inside 'trailhead.firstrun.branches' 
* nofirstrun - nothing - looks like about:newtab and hides both first and second stage of about:welcome
* cards - no modal straight to triplet. This hides only first stage and takes user straight to triplets on opening about:welcome page.


## How to switch between first run experiences

First run experiences are controlled by pref 'trailhead.firstrun.branches'. This pref value follow format ```'<interrupt>-<triplet>'``` where ```<interrupt>``` is the interrupt message name from interrupt section above and ```<triplet>``` is triplet message name. If no value is set for 'trailhead.firstrun.branches', by default 'join-supercharge' interrupt and triplet experience is used. 'join-supercharge' is default first run experience in 71+.

For Example:
* Open about:config and set preference 'trailhead.firstrun.branches' to string value 'modal_variant_a-supercharge'
* Open about:welcome shows 'modal_variant_a' first run modal stage 1 on welcome screen.
* Dismissing welcome screen by clicking on “Start browsing” shows stage 2 'supercharge' triplets experience on new tab.
