/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// A dumping ground for random testing functions
[Pref="dom.expose_test_interfaces"]
interface TestFunctions {
  [Throws]
  static void throwUncatchableException();
};
