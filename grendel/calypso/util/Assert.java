/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package calypso.util;

/**
 * Simple assertion class that throws exceptions when an assertion
 * fails.
 */
public final class Assert {
  // Enabled flag. In theory if this is set to false the assertion
  // code can get compiled out when optimizing.
  static final boolean kEnabled = true;

  /**
   * Throw an exception if aCondition is false.
   */
  final public static void Assertion(boolean aCondition) {
    if (kEnabled) {
      if (!aCondition) {
        throw new AssertionError("assertion");
      }
    }
  }

  /**
   * Throw an exception if aCondition is false.
   */
  final public static void Assertion(boolean aCondition, String aMessage) {
    if (kEnabled) {
      if (!aCondition) {
        throw new AssertionError("assertion: " + aMessage);
      }
    }
  }

  /**
   * Throw an exception always. Used when the caller runs across some
   * code that should never be reached.
   */
  final public static void NotReached(String msg) {
    if (kEnabled) {
      throw new AssertionError("not reached: " + msg);
    }
  }

  /**
   * Throw an exception always. Used when the caller runs across some
   * unimplemented functionality in pre-release code.
   */
  final public static void NotYetImplemented(String msg) {
    if (kEnabled) {
      throw new AssertionError("not yet implemented: " + msg);
    }
  }

  /**
   * Throw an exception if aCondition is false.
   */
  final public static void PreCondition(boolean aCondition) {
    if (kEnabled) {
      if (!aCondition) {
        throw new AssertionError("precondition");
      }
    }
  }

  /**
   * Throw an exception if aCondition is false.
   */
  final public static void PreCondition(boolean aCondition, String aMessage) {
    if (kEnabled) {
      if (!aCondition) {
        throw new AssertionError("precondition: " + aMessage);
      }
    }
  }

  /**
   * Throw an exception if aCondition is false.
   */
  final public static void PostCondition(boolean aCondition) {
    if (kEnabled) {
      if (!aCondition) {
        throw new AssertionError("precondition");
      }
    }
  }

  /**
   * Throw an exception if aCondition is false.
   */
  final public static void PostCondition(boolean aCondition, String aMessage) {
    if (kEnabled) {
      if (!aCondition) {
        throw new AssertionError("precondition: " + aMessage);
      }
    }
  }
}
