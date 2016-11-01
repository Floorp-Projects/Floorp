/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {createNode} = require("devtools/client/animationinspector/utils");
const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/locale/animationinspector.properties");

// List of playback rate presets displayed in the timeline toolbar.
const PLAYBACK_RATES = [.1, .25, .5, 1, 2, 5, 10];

/**
 * UI component responsible for displaying a playback rate selector UI.
 * The rendering logic is such that a predefined list of rates is generated.
 * If *all* animations passed to render share the same rate, then that rate is
 * selected in the <select> element, otherwise, the empty value is selected.
 * If the rate that all animations share isn't part of the list of predefined
 * rates, than that rate is added to the list.
 */
function RateSelector() {
  this.onRateChanged = this.onRateChanged.bind(this);
  EventEmitter.decorate(this);
}

exports.RateSelector = RateSelector;

RateSelector.prototype = {
  init: function (containerEl) {
    this.selectEl = createNode({
      parent: containerEl,
      nodeType: "select",
      attributes: {
        "class": "devtools-button",
        "title": L10N.getStr("timeline.rateSelectorTooltip")
      }
    });

    this.selectEl.addEventListener("change", this.onRateChanged);
  },

  destroy: function () {
    this.selectEl.removeEventListener("change", this.onRateChanged);
    this.selectEl.remove();
    this.selectEl = null;
  },

  getAnimationsRates: function (animations) {
    return sortedUnique(animations.map(a => a.state.playbackRate));
  },

  getAllRates: function (animations) {
    let animationsRates = this.getAnimationsRates(animations);
    if (animationsRates.length > 1) {
      return PLAYBACK_RATES;
    }

    return sortedUnique(PLAYBACK_RATES.concat(animationsRates));
  },

  render: function (animations) {
    let allRates = this.getAnimationsRates(animations);
    let hasOneRate = allRates.length === 1;

    this.selectEl.innerHTML = "";

    if (!hasOneRate) {
      // When the animations displayed have mixed playback rates, we can't
      // select any of the predefined ones, instead, insert an empty rate.
      createNode({
        parent: this.selectEl,
        nodeType: "option",
        attributes: {value: "", selector: "true"},
        textContent: "-"
      });
    }
    for (let rate of this.getAllRates(animations)) {
      let option = createNode({
        parent: this.selectEl,
        nodeType: "option",
        attributes: {value: rate},
        textContent: L10N.getFormatStr("player.playbackRateLabel", rate)
      });

      // If there's only one rate and this is the option for it, select it.
      if (hasOneRate && rate === allRates[0]) {
        option.setAttribute("selected", "true");
      }
    }
  },

  onRateChanged: function () {
    let rate = parseFloat(this.selectEl.value);
    if (!isNaN(rate)) {
      this.emit("rate-changed", rate);
    }
  }
};

let sortedUnique = arr => [...new Set(arr)].sort((a, b) => a > b);
