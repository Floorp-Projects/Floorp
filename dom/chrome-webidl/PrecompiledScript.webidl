/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * Represents a pre-compiled JS script, which can be repeatedly executed in
 * different globals without being re-parsed.
 */
[ChromeOnly, Exposed=Window]
interface PrecompiledScript {
  /**
   * Executes the script in the context of, and with the security principal
   * of, the given object's global. If compiled with a return value, returns
   * the value of the script's last expression. Otherwise returns undefined.
   */
  [Throws]
  any executeInGlobal(object global);

  /**
   * The URL that the script was loaded from.
   */
  [Pure]
  readonly attribute DOMString url;

  /**
   * True if the script was compiled with a return value, and will return the
   * value of its last expression when executed.
   */
  [Pure]
  readonly attribute boolean hasReturnValue;
};
