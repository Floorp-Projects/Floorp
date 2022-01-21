/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const xpcshellTestConfig = require("eslint-plugin-mozilla/lib/configs/xpcshell-test.js");
const browserTestConfig = require("eslint-plugin-mozilla/lib/configs/browser-test.js");
const mochitestTestConfig = require("eslint-plugin-mozilla/lib/configs/mochitest-test.js");
const chromeTestConfig = require("eslint-plugin-mozilla/lib/configs/chrome-test.js");
const fs = require("fs");
const path = require("path");

/**
 * Some configurations have overrides, which can't be specified within overrides,
 * so we need to remove them.
 */
function removeOverrides(config) {
  config = { ...config };
  delete config.overrides;
  return config;
}

const xpcshellTestPaths = [
  "**/test*/unit*/**/",
  "**/test*/*/unit*/",
  "**/test*/xpcshell/**/",
];

const browserTestPaths = ["**/test*/**/browser*/"];

const mochitestTestPaths = [
  // Note: we do not want to match testing/mochitest as that would apply
  // too many globals for that directory.
  "**/test/mochitest/",
  "**/tests/mochitest/",
  "**/test/mochitests/",
  "testing/mochitest/tests/SimpleTest/",
  "testing/mochitest/tests/Harness_sanity/",
];

const chromeTestPaths = ["**/test*/chrome/"];

const ignorePatterns = [
  ...fs
    .readFileSync(
      path.join(__dirname, "tools", "rewriting", "ThirdPartyPaths.txt")
    )
    .toString("utf-8")
    .split("\n"),
  ...fs
    .readFileSync(
      path.join(__dirname, "devtools", "client", "debugger", ".eslintignore")
    )
    .toString("utf-8")
    .split("\n")
    .filter(p => p && !p.startsWith("#"))
    .map(p => `devtools/client/debugger/${p}`),
];

