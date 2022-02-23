[<img align="right" src="https://img.shields.io/amo/stars/gesturefy.svg">](https://addons.mozilla.org/firefox/addon/gesturefy/reviews/)
[<img align="right" src="https://img.shields.io/amo/users/gesturefy.svg">](https://addons.mozilla.org/firefox/addon/gesturefy/statistics)
[<img align="right" src="https://img.shields.io/github/release/robbendebiene/gesturefy.svg">](https://github.com/Robbendebiene/Gesturefy/releases)
[<img align="right" src="https://img.shields.io/github/license/robbendebiene/gesturefy.svg">](https://github.com/Robbendebiene/Gesturefy/blob/master/LICENSE)


# <sub><img src="https://github.com/Robbendebiene/Gesturefy/blob/master/src/resources/img/iconx48.png" height="38" width="38"></sub>Gesturefy


#### [<img align="right" src="https://addons.cdn.mozilla.net/static/img/addons-buttons/AMO-button_2.png">](https://addons.mozilla.org/firefox/addon/gesturefy/) Navigate, operate, and browse faster with mouse gestures! A customizable Firefox mouse gesture add-on with a variety of different commands.


***

Gesturefy is a pure mouse gesture extension, which means it's only suited for mice and not touchpads. "What's a mouse gesture?" you might ask yourself. Well, mouse gestures are like keyboard shortcuts, only for your mouse. Instead of pressing a bunch of keys, you simply move your mouse in a certain manner to execute commands and actions. Mouse gestures can be more natural and convenient than keyboard shortcuts and thus are also suitable for casual users. Wait, is that a mouse in your hand? Come and try it out :)

**Features:**

 - Mouse gestures (moving the mouse while holding the left, middle, or right button)
 - More than 80 different predefined commands
 - Provides special commands like popup, user script, multi purpose and cross add-on command
 - Customizable gesture trace and status information style
 - Rocker gestures (left-click while holding the right mouse button and vice versa)
 - Wheel gestures (scroll wheel while holding the left, middle, or right button)
 - Multilingual, thanks to volunteers on [Crowdin](https://crowdin.com/project/gesturefy)
 - Light, dark and highcontrast theme

**Limitations:**

 - Gesturefy does not work on Mozilla related pages like [addons.mozilla.org](https://addons.mozilla.org), internal pages like about:addons or other add-on option pages (e.g. moz-extension://*). This is because Firefox restricts add-ons from accessing these pages for security reasons.
 - The page must be partially loaded to perform gestures.
 - **MacOS Sierra:** Wheel gestures currently doesn't work (see this [bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1424893))


**If you want to support Gesturefy, please [visit this page](https://github.com/Robbendebiene/Gesturefy/wiki/FAQ#where-and-how-can-i-support-gesturefy)!**

**Permissions explained:**

 - Access your data for all websites: *This is a key permission, because the complete gesture functionality is injected in every webpage you visit (which means a part of the code is running in each tab). This is necessary, because with the new API there is no other way to track your mouse movement or draw anything on the screen. It's also needed to perform page specific commands like scroll down or up.*
 - Read and modify browser settings: *This is required to change the context menu behaviour for MacOS and Linux users to support the usage of the right mouse button.*
 - Display notifications: *This is used to show a notification on Gesturefy updates or to display certain error messages.*


***

**Release History:**

See the [releases page](https://github.com/Robbendebiene/Gesturefy/releases) for a history of releases and highlights for each release.

**License:**

This project is licensed under the terms of the [GNU General Public License v3.0](https://github.com/Robbendebiene/Gesturefy/blob/master/LICENSE).

Copy right 2022 blob