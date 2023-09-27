# Colors and High Contrast Mode
Firefox offers several customisations to improve the accessibility of colors used to render web content and Firefox chrome. This document describes the customisation options available and their behaviour across platforms. It also describes how these options interact with one another. It is intended for developer reference :)

## The Colors Dialog
In `about:preferences > Language and Appearance`, you'll find a button labelled "Colors". This button launches the colors dialog, which contains all of our user-facing color customisation options, as well as stylistic customisations like link underlining. This dialog also contains the select that controls what we'll refer to as "Firefox High Contrast Mode", or FF HCM. FF HCM can be enabled "Always", "Never", or "Only with High Contrast Themes".
> Note: FF HCM only affects web content, so changing the option in this select will only alter color usage for web pages. It will not change FF chrome. Current behaviour on chrome pages (ie. `about:` pages) is undefined.

### User-customisable Colors
Users can choose to override background color, foreground color, visited link color, and/or unvisited link color by selecting a new color from the color inputs in the dialog. Modifications to these colors are stored in their corresponding user preference:
- `browser.background_color`
- `browser.foreground_color`
- `browser.visited_color`
- `browser.anchor_color`

### Color Usage and System Colors
Before we render any Firefox/web content, we need to select a color palette to render that content _with_. We don't always use the colors a user has selected in the colors dialog. In fact, there are three different sets of colors we can use to style Firefox and/or web content:
- Stand-in colors
- System colors
- Colors-dialog colors

> Note: Web pages may supply their own style sheets, which override a user's chosen color palette. When FF HCM is set to "Always", or set to "With High Contrast Themes" and OS HCM is enabled, the chosen color palette is _forced_, meaning it cannot be overridden by web pages. FF HCM and OS HCM do not directly change the way a color palette is chosen, but they _do_ change how the color palette is used.

We decide which set of colors to use in `PreferenceSheet::Load`. If `resistFingerprinting` is enabled, we use stand-in colors. These colors are pre-defined constants and are not dynamically fetched from the operating system. Check out `nsXPLookAndFeel::GetStandinForNativeColor` for more information, as well as the constants themselves.

If we aren't using stand-in colors, we'll check `browser.display.use_system_colors`, which is set from the "Use system colors" checkbox in the colors dialog. If that pref is true, we'll use system colors to style web content and Firefox chrome.

System colors are colors queried from the operating system. They help Firefox adapt to OS-level changes that aren't strictly HCM (ie. light/dark themeing). Because these colors are OS-dependent, a user operating Firefox on a Windows machine with system colors enabled will see Firefox differently than a user with system colors enabled on MacOS.

 So, how do we _get_ system colors? Our style system has a set of pre-defined `ColorID`'s in `ServoStyleConsts.h`, which are  mapped to platform-specific colors in  `widget/[cocoa | android | windows | gtk]/LookAndFeel.cpp`. Depending on the `ColorID` queried, we may do a dynamic fetch or simply return a constant. On MacOS, for example, `ColorID::TextForeground` and `ColorID::TextBackground` are hard-coded to return black and white respectively. `ColorID::Highlight`, on the other hand, queries the OS for `NSColor.selectedTextBackgroundColor`, which is set based on the accent color a user has selected in System Preferences.
 > Note: The colors we fetch here are theme-relative. If a user has set their OS to a dark theme, we'll fetch colors from that palette, and likewise for a light theme. Windows HCM, though not strictly a "theme", overrides the colors stored for Windows' light theme, leading to [some confusing code, like this](https://searchfox.org/mozilla-central/rev/b462b11e71b500e084f51e61fbd9e19ea0122c78/layout/style/PreferenceSheet.cpp#202-210).

 Lastly, if we are _not_ using system colors AND we are _not_ styling Firefox chrome AND we are _not_ `resistFingerprinting`, we'll use colors-dialog colors to style web content.

 By default, `browser.display.use_system_colors` is true on Windows and false elsewhere. This means users on Windows will _not_ see their selections in the colors dialog reflected automatically in Firefox. They'll need to uncheck "Use system colors" first.
 > Note: This is intentional. When Windows HCM is enabled, the system colors Windows exposes are pulled from the chosen HCM theme. With "Use system colors" checked, a Windows HCM user will see their HCM theme choices reflected in Firefox content automatically. Windows HCM is the most robust HCM offered among the operating systems we support, and so we cater to it here :)

Users on non-Windows platforms will see their selections in the colors dialog reflected automatically, but they will _not_ see OS changes until they check "Use system colors".

For a simplified flow chart of this decision tree, check out our [HCM Settings page](/https://firefox-source-docs.mozilla.org/accessible/HCMSettings.html)

## High Contrast Mode

### Operating System High Contrast Mode (OS HCM)

Operating System HCM (or OS HCM) describes a high contrast customisation that is enabled outside of Firefox, in the settings of a user's operating system. Each of our major desktop operating systems has an OS HCM variant:
- Windows: Settings > Accessibility > Increase Contrast > (select theme) > Apply
- MacOS: System Preferences > Accessibility > Display > Increase Contrast
- Linux: Settings > Themes > High Contrast

The presence of an OS HCM is stored in `IntID::UseAccessibilityTheme`.
