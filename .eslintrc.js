"use strict";

const xpcshellTestConfig = require("eslint-plugin-mozilla/lib/configs/xpcshell-test.js");
const browserTestConfig = require("eslint-plugin-mozilla/lib/configs/browser-test.js");
const mochitestTestConfig = require("eslint-plugin-mozilla/lib/configs/mochitest-test.js");
const chromeTestConfig = require("eslint-plugin-mozilla/lib/configs/chrome-test.js");

/**
 * Some configurations have overrides, which can't be specified within overrides,
 * so we need to remove them.
 */
function removeOverrides(config) {
  config = {...config};
  delete config.overrides;
  return config;
}

const xpcshellTestPaths = [
  "**/test*/unit*/",
  "**/test*/xpcshell/",
];

const browserTestPaths = [
  "**/test*/**/browser/",
];

const mochitestTestPaths = [
  "**/test*/mochitest/",
];

const chromeTestPaths = [
  "**/test*/chrome/",
];

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
  "overrides": [{
      "files": [
        "*.html",
        "*.xhtml",
        "*.xul",
        "*.xml",
        "js/src/builtin/**/*.js",
        "js/src/shell/**/*.js"
      ],
      "rules": {
        // Curly brackets are required for all the tree via recommended.js,
        // however these files aren't auto-fixable at the moment.
        "curly": "off"
      },
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
    // TODO: Bug 1515949. Enable no-undef for gfx/
    "files": "gfx/layers/apz/test/mochitest/**",
    "rules": {
      "no-undef": "off",
    }
  }, {
    ...removeOverrides(xpcshellTestConfig),
    "files": xpcshellTestPaths.map(path => `${path}**`),
    "excludedFiles": "devtools/**"
  }, {
    // If it is an xpcshell head file, we turn off global unused variable checks, as it
    // would require searching the other test files to know if they are used or not.
    // This would be expensive and slow, and it isn't worth it for head files.
    // We could get developers to declare as exported, but that doesn't seem worth it.
    "files": xpcshellTestPaths.map(path => `${path}head*.js`),
    "rules": {
      "no-unused-vars": ["error", {
        "args": "none",
        "vars": "local",
      }],
    },
  }, {
    ...browserTestConfig,
    "files": browserTestPaths.map(path => `${path}**`),
    "excludedFiles": "devtools/**"
  }, {
    ...removeOverrides(mochitestTestConfig),
    "files": mochitestTestPaths.map(path => `${path}**`),
    "excludedFiles": [
      "devtools/**",
      "security/manager/ssl/tests/mochitest/browser/**",
      "testing/mochitest/**",
    ],
  }, {
    ...removeOverrides(chromeTestConfig),
    "files": chromeTestPaths.map(path => `${path}**`),
    "excludedFiles": [
      "devtools/**",
    ],
  }, {
    "env": {
      // Ideally we wouldn't be using the simpletest env here, but our uses of
      // js files mean we pick up everything from the global scope, which could
      // be any one of a number of html files. So we just allow the basics...
      "mozilla/simpletest": true,
    },
    "files": [
      ...mochitestTestPaths.map(path => `${path}/**/*.js`),
      ...chromeTestPaths.map(path => `${path}/**/*.js`),
    ],
  }, {
    "files": [
      "extensions/permissions/test/**",
      "extensions/spellcheck/**",
      "extensions/universalchardet/tests/**",
    ],
    "rules": {
      "mozilla/reject-importGlobalProperties": "off",
      "mozilla/use-default-preference-values": "off",
      "mozilla/use-services": "off",
      "no-array-constructor": "off",
      "no-undef": "off",
      "no-unused-vars": "off",
      "no-redeclare": "off",
      "no-global-assign": "off",
    }
  }, {
    "files": [
      "image/**",
    ],
    "rules": {
      "mozilla/consistent-if-bracing": "off",
      "mozilla/use-chromeutils-generateqi": "off",
      "mozilla/use-services": "off",
      "no-array-constructor": "off",
      "no-implied-eval": "off",
      "no-redeclare": "off",
      "no-self-assign": "off",
      "no-throw-literal": "off",
      "no-undef": "off",
      "no-unneeded-ternary": "off",
      "no-unused-vars": "off",
    }
  }, {
    "files": [
      "netwerk/cookie/test/browser/**",
      "netwerk/test/browser/**",
      "netwerk/test/mochitests/**",
      "netwerk/test/unit*/**",
    ],
    "rules": {
      "mozilla/consistent-if-bracing": "off",
      "mozilla/reject-importGlobalProperties": "off",
      "mozilla/no-arbitrary-setTimeout": "off",
      "mozilla/no-define-cc-etc": "off",
      "mozilla/use-default-preference-values": "off",
      "mozilla/use-services": "off",
      "consistent-return": "off",
      "no-array-constructor": "off",
      "no-eval": "off",
      "no-global-assign": "off",
      "no-nested-ternary": "off",
      "no-new-wrappers": "off",
      "no-redeclare": "off",
      "no-return-await": "off",
      "no-sequences": "off",
      "no-shadow": "off",
      "no-throw-literal": "off",
      "no-undef": "off",
      "no-unreachable": "off",
      "no-unused-vars": "off",
      "no-useless-return": "off",
    }
  }, {
    "files": [
      "layout/**",
    ],
    "rules": {
      "object-shorthand": "off",
      "mozilla/avoid-removeChild": "off",
      "mozilla/consistent-if-bracing": "off",
      "mozilla/reject-importGlobalProperties": "off",
      "mozilla/no-arbitrary-setTimeout": "off",
      "mozilla/no-define-cc-etc": "off",
      "mozilla/use-chromeutils-generateqi": "off",
      "mozilla/use-default-preference-values": "off",
      "mozilla/use-includes-instead-of-indexOf": "off",
      "mozilla/use-services": "off",
      "mozilla/use-ownerGlobal": "off",
      "complexity": "off",
      "consistent-return": "off",
      "no-array-constructor": "off",
      "no-caller": "off",
      "no-cond-assign": "off",
      "no-extra-boolean-cast": "off",
      "no-eval": "off",
      "no-func-assign": "off",
      "no-global-assign": "off",
      "no-implied-eval": "off",
      "no-lonely-if": "off",
      "no-nested-ternary": "off",
      "no-new-wrappers": "off",
      "no-redeclare": "off",
      "no-restricted-globals": "off",
      "no-return-await": "off",
      "no-sequences": "off",
      "no-throw-literal": "off",
      "no-useless-concat": "off",
      "no-undef": "off",
      "no-unreachable": "off",
      "no-unsanitized/method": "off",
      "no-unsanitized/property": "off",
      "no-unsafe-negation": "off",
      "no-unused-vars": "off",
      "no-useless-return": "off",
    }
  }, {
    "files": [
      "dom/animation/**",
      "dom/base/test/*.*",
      "dom/base/test/unit/test_serializers_entities*.js",
      "dom/base/test/unit_ipc/**",
      "dom/base/test/jsmodules/**",
      "dom/base/*.*",
      "dom/canvas/**",
      "dom/encoding/**",
      "dom/events/**",
      "dom/fetch/**",
      "dom/file/**",
      "dom/html/**",
      "dom/jsurl/**",
      "dom/media/tests/**",
      "dom/media/webaudio/**",
      "dom/media/webspeech/**",
      "dom/messagechannel/**",
      "dom/midi/**",
      "dom/network/**",
      "dom/payments/**",
      "dom/performance/**",
      "dom/permission/**",
      "dom/quota/**",
      "dom/security/test/cors/**",
      "dom/security/test/csp/**",
      "dom/security/test/general/**",
      "dom/security/test/mixedcontentblocker/**",
      "dom/security/test/sri/**",
      "dom/serviceworkers/**",
      "dom/smil/**",
      "dom/tests/mochitest/**",
      "dom/u2f/**",
      "dom/vr/**",
      "dom/webauthn/**",
      "dom/webgpu/**",
      "dom/websocket/**",
      "dom/workers/**",
      "dom/worklet/**",
      "dom/xbl/**",
      "dom/xml/**",
      "dom/xslt/**",
      "dom/xul/**",
    ],
    "rules": {
      "consistent-return": "off",
      "dot-notation": "off",
      "object-shorthand": "off",
      "mozilla/avoid-removeChild": "off",
      "mozilla/consistent-if-bracing": "off",
      "mozilla/no-arbitrary-setTimeout": "off",
      "mozilla/no-compare-against-boolean-literals": "off",
      "mozilla/no-define-cc-etc": "off",
      "mozilla/no-useless-parameters": "off",
      "mozilla/no-useless-run-test": "off",
      "mozilla/reject-importGlobalProperties": "off",
      "mozilla/use-cc-etc": "off",
      "mozilla/use-chromeutils-generateqi": "off",
      "mozilla/use-chromeutils-import": "off",
      "mozilla/use-includes-instead-of-indexOf": "off",
      "mozilla/use-ownerGlobal": "off",
      "mozilla/use-services": "off",
      "no-array-constructor": "off",
      "no-caller": "off",
      "no-cond-assign": "off",
      "no-control-regex": "off",
      "no-debugger": "off",
      "no-else-return": "off",
      "no-empty": "off",
      "no-eval": "off",
      "no-extra-boolean-cast": "off",
      "no-func-assign": "off",
      "no-global-assign": "off",
      "no-implied-eval": "off",
      "no-lone-blocks": "off",
      "no-lonely-if": "off",
      "no-nested-ternary": "off",
      "no-new-object": "off",
      "no-new-wrappers": "off",
      "no-octal": "off",
      "no-redeclare": "off",
      "no-return-await": "off",
      "no-restricted-globals": "off",
      "no-self-assign": "off",
      "no-self-compare": "off",
      "no-sequences": "off",
      "no-shadow": "off",
      "no-shadow-restricted-names": "off",
      "no-sparse-arrays": "off",
      "no-throw-literal": "off",
      "no-unneeded-ternary": "off",
      "no-unreachable": "off",
      "no-unsanitized/method": "off",
      "no-unsanitized/property": "off",
      "no-undef": "off",
      "no-unused-vars": "off",
      "no-useless-call": "off",
      "no-useless-concat": "off",
      "no-useless-return": "off",
      "no-with": "off",
    }
  }, {
    "files": [
      "browser/components/extensions/ExtensionControlledPopup.jsm",
      "browser/components/extensions/test/browser/browser_ext_devtools_network.js",
      "browser/components/extensions/test/browser/browser_ext_tabs_zoom.js",
      "browser/components/places/tests/browser/browser_bookmarksProperties.js",
      "browser/components/preferences/in-content/tests/browser_extension_controlled.js",
      "browser/extensions/formautofill/FormAutofillParent.jsm",
      "browser/tools/mozscreenshots/head.js",
      "devtools/client/aboutdebugging/test/browser/helper-addons.js",
      "devtools/client/inspector/animation/animation.js",
      "devtools/client/inspector/changes/ChangesView.js",
      "devtools/client/inspector/markup/test/helper_screenshot_node.js",
      "devtools/client/performance/modules/widgets/graphs.js",
      "devtools/client/scratchpad/scratchpad.js",
      "devtools/client/webconsole/webconsole-wrapper.js",
      "devtools/server/tests/unit/test_breakpoint-17.js",
      "devtools/shared/adb/adb-process.js",
      "devtools/shared/fronts/webconsole.js",
      "dom/l10n/tests/mochitest/document_l10n/non-system-principal/test.html",
      "dom/payments/test/test_basiccard.html",
      "dom/payments/test/test_bug1478740.html",
      "dom/payments/test/test_canMakePayment.html",
      "dom/payments/test/test_closePayment.html",
      "dom/payments/test/test_showPayment.html",
      "dom/tests/browser/browser_persist_cookies.js",
      "dom/tests/browser/browser_persist_mixed_content_image.js",
      "netwerk/test/unit/test_http2-proxy.js",
      "toolkit/components/contentprefs/ContentPrefService2.jsm",
      "toolkit/components/extensions/ExtensionShortcuts.jsm",
      "toolkit/components/extensions/ExtensionTestCommon.jsm",
      "toolkit/components/extensions/test/browser/browser_ext_themes_dynamic_getCurrent.js",
      "toolkit/components/extensions/test/browser/browser_ext_themes_warnings.js",
      "toolkit/components/passwordmgr/test/browser/browser_autocomplete_footer.js",
      "toolkit/components/remotebrowserutils/tests/browser/browser_httpResponseProcessSelection.js",
      "toolkit/components/satchel/FormHistory.jsm",
      "toolkit/content/tests/browser/browser_findbar.js",
      "toolkit/modules/NewTabUtils.jsm",
      "toolkit/mozapps/extensions/test/browser/browser_CTP_plugins.js",
      "toolkit/mozapps/extensions/test/browser/head.js",
    ],
    "rules": {
      "no-async-promise-executor": "off",
    }
  }]
};