module.exports = {
  parser: "@babel/eslint-parser",
  parserOptions: {
    sourceType: "script",
    babelOptions: {
      configFile: path.join(__dirname, ".babel-eslint.rc.js"),
    },
  },
  ignorePatterns,
  // Ignore eslint configurations in parent directories.
  root: true,
  // New rules and configurations should generally be added in
  // tools/lint/eslint/eslint-plugin-mozilla/lib/configs/recommended.js to
  // allow external repositories that use the plugin to pick them up as well.
  extends: ["plugin:mozilla/recommended"],
  plugins: ["mozilla"],
  overrides: [
    {
      // All .eslintrc.js files are in the node environment, so turn that
      // on here.
      // https://github.com/eslint/eslint/issues/13008
      files: [".eslintrc.js"],
      env: {
        node: true,
        browser: false,
      },
    },
    {
      files: "*.sjs",
      rules: {
        complexity: "warn",
        "no-undef": "warn",
        "no-empty": "warn",
        "no-shadow": "warn",
        "no-redeclare": "warn",
        "no-unused-vars": "warn",
        "no-fallthrough": "warn",
        "no-control-regex": "warn",
        "no-throw-literal": "warn",
        "no-useless-concat": "warn",
        "consistent-return": "warn",
        "mozilla/use-cc-etc": "warn",
        "mozilla/use-services": "warn",
        "mozilla/use-includes-instead-of-indexOf": "warn",
        "mozilla/no-compare-against-boolean-literals": "warn",
      },
    },
    {
      files: [
        "*.html",
        "*.xhtml",
        "*.xul",
        "*.xml",
        "js/src/builtin/**/*.js",
        "js/src/shell/**/*.js",
      ],
      rules: {
        // Curly brackets are required for all the tree via recommended.js,
        // however these files aren't auto-fixable at the moment.
        curly: "off",
      },
    },
    {
      // TODO: Bug 1515949. Enable no-undef for gfx/
      files: "gfx/layers/apz/test/mochitest/**",
      rules: {
        "no-undef": "off",
      },
    },
    {
      ...removeOverrides(xpcshellTestConfig),
      files: xpcshellTestPaths.map(path => `${path}**`),
      excludedFiles: "devtools/**",
    },
    {
      // If it is an xpcshell head file, we turn off global unused variable checks, as it
      // would require searching the other test files to know if they are used or not.
      // This would be expensive and slow, and it isn't worth it for head files.
      // We could get developers to declare as exported, but that doesn't seem worth it.
      files: xpcshellTestPaths.map(path => `${path}head*.js`),
      rules: {
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "local",
          },
        ],
      },
    },
    {
      // This section enables warning of no-unused-vars globally for all test*.js
      // files in xpcshell test paths.
      // These are turned into errors with selected exclusions in the next
      // section.
      // Bug 1612907: This section should go away once the exclusions are removed
      // from the following section.
      files: xpcshellTestPaths.map(path => `${path}test*.js`),
      rules: {
        // No declaring variables that are never used
        "no-unused-vars": [
          "warn",
          {
            args: "none",
            vars: "all",
          },
        ],
      },
    },
    {
      // This section makes global issues with no-unused-vars be reported as
      // errors - except for the excluded lists which are being fixed in the
      // dependencies of bug 1612907.
      files: xpcshellTestPaths.map(path => `${path}test*.js`),
      excludedFiles: [
        // These are suitable as good first bugs, take one or two related lines
        // per bug.
        "caps/tests/unit/test_origin.js",
        "caps/tests/unit/test_site_origin.js",
        "chrome/test/unit/test_no_remote_registration.js",
        "extensions/permissions/**",
        "image/test/unit/**",
        "intl/uconv/tests/unit/test_bug317216.js",
        "intl/uconv/tests/unit/test_bug340714.js",
        "modules/libjar/test/unit/test_empty_jar_telemetry.js",
        "modules/libjar/zipwriter/test/unit/test_alignment.js",
        "modules/libjar/zipwriter/test/unit/test_bug419769_2.js",
        "modules/libjar/zipwriter/test/unit/test_storedata.js",
        "modules/libjar/zipwriter/test/unit/test_zippermissions.js",
        "modules/libpref/test/unit/test_changeType.js",
        "modules/libpref/test/unit/test_dirtyPrefs.js",
        "toolkit/crashreporter/test/unit/test_crash_AsyncShutdown.js",
        "toolkit/mozapps/update/tests/unit_aus_update/testConstants.js",
        "xpcom/tests/unit/test_hidden_files.js",
        "xpcom/tests/unit/test_localfile.js",

        // These are more complicated bugs which may require some in-depth
        // investigation or different solutions. They are also likely to be
        // a reasonable size.
        "browser/components/**",
        "browser/modules/**",
        "dom/**",
        "netwerk/**",
        "security/manager/ssl/tests/unit/**",
        "services/**",
        "testing/xpcshell/**",
        "toolkit/components/**",
        "toolkit/modules/**",
      ],
      rules: {
        // No declaring variables that are never used
        "no-unused-vars": [
          "error",
          {
            args: "none",
            vars: "all",
          },
        ],
      },
    },
    {
      ...browserTestConfig,
      files: browserTestPaths.map(path => `${path}**`),
    },
    {
      ...removeOverrides(mochitestTestConfig),
      files: mochitestTestPaths.map(path => `${path}**`),
      excludedFiles: ["security/manager/ssl/tests/mochitest/browser/**"],
    },
    {
      ...removeOverrides(chromeTestConfig),
      files: chromeTestPaths.map(path => `${path}**`),
    },
    {
      env: {
        // Ideally we wouldn't be using the simpletest env here, but our uses of
        // js files mean we pick up everything from the global scope, which could
        // be any one of a number of html files. So we just allow the basics...
        "mozilla/simpletest": true,
      },
      files: [
        ...mochitestTestPaths.map(path => `${path}/**/*.js`),
        ...chromeTestPaths.map(path => `${path}/**/*.js`),
      ],
    },
    {
      files: [
        "netwerk/cookie/test/browser/**",
        "netwerk/test/browser/**",
        "netwerk/test/mochitests/**",
        "netwerk/test/unit*/**",
      ],
      rules: {
        "mozilla/no-arbitrary-setTimeout": "off",
        "mozilla/no-define-cc-etc": "off",
        "consistent-return": "off",
        "no-eval": "off",
        "no-global-assign": "off",
        "no-nested-ternary": "off",
        "no-redeclare": "off",
        "no-shadow": "off",
        "no-throw-literal": "off",
      },
    },
    {
      files: ["layout/**"],
      rules: {
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
        complexity: "off",
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
      },
    },
    {
      files: [
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
        "dom/media/webrtc/tests/**",
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
        "dom/security/test/mixedcontentblocker/**",
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
        "dom/xml/**",
        "dom/xslt/**",
        "dom/xul/**",
        "dom/ipc/test.xhtml",
      ],
      rules: {
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
      },
    },
    {
      files: [
        "testing/mochitest/browser-harness.xhtml",
        "testing/mochitest/chrome/test_chromeGetTestFile.xhtml",
        "testing/mochitest/chrome/test_sanityEventUtils.xhtml",
        "testing/mochitest/chrome/test_sanityException.xhtml",
        "testing/mochitest/chrome/test_sanityException2.xhtml",
        "testing/mochitest/harness.xhtml",
      ],
      rules: {
        "dot-notation": "off",
        "object-shorthand": "off",
        "mozilla/use-services": "off",
        "mozilla/no-compare-against-boolean-literals": "off",
        "mozilla/no-useless-parameters": "off",
        "mozilla/no-useless-removeEventListener": "off",
        "mozilla/use-cc-etc": "off",
        "consistent-return": "off",
        "no-fallthrough": "off",
        "no-nested-ternary": "off",
        "no-redeclare": "off",
        "no-sequences": "off",
        "no-shadow": "off",
        "no-throw-literal": "off",
        "no-undef": "off",
        "no-unsanitized/property": "off",
        "no-unused-vars": "off",
        "no-useless-call": "off",
      },
    },
    {
      files: [
        "dom/base/test/chrome/file_bug1139964.xhtml",
        "dom/base/test/chrome/file_bug549682.xhtml",
        "dom/base/test/chrome/file_bug616841.xhtml",
        "dom/base/test/chrome/file_bug990812-1.xhtml",
        "dom/base/test/chrome/file_bug990812-2.xhtml",
        "dom/base/test/chrome/file_bug990812-3.xhtml",
        "dom/base/test/chrome/file_bug990812-4.xhtml",
        "dom/base/test/chrome/file_bug990812-5.xhtml",
        "dom/base/test/chrome/file_bug990812.xhtml",
        "dom/base/test/chrome/test_bug1098074_throw_from_ReceiveMessage.xhtml",
        "dom/base/test/chrome/test_bug339494.xhtml",
        "dom/base/test/chrome/test_bug429785.xhtml",
        "dom/base/test/chrome/test_bug467123.xhtml",
        "dom/base/test/chrome/test_bug683852.xhtml",
        "dom/base/test/chrome/test_bug780529.xhtml",
        "dom/base/test/chrome/test_bug800386.xhtml",
        "dom/base/test/chrome/test_bug884693.xhtml",
        "dom/base/test/chrome/test_document-element-inserted.xhtml",
        "dom/base/test/chrome/test_domparsing.xhtml",
        "dom/base/test/chrome/title_window.xhtml",
        "dom/base/test/chrome/window_nsITextInputProcessor.xhtml",
        "dom/base/test/chrome/window_swapFrameLoaders.xhtml",
        "dom/base/test/test_domrequesthelper.xhtml",
        "dom/bindings/test/test_bug1123516_maplikesetlikechrome.xhtml",
        "dom/console/tests/test_jsm.xhtml",
        "dom/events/test/test_bug1412775.xhtml",
        "dom/events/test/test_bug336682_2.xhtml",
        "dom/events/test/test_bug415498.xhtml",
        "dom/events/test/test_bug602962.xhtml",
        "dom/events/test/test_bug617528.xhtml",
        "dom/events/test/test_bug679494.xhtml",
        "dom/indexedDB/test/test_globalObjects_chrome.xhtml",
        "dom/indexedDB/test/test_wrappedArray.xhtml",
        "dom/ipc/test.xhtml",
        "dom/ipc/tests/test_process_error.xhtml",
        "dom/notification/test/chrome/test_notification_system_principal.xhtml",
        "dom/security/test/general/test_bug1277803.xhtml",
        "dom/serviceworkers/test/test_serviceworkerinfo.xhtml",
        "dom/serviceworkers/test/test_serviceworkermanager.xhtml",
        "dom/system/tests/test_constants.xhtml",
        "dom/tests/mochitest/chrome/DOMWindowCreated_chrome.xhtml",
        "dom/tests/mochitest/chrome/MozDomFullscreen_chrome.xhtml",
        "dom/tests/mochitest/chrome/sizemode_attribute.xhtml",
        "dom/tests/mochitest/chrome/test_cyclecollector.xhtml",
        "dom/tests/mochitest/chrome/test_docshell_swap.xhtml",
        "dom/tests/mochitest/chrome/window_focus.xhtml",
        "dom/url/tests/test_bug883784.xhtml",
        "dom/workers/test/test_WorkerDebugger.xhtml",
        "dom/workers/test/test_WorkerDebugger_console.xhtml",
        "dom/workers/test/test_fileReadSlice.xhtml",
        "dom/workers/test/test_fileReaderSync.xhtml",
        "dom/workers/test/test_fileSlice.xhtml",
      ],
      rules: {
        "mozilla/no-useless-parameters": "off",
        "mozilla/no-useless-removeEventListener": "off",
        "mozilla/use-chromeutils-generateqi": "off",
        "mozilla/use-services": "off",
        complexity: "off",
        "no-array-constructor": "off",
        "no-caller": "off",
        "no-empty": "off",
        "no-eval": "off",
        "no-lone-blocks": "off",
        "no-redeclare": "off",
        "no-shadow": "off",
        "no-throw-literal": "off",
        "no-unsanitized/method": "off",
        "no-useless-return": "off",
        "object-shorthand": "off",
      },
    },
    {
      files: [
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
      rules: {
        "mozilla/prefer-boolean-length-check": "off",
      },
    },
    {
      // TODO: Bug 1609271 Fix all violations for ChromeUtils.import(..., null)
      files: [
        "browser/base/content/test/sync/browser_fxa_web_channel.js",
        "browser/components/customizableui/test/browser_1042100_default_placements_update.js",
        "browser/components/customizableui/test/browser_1096763_seen_widgets_post_reset.js",
        "browser/components/customizableui/test/browser_1161838_inserted_new_default_buttons.js",
        "browser/components/customizableui/test/browser_989338_saved_placements_not_resaved.js",
        "browser/components/customizableui/test/browser_currentset_post_reset.js",
        "browser/components/customizableui/test/browser_panel_keyboard_navigation.js",
        "browser/components/customizableui/test/browser_proton_toolbar_hide_toolbarbuttons.js",
        "browser/components/migration/tests/unit/test_Edge_db_migration.js",
        "browser/components/translation/test/unit/test_cld2.js",
        "browser/extensions/formautofill/test/unit/test_sync.js",
        "devtools/client/aboutdebugging/test/browser/browser_aboutdebugging_addons_debug_popup.js",
        "dom/ipc/tests/browser_memory_distribution_telemetry.js",
        "dom/push/test/xpcshell/head.js",
        "dom/push/test/xpcshell/test_broadcast_success.js",
        "dom/push/test/xpcshell/test_crypto.js",
        "services/common/tests/unit/head_helpers.js",
        "services/common/tests/unit/test_uptake_telemetry.js",
        "services/fxaccounts/tests/xpcshell/test_accounts.js",
        "services/fxaccounts/tests/xpcshell/test_accounts_device_registration.js",
        "services/fxaccounts/tests/xpcshell/test_loginmgr_storage.js",
        "services/fxaccounts/tests/xpcshell/test_oauth_token_storage.js",
        "services/fxaccounts/tests/xpcshell/test_oauth_tokens.js",
        "services/fxaccounts/tests/xpcshell/test_web_channel.js",
        "services/sync/modules-testing/utils.js",
        "services/sync/tests/unit/test_postqueue.js",
        "toolkit/components/cloudstorage/tests/unit/test_cloudstorage.js",
        "toolkit/components/crashes/tests/xpcshell/test_crash_manager.js",
        "toolkit/components/crashes/tests/xpcshell/test_crash_service.js",
        "toolkit/components/crashes/tests/xpcshell/test_crash_store.js",
        "toolkit/components/featuregates/test/unit/test_FeatureGate.js",
        "toolkit/components/normandy/test/browser/browser_actions_ShowHeartbeatAction.js",
        "toolkit/modules/subprocess/test/xpcshell/test_subprocess.js",
        "toolkit/modules/tests/xpcshell/test_GMPInstallManager.js",
        "toolkit/mozapps/extensions/internal/AddonTestUtils.jsm",
        "toolkit/mozapps/extensions/test/browser/browser_gmpProvider.js",
        "toolkit/mozapps/extensions/test/xpcshell/head_addons.js",
        "toolkit/mozapps/extensions/test/xpcshell/rs-blocklist/test_blocklist_clients.js",
        "toolkit/mozapps/extensions/test/xpcshell/rs-blocklist/test_blocklist_regexp_split.js",
        "toolkit/mozapps/extensions/test/xpcshell/rs-blocklist/test_blocklist_targetapp_filter.js",
        "toolkit/mozapps/extensions/test/xpcshell/rs-blocklist/test_blocklist_telemetry.js",
        "toolkit/mozapps/extensions/test/xpcshell/rs-blocklist/test_blocklistchange.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_gmpProvider.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_no_addons.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_permissions_prefs.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_signed_updatepref.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_signed_verify.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_webextension_events.js",
        "toolkit/mozapps/extensions/test/xpcshell/test_XPIStates.js",
      ],
      rules: {
        "mozilla/reject-chromeutils-import-params": "warn",
      },
    },
  ],
};
