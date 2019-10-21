/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// globals window, document

require("../reps/reps.css");

const React = require("react");
const ReactDOM = require("react-dom");

const { bootstrap, renderRoot } = require("devtools-launchpad");

const RepsConsole = require("./components/Console");
const { configureStore } = require("./store");

require("./launchpad.css");

function onConnect(connection) {
  if (!connection) {
    return;
  }

  const client = {
    clientCommands: {
      evaluate: input =>
        connection.tabConnection.tabTarget.activeConsole.evaluateJSAsync(input),
    },

    createObjectClient: function(grip) {
      return connection.tabConnection.threadFront.pauseGrip(grip);
    },
    createLongStringClient: function(grip) {
      return connection.tabConnection.tabTarget.activeConsole.longString(grip);
    },
    releaseActor: function(actor) {
      const debuggerClient = connection.tabConnection.debuggerClient;
      const objFront = debuggerClient.getFrontByID(actor);

      if (objFront) {
        return objFront.release();
      }

      // In case there's no object front, use the client's release method.
      return debuggerClient.release(actor).catch(() => {});
    },
  };

  const store = configureStore({
    makeThunkArgs: (args, state) => ({ ...args, client }),
    client,
  });
  renderRoot(React, ReactDOM, RepsConsole, store);
}

function onConnectionError(e) {
  const h1 = document.createElement("h1");
  h1.innerText = `An error occured during the connection: «${e.message}»`;
  console.warn("An error occured during the connection", e);
  renderRoot(React, ReactDOM, h1);
}

bootstrap(React, ReactDOM)
  .then(onConnect, onConnectionError)
  .catch(onConnectionError);
