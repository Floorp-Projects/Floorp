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
      "mozilla/avoid-removeChild": "off",
      "mozilla/consistent-if-bracing": "off",
      "mozilla/no-arbitrary-setTimeout": "off",
      "mozilla/no-compare-against-boolean-literals": "off",
      "mozilla/no-define-cc-etc": "off",
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
      "dom/l10n/tests/mochitest/document_l10n/non-system-principal/test.html",
      "dom/payments/test/test_basiccard.html",
      "dom/payments/test/test_bug1478740.html",
      "dom/payments/test/test_canMakePayment.html",
      "dom/payments/test/test_closePayment.html",
      "dom/payments/test/test_showPayment.html",
      "dom/tests/browser/browser_persist_cookies.js",
      "dom/tests/browser/browser_persist_mixed_content_image.js",
      "netwerk/test/unit/test_http2-proxy.js",
    ],
    "rules": {
      "no-async-promise-executor": "off",
    }
  }, {
    "files": [
      "browser/base/content/test/chrome/test_aboutCrashed.xhtml",
      "browser/base/content/test/chrome/test_aboutRestartRequired.xhtml",
      "browser/base/content/test/general/browser_tab_dragdrop2_frame1.xhtml",
      "browser/components/places/tests/chrome/test_0_bug510634.xhtml",
      "browser/components/places/tests/chrome/test_bug1163447_selectItems_through_shortcut.xhtml",
      "browser/components/places/tests/chrome/test_0_bug510634.xhtml",
      "browser/components/places/tests/chrome/test_bug1163447_selectItems_through_shortcut.xhtml",
      "browser/components/places/tests/chrome/test_bug549192.xhtml",
      "browser/components/places/tests/chrome/test_bug549491.xhtml",
      "browser/components/places/tests/chrome/test_selectItems_on_nested_tree.xhtml",
      "browser/components/places/tests/chrome/test_treeview_date.xhtml",
    ],
    "rules": {
      "mozilla/no-arbitrary-setTimeout": "off",
      "object-shorthand": "off",
      "no-undef": "off",
      "no-unused-vars": "off",
    }
  }, {
    "files": [
      "accessible/tests/mochitest/actions/test_keys_menu.xhtml",
      "accessible/tests/mochitest/elm/test_listbox.xhtml",
      "accessible/tests/mochitest/events/test_focus_autocomplete.xhtml",
      "accessible/tests/mochitest/events/test_focus_contextmenu.xhtml",
      "accessible/tests/mochitest/events/test_tree.xhtml",
      "accessible/tests/mochitest/hittest/test_zoom_tree.xhtml",
      "accessible/tests/mochitest/name/test_general.xhtml",
      "accessible/tests/mochitest/name/test_tree.xhtml",
      "accessible/tests/mochitest/selectable/test_listbox.xhtml",
      "accessible/tests/mochitest/states/test_expandable.xhtml",
      "accessible/tests/mochitest/tree/test_button.xhtml",
      "accessible/tests/mochitest/tree/test_tree.xhtml",
      "accessible/tests/mochitest/treeupdate/test_contextmenu.xhtml",
      "accessible/tests/mochitest/treeupdate/test_menu.xhtml",
    ],
    "rules": {
      "object-shorthand": "off",
      "mozilla/no-compare-against-boolean-literals": "off",
      "mozilla/use-cc-etc": "off",
      "consistent-return": "off",
      "no-redeclare": "off",
      "no-sequences": "off",
      "no-shadow": "off",
      "no-unused-vars": "off",
      "no-useless-call": "off",
    }
  }, {
    "files": [
      "accessible/**",
      "devtools/**",
      "dom/**",
      "docshell/**",
      "editor/libeditor/tests/**",
      "editor/spellchecker/tests/test_bug338427.html",
      "gfx/**",
      "image/test/browser/browser_image.js",
      "js/src/builtin/**",
      "layout/**",
      "mobile/android/**",
      "modules/**",
      "netwerk/**",
      "remote/**",
      "security/manager/**",
      "services/**",
      "storage/test/unit/test_vacuum.js",
      "taskcluster/docker/periodic-updates/scripts/**",
      "testing/**",
      "tools/**",
      "widget/tests/test_assign_event_data.html",
    ],
    "rules": {
      "mozilla/prefer-boolean-length-check": "off",
    }
  }]
};
