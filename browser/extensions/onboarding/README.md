# Onboarding

System addon to provide the onboarding overlay for user friendly tours.

## Architecture

Everytime `about:home` or `about:newtab` page is opened, onboarding overlay is injected into that page (if `browser.onboarding.enabled` preference is `true`).

## Landing rules

We would apply some rules:

* Avoid `chrome://` in `onbaording.js` since onboarding is intented to be injected into a normal content process page.
* All styles and ids should be formated as `onboarding-*` to avoid conflict with the origin page.
* All strings in `locales` should be formated as `onboarding.*` for consistency.

## How to pump tour set version after update tours

The tourset version is used to track the last major tourset change version. The `tourset-version` pref store the major tourset version (ex: `1`) but not the current browser version. When browser update to the next version (ex: 58, 59) the tourset pref is still `1` if we didn't do any major tourset update.

Once the tour set version is updated (ex: `2`), onboarding overlay should show the update tour to the updated user (ex: update from v56 -> v57), even when user have watched the previous tours or preferred to hide the previous tours.

Edit `browser/app/profile/firefox.js` and set `browser.onboarding.tourset-version` as `[tourset version]` (in integer format).

For example if we update the tourset in v60 and decide to show all update users the tour, we set `browser.onboarding.tourset-version`  as `3`.
