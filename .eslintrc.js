"use strict";

module.exports = {
  // New rules and configurations should generally be added in
  // tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js to
  // allow external repositories that use the plugin to pick them up as well.
  "extends": [
    "plugin:mozilla/recommended"
  ],
  "plugins": [
    "mozilla"
  ],
  // The html plugin is enabled via a command line option on eslint. To avoid
  // bad interactions with the xml preprocessor in eslint-plugin-mozilla, we
  // turn off processing of the html plugin for .xml files.
  "settings": {
    "html/xml-extensions": [ ".xhtml" ]
  },

  "overrides": [{
    // eslint-plugin-html handles eol-last slightly different - it applies to
    // each set of script tags, so we turn it off here.
    "files": "**/*.*html",
    "rules": {
      "eol-last": "off",
    }
  }, {
    // These xbl bindings are assumed to be in the browser-window environment,
    // we would mark it in the files, but ESLint made this more difficult with
    // our xml processor, so we list them here. Bug 1397874 & co are working
    // towards removing these files completely.
    "files": [
      "browser/base/content/tabbrowser.xml",
      "browser/base/content/urlbarBindings.xml",
      "browser/components/search/content/search.xml",
      "browser/components/translation/translation-infobar.xml",
      "toolkit/components/prompts/content/tabprompts.xml"
    ],
    "env": {
      "mozilla/browser-window": true
    }
  }, {
    // TODO: Bug 1513639. Temporarily turn off reject-importGlobalProperties
    // due to other ESLint enabling happening in DOM.
    "files": "dom/**",
    "rules": {
      "mozilla/reject-importGlobalProperties": "off",
    }
  }, {
    // TODO: Bug 1515949. Enable no-undef for gfx/
    "files": "gfx/layers/apz/test/mochitest/**",
    "rules": {
      "no-undef": "off",
    }
  }, {
    // TODO: Bug 1246594. Empty this list once the rule has landed for all dirs
    "files": [
      "accessible/tests/mochitest/events.js",
      "browser/actors/ContextMenuChild.jsm",
      "browser/base/content/**",
      "browser/components/customizableui/**",
      "browser/components/enterprisepolicies/Policies.jsm",
      "browser/components/places/content/**",
      "browser/components/preferences/**",
      "browser/components/privatebrowsing/test/browser/browser_privatebrowsing_cache.js",
      "browser/components/urlbar/tests/browser/head-common.js",
      "browser/extensions/fxmonitor/privileged/FirefoxMonitor.jsm",
      "browser/modules/**",
      "browser/tools/mozscreenshots/mozscreenshots/extension/TestRunner.jsm",
      "docshell/test/chrome/docshell_helpers.js",
      "docshell/test/navigation/NavigationUtils.js",
      "dom/asmjscache/test/**",
      "dom/cache/test/mochitest/test_cache_tons_of_fd.html",
      "dom/crypto/test/**",
      "dom/indexedDB/test/**",
      "dom/localstorage/test/unit/test_migration.js",
      "dom/plugins/test/mochitest/head.js",
      "gfx/layers/apz/test/mochitest/**",
      "mobile/android/components/**",
      "mobile/android/modules/**",
      "modules/libmar/tests/unit/head_libmar.js",
      "netwerk/protocol/http/WellKnownOpportunisticUtils.jsm",
      "netwerk/test/httpserver/httpd.js",
      "netwerk/test/httpserver/test/**",
      "parser/htmlparser/tests/mochitest/parser_datreader.js",
      "testing/marionette/event.js",
      "testing/mochitest/**",
      "testing/modules/tests/xpcshell/test_assert.js",
      "testing/specialpowers/content/specialpowersAPI.js",
      "testing/talos/talos/**",
      "toolkit/components/aboutmemory/content/aboutMemory.js",
      "toolkit/components/captivedetect/test/unit/test_multiple_requests.js",
      "toolkit/components/cleardata/ServiceWorkerCleanUp.jsm",
      "toolkit/components/ctypes/**",
      "toolkit/components/downloads/**",
      "toolkit/components/places/tests/**",
      "toolkit/components/processsingleton/MainProcessSingleton.jsm",
      "toolkit/components/prompts/**",
      "toolkit/components/search/SearchService.jsm",
      "toolkit/components/telemetry/app/TelemetryEnvironment.jsm",
      "toolkit/content/**",
      "toolkit/modules/**",
      "toolkit/mozapps/**",
      "tools/leak-gauge/leak-gauge.html",
      "xpcom/tests/unit/test_iniParser.js",
    ],
    "rules": {
      "no-throw-literal": "off",
    }
  }]
};
