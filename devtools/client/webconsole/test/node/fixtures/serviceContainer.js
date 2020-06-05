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
  sourceMapURLService: {
    subscribeByURL: () => {
      return () => {};
    },
    subscribeByID: () => {
      return () => {};
    },
    subscribeByLocation: () => {
      return () => {};
    },
    originalPositionForURL: () => {
      return new Promise(resolve => {
        resolve();
      });
    },
    originalPositionForID: () => {
      return new Promise(resolve => {
        resolve();
      });
    },
  },
  openLink: () => {},
  // eslint-disable-next-line react/display-name
  createElement: tagName => document.createElement(tagName),
};
