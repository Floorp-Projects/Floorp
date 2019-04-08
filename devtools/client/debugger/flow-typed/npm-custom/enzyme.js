/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Using this custom definition of the enzyme module avoids generating some
// flow errors that are unrelated to uses of enzyme itself...
// It would be nice to clean this up.
declare module "enzyme" {
  declare module.exports: any;
}
