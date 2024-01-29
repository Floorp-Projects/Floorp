# Onboarding flow

Onboarding flow comprises of entire flow users have after Firefox has successfully been installed or upgraded.

For new users, the first instance of new tab shows relevant messaging on about:welcome. For existing users, an upgrade dialog with release highlights is shown on major release upgrades.


## New User Onboarding

A full-page multistep experience that shows up on first run since Fx80 with `browser.aboutwelcome.enabled` pref as `true`. Setting `browser.aboutwelcome.enabled` to `false` takes user to about:newtab and hides about:welcome.

### Default values

Multistage proton onboarding experience is live since Fx89 and its major variations are:

#### Zero onboarding

No about:welcome experience is shown (users see about:newtab during first run).

Testing instructions: Set `browser.aboutwelcome.enabled` to `false` in about:config

#### Proton

A full-page multistep experience that shows a large splash screen and several subsequent screens. See [Default experience variations](#default-experience-variations) for more information.

#### Return to AMO (RTAMO)

Special custom onboarding experience shown to users when they try to download an addon from addons.mozilla.org but don’t have Firefox installed. This experience allows them to install the addon they were trying to install directly from a button on [RTAMO](https://docs.google.com/document/d/1QOJ8P0xQbdynAmEzOIx8I5qARwA-VqmOMpHzK9h9msg/edit?usp=sharing).

Note that this uses [attribution data](https://docs.google.com/document/d/1zB5zwiyNVOiTD4I3aZ-Wm8KFai9nnWuRHsPg-NW4tcc/edit#heading=h.szk066tfte4n) added to the browser during the download process, which is only currently implemented for Windows.

Testing instructions:
- Set pref browser.newtabpage.activity-stream.asrouter.devtoolsEnabled as true
- Open about:newtab#devtools
- Click Targeting -> Attribution -> Force Attribution
- Open about:welcome, should display RTAMO page

### General capabilities
- Run experiments and roll-outs through Nimbus (see [FeatureManifests](https://searchfox.org/mozilla-central/rev/5e955a47c4af398e2a859b34056017764e7a2252/toolkit/components/nimbus/FeatureManifest.js#56)), only windows is supported. FeatureConfig (from prefs or experiments) has higher precedence to defaults. See [Default experience variations](#default-experience-variations)
- AboutWelcomeDefaults methods [getDefaults](https://searchfox.org/mozilla-central/rev/81c32a2ea5605c5cb22bd02d28c362c140b5cfb4/browser/components/newtab/aboutwelcome/lib/AboutWelcomeDefaults.jsm#539) and [prepareContentForReact](https://searchfox.org/mozilla-central/rev/81c32a2ea5605c5cb22bd02d28c362c140b5cfb4/browser/components/newtab/aboutwelcome/lib/AboutWelcomeDefaults.jsm#566) have dynamic rules which are applied to both experiments and default UI before content is shown to user.
- about:welcome only shows up for users who download Firefox Beta or release (currently not enabled on Nightly)
- [Enterprise builds](https://searchfox.org/mozilla-central/rev/5e955a47c4af398e2a859b34056017764e7a2252/browser/components/enterprisepolicies/Policies.jsm#1385) can turn off about:welcome by setting the browser.aboutwelcome.enabled preference to false.

### Default experience variations
In order of precedence:
- Has AMO attribution
   - Return to AMO
- Experiments
- Defaults
  - Proton default content with below screens
    - Welcome Screen with option to 'Pin Firefox', 'Set default' or 'Get Started'
    - Import screen allows user to import password, bookmarks and browsing history from previous browser.
    - Set a theme lets users personalize Firefox with a theme.

## Upgrade Dialog
Upgrade Dialog was first introduced in Fx89 with MR1 release. It replaces whatsnew tab with an upgrade modal explaining proton changes, setting Firefox as default and/or pinning, and allowing theme change.

### Feature Details:
- Hides whatsnew tab on release channel when Upgrade Modal is shown
- Modal dialog appears on major version upgrade to 89 for MR1
  - It’s a window modal preventing access to tabs and other toolbar UI
- Support desired content and actions on each screen. For MR1 initial screen explains proton changes, highlight option to set Firefox as default and pin.  Subsequent screen allows theme changes.

### Testing Instructions:
- In about:config, set:
  - `browser.startup.homepage_override.mstone` to `88.0` . The dialog only shows after it detects a major upgrade and need to set to 88 to trigger MR1 upgrade dialog.
  - Ensure pref `browser.startup.upgradeDialog.version` is empty. After the dialog shows, `browser.startup.upgradeDialog.version` remembers what version of the dialog to avoid reshowing.
- Restart Firefox
