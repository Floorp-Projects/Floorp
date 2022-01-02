/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import { Base } from "content-src/components/Base/Base";
import { DetectUserSessionStart } from "content-src/lib/detect-user-session-start";
import { initStore } from "content-src/lib/init-store";
import { Provider } from "react-redux";
import React from "react";
import ReactDOM from "react-dom";
import { reducers } from "common/Reducers.jsm";

export const NewTab = ({ store }) => (
  <Provider store={store}>
    <Base />
  </Provider>
);

export function renderWithoutState() {
  const store = initStore(reducers);
  new DetectUserSessionStart(store).sendEventOrAddListener();

  // If this document has already gone into the background by the time we've reached
  // here, we can deprioritize requesting the initial state until the event loop
  // frees up. If, however, the visibility changes, we then send the request.
  let didRequest = false;
  let requestIdleCallbackId = 0;
  function doRequest() {
    if (!didRequest) {
      if (requestIdleCallbackId) {
        cancelIdleCallback(requestIdleCallbackId);
      }
      didRequest = true;
      store.dispatch(ac.AlsoToMain({ type: at.NEW_TAB_STATE_REQUEST }));
    }
  }

  if (document.hidden) {
    requestIdleCallbackId = requestIdleCallback(doRequest);
    addEventListener("visibilitychange", doRequest, { once: true });
  } else {
    doRequest();
  }

  ReactDOM.hydrate(<NewTab store={store} />, document.getElementById("root"));
}

export function renderCache(initialState) {
  const store = initStore(reducers, initialState);
  new DetectUserSessionStart(store).sendEventOrAddListener();

  ReactDOM.hydrate(<NewTab store={store} />, document.getElementById("root"));
}
