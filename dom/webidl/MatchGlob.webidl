/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Represents a simple glob pattern matcher. Any occurrence of "*" in the glob
 * pattern matches any literal string of characters in the string being
 * compared. Additionally, if created with `allowQuestion = true`, any
 * occurrence of "?" in the glob matches any single literal character.
 */
[Constructor(DOMString glob, optional boolean allowQuestion = true),
 ChromeOnly, Exposed=(Window,System)]
interface MatchGlob {
  /**
   * Returns true if the string matches the glob.
   */
  boolean matches(DOMString string);

  /**
   * The glob string this MatchGlob represents.
   */
  [Constant]
  readonly attribute DOMString glob;
};
