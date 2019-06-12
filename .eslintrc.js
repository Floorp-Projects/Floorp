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

      // Not enabling the rules below for now pending prettier roll-out.
      "brace-style": "off",
      "comma-dangle": "off",
      "linebreak-style": "off",
      "no-tabs": "off",
      "no-mixed-spaces-and-tabs": "off",
      "no-multi-spaces": "off",
      "no-trailing-spaces": "off",
      "padded-blocks": "off",
      "quotes": "off",
      "semi": "off",
      "space-infix-ops": "off",
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

      // Not enabling the rules below for now pending prettier roll-out.
      "brace-style": "off",
      "comma-dangle": "off",
      "comma-spacing": "off",
      "key-spacing": "off",
      "keyword-spacing": "off",
      "no-extra-semi": "off",
      "no-tabs": "off",
      "no-mixed-spaces-and-tabs": "off",
      "no-multi-spaces": "off",
      "no-trailing-spaces": "off",
      "padded-blocks": "off",
      "quotes": "off",
      "semi": "off",
      "space-before-function-paren": "off",
      "space-infix-ops": "off",
    }
  }, {
    "files": [
      "netwerk/cookie/test/browser/**",
      "netwerk/test/browser/**",
      "netwerk/test/mochitests/**",
      "netwerk/test/unit*/**",
    ],
    "rules": {
      "object-shorthand": "off",
      "mozilla/consistent-if-bracing": "off",
      "mozilla/reject-importGlobalProperties": "off",
      "mozilla/no-arbitrary-setTimeout": "off",
      "mozilla/no-define-cc-etc": "off",
      "mozilla/no-useless-parameters": "off",
      "mozilla/no-useless-run-test": "off",
      "mozilla/use-chromeutils-generateqi": "off",
      "mozilla/use-chromeutils-import": "off",
      "mozilla/use-default-preference-values": "off",
      "mozilla/use-services": "off",
      "consistent-return": "off",
      "no-array-constructor": "off",
      "no-extra-boolean-cast": "off",
      "no-eval": "off",
      "no-else-return": "off",
      "no-global-assign": "off",
      "no-lonely-if": "off",
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

      // Not enabling the rules below for now pending prettier roll-out.
      "arrow-spacing": "off",
      "block-spacing": "off",
      "brace-style": "off",
      "comma-dangle": "off",
      "comma-spacing": "off",
      "comma-style": "off",
      "eol-last": "off",
      "func-call-spacing": "off",
      "generator-star-spacing": "off",
      "key-spacing": "off",
      "keyword-spacing": "off",
      "no-extra-semi": "off",
      "no-tabs": "off",
      "no-mixed-spaces-and-tabs": "off",
      "no-multi-spaces": "off",
      "no-trailing-spaces": "off",
      "no-whitespace-before-property": "off",
      "padded-blocks": "off",
      "quotes": "off",
      "rest-spread-spacing": "off",
      "semi": "off",
      "space-before-blocks": "off",
      "space-before-function-paren": "off",
      "space-infix-ops": "off",
      "space-unary-ops": "off",
      "spaced-comment": "off",
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
      "mozilla/no-useless-parameters": "off",
      "mozilla/no-useless-run-test": "off",
      "mozilla/use-chromeutils-generateqi": "off",
      "mozilla/use-chromeutils-import": "off",
      "mozilla/use-default-preference-values": "off",
      "mozilla/use-includes-instead-of-indexOf": "off",
      "mozilla/use-services": "off",
      "mozilla/use-ownerGlobal": "off",
      "complexity": "off",
      "consistent-return": "off",
      "dot-notation": "off",
      "no-array-constructor": "off",
      "no-caller": "off",
      "no-cond-assign": "off",
      "no-extra-boolean-cast": "off",
      "no-eval": "off",
      "no-else-return": "off",
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

      // Not enabling the rules below for now pending prettier roll-out.
      "arrow-spacing": "off",
      "block-spacing": "off",
      "brace-style": "off",
      "comma-dangle": "off",
      "comma-spacing": "off",
      "comma-style": "off",
      "eol-last": "off",
      "func-call-spacing": "off",
      "generator-star-spacing": "off",
      "linebreak-style": "off",
      "key-spacing": "off",
      "keyword-spacing": "off",
      "no-extra-semi": "off",
      "no-tabs": "off",
      "no-mixed-spaces-and-tabs": "off",
      "no-multi-spaces": "off",
      "no-trailing-spaces": "off",
      "no-unexpected-multiline": "off",
      "no-whitespace-before-property": "off",
      "padded-blocks": "off",
      "quotes": "off",
      "rest-spread-spacing": "off",
      "semi": "off",
      "space-before-blocks": "off",
      "space-before-function-paren": "off",
      "space-infix-ops": "off",
      "space-unary-ops": "off",
      "spaced-comment": "off",
    }
  }]
};
