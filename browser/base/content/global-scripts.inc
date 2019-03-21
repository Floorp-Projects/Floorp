# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# JS files which are needed by browser.xul but no other top level windows to
# support MacOS specific features should be loaded directly from browser.xul
# rather than this file.

# If you update this list, you may need to add a mapping within the following
# file so that ESLint works correctly:
# tools/lint/eslint/eslint-plugin-mozilla/lib/environments/browser-window.js

<script type="text/javascript">
var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

Services.scriptloader.loadSubScript("chrome://browser/content/browser.js", this);
Services.scriptloader.loadSubScript("chrome://browser/content/browser-places.js", this);
Services.scriptloader.loadSubScript("chrome://global/content/globalOverlay.js", this);
Services.scriptloader.loadSubScript("chrome://global/content/editMenuOverlay.js", this);
Services.scriptloader.loadSubScript("chrome://browser/content/utilityOverlay.js", this);
#ifdef XP_MACOSX
Services.scriptloader.loadSubScript("chrome://global/content/macWindowMenu.js", this);
#endif

</script>
