/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A stub object to hold an Error, Exception, or DOMException object to be
 * transferred via structured clone. The object will automatically be decoded
 * directly into the type of the error object that it wraps.
 *
 * This is a temporary workaround for lack of native Error and Exception
 * cloning support, and can be removed once bug 1556604 and bug 1561357 are
 * fixed.
 */
[ChromeOnly, Exposed=(Window,Worker)]
interface ClonedErrorHolder {
  [Throws]
  constructor(object aError);
};
