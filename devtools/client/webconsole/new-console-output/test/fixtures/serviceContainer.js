/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  attachRefToHud: () => {},
  emitNewMessage: () => {},
  hudProxyClient: {},
  onViewSourceInDebugger: () => {},
  onViewSourceInStyleEditor: () => {},
  onViewSourceInScratchpad: () => {},
  openNetworkPanel: () => {},
  sourceMapService: {
    subscribe: () => {},
  },
  openLink: () => {},
  // eslint-disable-next-line react/display-name
  createElement: tagName => document.createElement(tagName)
};
