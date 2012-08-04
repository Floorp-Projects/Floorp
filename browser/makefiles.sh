# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Deliberately excluded, since the braces cause some versions of sed to choke, 
# causing the build to fail (see bug 696498 comment 13):
# browser/app/profile/extensions/{972ce4c6-7e08-4474-a285-3208198ce6fd}/Makefile
add_makefiles "
browser/Makefile
browser/app/Makefile
browser/app/profile/extensions/Makefile
browser/base/Makefile
browser/components/Makefile
browser/components/about/Makefile
browser/components/build/Makefile
browser/components/certerror/Makefile
browser/components/dirprovider/Makefile
browser/components/downloads/Makefile
browser/components/downloads/src/Makefile
browser/components/feeds/Makefile
browser/components/feeds/public/Makefile
browser/components/feeds/src/Makefile
browser/components/migration/Makefile
browser/components/migration/public/Makefile
browser/components/migration/src/Makefile
browser/components/places/Makefile
browser/components/places/src/Makefile
browser/components/preferences/Makefile
browser/components/privatebrowsing/Makefile
browser/components/privatebrowsing/src/Makefile
browser/components/search/Makefile
browser/components/sessionstore/Makefile
browser/components/sessionstore/src/Makefile
browser/components/sidebar/Makefile
browser/components/shell/Makefile
browser/components/shell/public/Makefile
browser/components/shell/src/Makefile
browser/components/tabview/Makefile
browser/components/thumbnails/Makefile
browser/devtools/Makefile
browser/devtools/debugger/Makefile
browser/devtools/highlighter/Makefile
browser/devtools/scratchpad/Makefile
browser/devtools/shared/Makefile
browser/devtools/sourceeditor/Makefile
browser/devtools/styleeditor/Makefile
browser/devtools/styleinspector/Makefile
browser/devtools/tilt/Makefile
browser/devtools/webconsole/Makefile
browser/extensions/Makefile
browser/fuel/Makefile
browser/fuel/public/Makefile
browser/fuel/src/Makefile
browser/installer/Makefile
browser/locales/Makefile
browser/modules/Makefile
browser/themes/Makefile
$MOZ_BRANDING_DIRECTORY/Makefile
$MOZ_BRANDING_DIRECTORY/content/Makefile
$MOZ_BRANDING_DIRECTORY/locales/Makefile
toolkit/locales/Makefile
extensions/spellcheck/locales/Makefile
intl/locales/Makefile
netwerk/locales/Makefile
dom/locales/Makefile
security/manager/locales/Makefile
"

if [ "$MOZ_SAFE_BROWSING" ]; then
  add_makefiles "
    browser/components/safebrowsing/Makefile
  "
fi

if [ "$MAKENSISU" ]; then
  add_makefiles "
    browser/installer/windows/Makefile
  "
fi

if [ "$MOZ_WIDGET_TOOLKIT" = "gtk2" -o "$MOZ_WIDGET_TOOLKIT" = "qt" ]; then
  add_makefiles "
    browser/themes/gnomestripe/Makefile
    browser/themes/gnomestripe/communicator/Makefile
  "
elif [ "$MOZ_WIDGET_TOOLKIT" = "cocoa" ]; then
  add_makefiles "
    browser/themes/pinstripe/Makefile
    browser/themes/pinstripe/communicator/Makefile
  "
else
  add_makefiles "
    browser/themes/winstripe/Makefile
    browser/themes/winstripe/communicator/Makefile
  "
fi

if [ "$ENABLE_TESTS" ]; then
  add_makefiles "
    browser/base/content/test/Makefile
    browser/base/content/test/newtab/Makefile
    browser/components/certerror/test/Makefile
    browser/components/dirprovider/tests/Makefile
    browser/components/downloads/test/Makefile
    browser/components/downloads/test/browser/Makefile
    browser/components/preferences/tests/Makefile
    browser/components/search/test/Makefile
    browser/components/sessionstore/test/Makefile
    browser/components/shell/test/Makefile
    browser/components/feeds/test/Makefile
    browser/components/feeds/test/chrome/Makefile
    browser/components/migration/tests/Makefile
    browser/components/places/tests/Makefile
    browser/components/places/tests/chrome/Makefile
    browser/components/places/tests/browser/Makefile
    browser/components/privatebrowsing/test/Makefile
    browser/components/privatebrowsing/test/browser/Makefile
    browser/components/tabview/test/Makefile
    browser/components/test/Makefile
    browser/components/thumbnails/test/Makefile
    browser/devtools/debugger/test/Makefile
    browser/devtools/highlighter/test/Makefile
    browser/devtools/scratchpad/test/Makefile
    browser/devtools/shared/test/Makefile
    browser/devtools/sourceeditor/test/Makefile
    browser/devtools/styleeditor/test/Makefile
    browser/devtools/styleinspector/test/Makefile
    browser/devtools/tilt/test/Makefile
    browser/devtools/webconsole/test/Makefile
    browser/fuel/test/Makefile
    browser/modules/test/Makefile
  "
  if [ "$MOZ_SAFE_BROWSING" ]; then
    add_makefiles "
      browser/components/safebrowsing/content/test/Makefile
    "
  fi
fi
