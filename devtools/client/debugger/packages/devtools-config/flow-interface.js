/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

declare module "devtools-config" {
  declare module.exports: {
    isDevelopment: () => boolean;
    getValue: (key: string) => any;
    isEnabled: () => boolean;
    isTesting: () => boolean;
    isFirefoxPanel: () => boolean;
    isFirefox: () => boolean;
    setConfig: (value: Object) => void;
    getConfig: () => Object;
  }
}
