/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MESSAGE_TYPE_HASH as msg } from "common/ActorConstants.jsm";
import { actionCreators as ac } from "common/Actions.jsm";

export const ASRouterUtils = {
  addListener(listener) {
    if (global.ASRouterAddParentListener) {
      global.ASRouterAddParentListener(listener);
    }
  },
  removeListener(listener) {
    if (global.ASRouterRemoveParentListener) {
      global.ASRouterRemoveParentListener(listener);
    }
  },
  sendMessage(action) {
    if (global.ASRouterMessage) {
      return global.ASRouterMessage(action);
    }
    throw new Error(`Unexpected call:\n${JSON.stringify(action, null, 3)}`);
  },
  blockById(id, options) {
    return ASRouterUtils.sendMessage({
      type: msg.BLOCK_MESSAGE_BY_ID,
      data: { id, ...options },
    });
  },
  modifyMessageJson(content) {
    return ASRouterUtils.sendMessage({
      type: msg.MODIFY_MESSAGE_JSON,
      data: { content },
    });
  },
  executeAction(button_action) {
    return ASRouterUtils.sendMessage({
      type: msg.USER_ACTION,
      data: button_action,
    });
  },
  unblockById(id) {
    return ASRouterUtils.sendMessage({
      type: msg.UNBLOCK_MESSAGE_BY_ID,
      data: { id },
    });
  },
  blockBundle(bundle) {
    return ASRouterUtils.sendMessage({
      type: msg.BLOCK_BUNDLE,
      data: { bundle },
    });
  },
  unblockBundle(bundle) {
    return ASRouterUtils.sendMessage({
      type: msg.UNBLOCK_BUNDLE,
      data: { bundle },
    });
  },
  overrideMessage(id) {
    return ASRouterUtils.sendMessage({
      type: msg.OVERRIDE_MESSAGE,
      data: { id },
    });
  },
  sendTelemetry(ping) {
    return ASRouterUtils.sendMessage(ac.ASRouterUserEvent(ping));
  },
  getPreviewEndpoint() {
    if (
      global.document &&
      global.document.location &&
      global.document.location.href.includes("endpoint")
    ) {
      const params = new URLSearchParams(
        global.document.location.href.slice(
          global.document.location.href.indexOf("endpoint")
        )
      );
      try {
        const endpoint = new URL(params.get("endpoint"));
        return {
          url: endpoint.href,
          snippetId: params.get("snippetId"),
          theme: this.getPreviewTheme(),
          dir: this.getPreviewDir(),
        };
      } catch (e) {}
    }

    return null;
  },
  getPreviewTheme() {
    return new URLSearchParams(
      global.document.location.href.slice(
        global.document.location.href.indexOf("theme")
      )
    ).get("theme");
  },
  getPreviewDir() {
    return new URLSearchParams(
      global.document.location.href.slice(
        global.document.location.href.indexOf("dir")
      )
    ).get("dir");
  },
};
