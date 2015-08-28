/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const CC = Components.Constructor;

const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const createDispatcher = require('devtools/shared/create-dispatcher')({ log: true });
const waitUntilService = require('devtools/shared/fluxify/waitUntilService');
const services = {
  WAIT_UNTIL: waitUntilService.name
};

const Services = require("Services");
const { waitForTick, waitForTime } = require("devtools/toolkit/DevToolsUtils");

let loadSubScript = Cc[
  '@mozilla.org/moz/jssubscript-loader;1'
].getService(Ci.mozIJSSubScriptLoader).loadSubScript;

function getFileUrl(name, allowMissing=false) {
  let file = do_get_file(name, allowMissing);
  return Services.io.newFileURI(file).spec;
}
