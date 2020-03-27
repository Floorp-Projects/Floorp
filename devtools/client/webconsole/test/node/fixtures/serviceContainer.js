/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  attachRefToWebConsoleUI: () => {},
  emitForTests: () => {},
  proxy: {
    client: {},
    releaseActor: actor => console.log("Release actor", actor),
  },
  onViewSourceInDebugger: () => {},
  onViewSourceInStyleEditor: () => {},
  openNetworkPanel: () => {},
  resendNetworkRequest: () => {},
  sourceMapService: {
    subscribe: () => {},
    originalPositionFor: () => {
      return new Promise(resolve => {
        resolve();
      });
    },
  },
  openLink: () => {},
  // eslint-disable-next-line react/display-name
  createElement: tagName => document.createElement(tagName),
};
