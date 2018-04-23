# Onboarding

System addon to provide the onboarding overlay for user-friendly tours.

## How to show the onboarding tour

Open `about:config` page and filter with `onboarding` keyword. Then set following preferences:

```
browser.onboarding.disabled = false
browser.onboarding.tour-set = "new" // for new user tour, or "update" for update user tour
```
And make sure the value of `browser.onboarding.tourset-verion` and `browser.onboarding.seen-tourset-verion` are the same.

## How to show the onboarding notification

Besides above settings, notification will wait 5 minutes before showing the first notification on a new profile or the updated user profile (to not put too much information to the user at once).

To manually remove the mute duration, set pref `browser.onboarding.notification.mute-duration-on-first-session-ms` to `0` and notification will be shown at the next time you open `about:home`, `about:newtab`, or `about:welcome`.

## How to show the snippets

Snippets (the remote notification that handled by activity stream) will only be shown after onboarding notifications are all done. You can set preference `browser.onboarding.notification.finished` to `true` to disable onboarding notification and accept snippets right away.

## Architecture

![](https://i.imgur.com/7RK89Zw.png)

During booting from `bootstrap.js`, `OnboardingTourType.jsm` will check the onboarding tour type (`new` and `update` are supported types) and set required initial states into preferences.

Everytime `about:home`, `about:newtab`, or `about:welcome` page is opened, `onboarding.js` is injected into that page via [frame scripts](https://developer.mozilla.org/en-US/Firefox/Multiprocess_Firefox/Message_Manager/Communicating_with_frame_scripts).

Then in `onboarding.js`, all tours are defined inside of `onboardingTourset` dictionary. `getTourIDList` function will load tours from proper preferences. (Check `How to change the order of tours` section for more detail).

When user clicks the action button in each tour, We use [UITour](http://bedrock.readthedocs.io/en/latest/uitour.html) to highlight the correspondent browser UI element. The UITour client is bundled in onboarding addon via `jar.mn`.

## Landing rules

We would apply some rules:

* To avoid conflict with the origin page, all styles and ids should be formatted as `onboarding-*`.
* For consistency and easier filtering, all strings in `locales` should be formatted as `onboarding.*`.
* For consistency, all related preferences should be formatted as `browser.onboarding.*`.
* For accessibility, images that are for presentation only should have `role="presentation"` attribute.

## How to change the order of tours

Edit `browser/app/profile/firefox.js` and modify `browser.onboarding.newtour` for the new user tour or `browser.onboarding.updatetour` for the update user tour. You can change the tour list and the order by concate `tourIds` with `,` sign. You can find available `tourId` from `onboardingTourset` in `onboarding.js`.

## How to pump tour set version after update tours

We only update the tourset version when we have different **update** tourset. Update the new tourset **does not** require update the tourset version.

The tourset version is used to track the last major tourset change version. The `tourset-version` pref store the major tourset version (ex: `1`) but not the current browser version. When browser update to the next version (ex: 58, 59) the tourset pref is still `1` if we didn't do any major tourset update.

Once the tour set version is updated (ex: `2`), onboarding overlay should show the update tour to the updated user (ex: update from v56 -> v57), even when user has watched the previous tours or preferred to hide the previous tours.

Edit `browser/app/profile/firefox.js` and set `browser.onboarding.tourset-version` as `[tourset version]` (in integer format).

For example, if we update the tourset in v60 and decide to show all update users the tour, we set `browser.onboarding.tourset-version`  as `3`.

## Icon states

Onboarding module has two states for its overlay icon: `default` and `watermark`.
By default, it shows `default` state.
When either tours or notifications are all completed, the icon changes to the `watermark` state.
The icon state is stored in `browser.onboarding.state`.
When `tourset-version` is updated, or when we detect the `tour-type` is changed to `update`, icon state will be changed back to the `default` state.

## Customizable preferences

Here are current support preferences that allow to customize the Onboarding's behavior.

| PREF | DESCRIPTION | DEFAULT |
|-----|-------------|:-----:|
| `browser.onboarding.enabled` | disable onboarding experience entirely | true
| `browser.onboarding.notification.finished` | Decide if we want to hide the notification permanently. | false
| `browser.onboarding.notification.mute-duration-on-first-session-ms` |Notification mute duration. It also effect when the speech bubble is hidden and turned into the blue dot | 300000 (5 Min)
| `browser.onboarding.notification.max-life-time-all-tours-ms` | Notification tours will all hide after this period | 1209600000 (10 Days)
| `browser.onboarding.notification.max-life-time-per-tours-ms` | Per Notification tours will hide and show the next tour after this period | 432000000 (5 Days)
| `browser.onboarding.notification.max-prompt-count-per-tour` | Each tour can only show the specific times in notification bar if user didn't interact with the tour notification. | 8
| `browser.onboarding.newtour` | The tourset of new user tour. | performance,private,screenshots,addons,customize,default
| `browser.onboarding.newtour.tooltip` | The string id which is shown in the new user tour's speech bubble. The preffered length is 2 lines. Should use `%S` to denote Firefox (brand short name) in string, or use `%1$S` if the name shows more than 1 time. | `onboarding.overlay-icon-tooltip2`
| `browser.onboarding.updatetour` | The tourset of new user tour. | performance,library,screenshots,singlesearch,customize,sync
| `browser.onboarding.updatetour.tooltip` | The string id which is shown in the update user tour's speech bubble. The preffered length is 2 lines. Should use `%S` to denote Firefox (brand short name) in string, or use `%1$S` if the name shows shows more than 1 time. | `onboarding.overlay-icon-tooltip-updated2`
| `browser.onboarding.default-icon-src` | The default icon url. Should be svg or at least 64x64 | `chrome://branding/content/icon64.png`
| `browser.onboarding.watermark-icon-src` | The watermark icon url. Should be svg or at least 64x64 | `resource://onboarding/img/watermark.svg`
