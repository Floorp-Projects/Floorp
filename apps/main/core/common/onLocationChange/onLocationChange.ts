/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class OnLocationChange {
  private static instance: OnLocationChange;
  static getInstance(): OnLocationChange {
    if (!OnLocationChange.instance) {
      OnLocationChange.instance = new OnLocationChange();
    }
    return OnLocationChange.instance;
  }

  public webProgress: unknown;
  public request: unknown;
  public locationURI: unknown;
  public flags: unknown;
  public isSimulated: unknown;

  constructor() {
    this.webProgress = null;
    this.request = null;
    this.locationURI = null;
    this.flags = null;
    this.isSimulated = null;
    this.init();
  }

  private init() {
    window.gBrowser.addProgressListener(this.listener);
  }
}
