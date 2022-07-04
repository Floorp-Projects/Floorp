/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

module.exports = {
  attachRefToWebConsoleUI: () => {},
  emitForTests: () => {},
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
  },
  openLink: () => {},
  // eslint-disable-next-line react/display-name
  createElement: tagName => document.createElement(tagName),
};
