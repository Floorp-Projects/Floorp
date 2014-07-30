/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * /!\ FIXME: THIS IS A HORRID HACK which fakes both the mozL10n and webL10n
 * objects and makes them returning "fake string" for any requested string id.
 * @type {Object}
 */
document.webL10n = document.mozL10n = {
  get: function() {
    return "fake text";
  }
};
