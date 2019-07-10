/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// lifted from https://gist.github.com/kitze/23d82bb9eb0baabfd03a6a720b1d637f
export const ConditionalWrapper = ({ condition, wrap, children }) =>
  condition ? wrap(children) : children;
