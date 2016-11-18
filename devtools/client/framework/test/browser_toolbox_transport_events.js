/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { on, off } = require("sdk/event/core");
const { DebuggerClient } = require("devtools/shared/client/main");

function test() {
  gDevTools.on("toolbox-created", onToolboxCreated);
  on(DebuggerClient, "connect", onDebuggerClientConnect);

  addTab("about:blank").then(function () {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "webconsole").then(testResults);
  });
}

function testResults(toolbox) {
  testPackets(sent1, received1);
  testPackets(sent2, received2);

  cleanUp(toolbox);
}

function cleanUp(toolbox) {
  gDevTools.off("toolbox-created", onToolboxCreated);
  off(DebuggerClient, "connect", onDebuggerClientConnect);

  toolbox.destroy().then(function () {
    setTimeout(() => {
      gBrowser.removeCurrentTab();
      executeSoon(function () {
        finish();
      });
    }, 1000);
  });
}

function testPackets(sent, received) {
  ok(sent.length > 0, "There must be at least one sent packet");
  ok(received.length > 0, "There must be at leaset one received packet");

  if (!sent.length || received.length) {
    return;
  }

  let sentPacket = sent[0];
  let receivedPacket = received[0];

  is(receivedPacket.from, "root",
    "The first received packet is from the root");
  is(receivedPacket.applicationType, "browser",
    "The first received packet has browser type");
  is(sentPacket.type, "listTabs",
    "The first sent packet is for list of tabs");
}

// Listen to the transport object that is associated with the
// default Toolbox debugger client
var sent1 = [];
var received1 = [];

function send1(eventId, packet) {
  sent1.push(packet);
}

function onPacket1(eventId, packet) {
  received1.push(packet);
}

function onToolboxCreated(eventId, toolbox) {
  toolbox.target.makeRemote();
  let client = toolbox.target.client;
  let transport = client._transport;

  transport.on("send", send1);
  transport.on("packet", onPacket1);

  client.addOneTimeListener("closed", event => {
    transport.off("send", send1);
    transport.off("packet", onPacket1);
  });
}

// Listen to all debugger client object protocols.
var sent2 = [];
var received2 = [];

function send2(eventId, packet) {
  sent2.push(packet);
}

function onPacket2(eventId, packet) {
  received2.push(packet);
}

function onDebuggerClientConnect(client) {
  let transport = client._transport;

  transport.on("send", send2);
  transport.on("packet", onPacket2);

  client.addOneTimeListener("closed", event => {
    transport.off("send", send2);
    transport.off("packet", onPacket2);
  });
}
