/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ChromeUtils = require("ChromeUtils");

ChromeUtils.defineModuleGetter(this, "VariablesView",
  "resource://devtools/client/shared/widgets/VariablesView.jsm");
ChromeUtils.defineModuleGetter(this, "VariablesViewController",
  "resource://devtools/client/shared/widgets/VariablesViewController.jsm");

/**
 * Fill the tooltip with a variables view, inspecting an object via its
 * corresponding object actor, as specified in the remote debugging protocol.
 *
 * @param {Tooltip} tooltip
 *        The tooltip to use
 * @param {object} objectActor
 *        The value grip for the object actor.
 * @param {object} viewOptions [optional]
 *        Options for the variables view visualization.
 * @param {object} controllerOptions [optional]
 *        Options for the variables view controller.
 * @param {object} relayEvents [optional]
 *        A collection of events to listen on the variables view widget.
 *        For example, { fetched: () => ... }
 * @param {array} extraButtons [optional]
 *        An array of extra buttons to add.  Each element of the array
 *        should be of the form {label, className, command}.
 * @param {Toolbox} toolbox [optional]
 *        Pass the instance of the current toolbox if you want the variables
 *        view widget to allow highlighting and selection of DOM nodes
 */

function setTooltipVariableContent(tooltip, objectActor,
                                   viewOptions = {}, controllerOptions = {},
                                   relayEvents = {}, extraButtons = [],
                                   toolbox = null) {
  const doc = tooltip.doc;
  const vbox = doc.createElement("vbox");
  vbox.className = "devtools-tooltip-variables-view-box";
  vbox.setAttribute("flex", "1");

  const innerbox = doc.createElement("vbox");
  innerbox.className = "devtools-tooltip-variables-view-innerbox";
  innerbox.setAttribute("flex", "1");
  vbox.appendChild(innerbox);

  for (const { label, className, command } of extraButtons) {
    const button = doc.createElement("button");
    button.className = className;
    button.setAttribute("label", label);
    button.addEventListener("command", command);
    vbox.appendChild(button);
  }

  const widget = new VariablesView(innerbox, viewOptions);

  // If a toolbox was provided, link it to the vview
  if (toolbox) {
    widget.toolbox = toolbox;
  }

  // Analyzing state history isn't useful with transient object inspectors.
  widget.commitHierarchy = () => {};

  for (const e in relayEvents) {
    widget.on(e, relayEvents[e]);
  }
  VariablesViewController.attach(widget, controllerOptions);

  // Some of the view options are allowed to change between uses.
  widget.searchPlaceholder = viewOptions.searchPlaceholder;
  widget.searchEnabled = viewOptions.searchEnabled;

  // Use the object actor's grip to display it as a variable in the widget.
  // The controller options are allowed to change between uses.
  widget.controller.setSingleVariable(
    { objectActor: objectActor }, controllerOptions);

  tooltip.content = vbox;
  tooltip.panel.setAttribute("clamped-dimensions", "");
}

exports.setTooltipVariableContent = setTooltipVariableContent;
