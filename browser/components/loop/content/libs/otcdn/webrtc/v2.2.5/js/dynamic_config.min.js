/**
 * @license  OpenTok JavaScript Library v2.2.5
 * http://www.tokbox.com/
 *
 * Copyright (c) 2014 TokBox, Inc.
 * Released under the MIT license
 * http://opensource.org/licenses/MIT
 *
 * Date: May 22 07:14:18 2014
 */

!(function() {
  TB.Config.replaceWith({
    global: {
      exceptionLogging: {
        enabled: true,
        messageLimitPerPartner: 100
      },

      iceServers: {
        enabled: false
      },
    },

    partners: {
    }
  });
})(TB);
