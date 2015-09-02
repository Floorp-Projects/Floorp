/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global PromisesController, promise */

"use strict";

/**
 * The main promise debugger UI.
 */
let PromisesPanel = {
  PANEL_INITIALIZED: "panel-initialized",

  initialize: Task.async(function*() {
    if (PromisesController.destroyed) {
      return null;
    }
    if (this.initialized) {
      return this.initialized.promise;
    }
    this.initialized = promise.defer();

    this.initialized.resolve();

    this.emit(this.PANEL_INITIALIZED);
  }),

  destroy: Task.async(function*() {
    if (!this.initialized) {
      return null;
    }
    if (this.destroyed) {
      return this.destroyed.promise;
    }
    this.destroyed = promise.defer();

    this.destroyed.resolve();
  }),
};

EventEmitter.decorate(PromisesPanel);
