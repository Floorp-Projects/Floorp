/*
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

(function contentScriptClosure() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const Cm = Components.manager;
  const Cu = Components.utils;
  const Cr = Components.results;

  // we need to use closure here -- we are running in the global context
  Cu.import('resource://gre/modules/Services.jsm');

  var isRemote = Services.appinfo.processType ===
                 Services.appinfo.PROCESS_TYPE_CONTENT;
  var isStarted = false;

  function startup() {
    if (isStarted) {
      return;
    }

    isStarted = true;
    Cu.import('resource://shumway/ShumwayBootstrapUtils.jsm');
    ShumwayBootstrapUtils.register();
  }

  function shutdown() {
    if (!isStarted) {
      return;
    }

    isStarted = false;
    ShumwayBootstrapUtils.unregister();
    Cu.unload('resource://shumway/ShumwayBootstrapUtils.jsm');
  }


  function updateSettings() {
    let mm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsISyncMessageSender);
    var results = mm.sendSyncMessage('Shumway:Chrome:isEnabled');
    var isEnabled = results.some(function (item) {
      return item;
    });

    if (isEnabled) {
      startup();
    } else {
      shutdown();
    }
  }

  if (isRemote) {
    addMessageListener('Shumway:Child:refreshSettings', updateSettings);
    updateSettings();

    addMessageListener('Shumway:Child:shutdown', function shutdownListener(e) {
      removeMessageListener('Shumway:Child:refreshSettings', updateSettings);
      removeMessageListener('Shumway:Child:shutdown', shutdownListener);

      shutdown();
    });
  }
})();
