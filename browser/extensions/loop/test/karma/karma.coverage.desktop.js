/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env node */

module.exports = function(config) {
  "use strict";

  var baseConfig = require("./karma.conf.base.js")(config);

  // List of files / patterns to load in the browser.
  baseConfig.files = baseConfig.files.concat([
    "content/panels/vendor/l10n.js",
    "content/shared/vendor/react-0.13.3.js",
    "content/shared/vendor/classnames-2.2.0.js",
    "content/shared/vendor/lodash-3.9.3.js",
    "content/shared/vendor/backbone-1.2.1.js",
    "test/shared/vendor/*.js",
    "test/shared/loop_mocha_utils.js",
    "test/karma/head.js", // Stub out DOM event listener due to races.
    "content/shared/js/loopapi-client.js",
    "content/shared/js/utils.js",
    "content/shared/js/models.js",
    "content/shared/js/mixins.js",
    "content/shared/js/actions.js",
    "content/shared/js/otSdkDriver.js",
    "content/shared/js/validate.js",
    "content/shared/js/dispatcher.js",
    "content/shared/js/store.js",
    "content/shared/js/activeRoomStore.js",
    "content/shared/js/views.js",
    "content/shared/js/textChatStore.js",
    "content/shared/js/textChatView.js",
    "content/panels/js/feedbackViews.js",
    "content/panels/js/conversationAppStore.js",
    "content/panels/js/roomStore.js",
    "content/panels/js/roomViews.js",
    "content/panels/js/conversation.js",
    "test/desktop-local/*.js"
  ]);

  // List of files to exclude.
  baseConfig.exclude = baseConfig.exclude.concat([
    "test/desktop-local/panel_test.js"
  ]);

  // Preprocess matching files before serving them to the browser.
  // Available preprocessors: https://npmjs.org/browse/keyword/karma-preprocessor .
  baseConfig.preprocessors = {
    "content/panels/js/*.js": ["coverage"]
  };

  baseConfig.coverageReporter.dir = "test/coverage/desktop";

  config.set(baseConfig);
};
