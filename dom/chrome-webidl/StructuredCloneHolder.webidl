/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * A holder for structured-clonable data which can itself be cloned with
 * little overhead, and deserialized into an arbitrary global.
 */
[ChromeOnly, Exposed=*]
interface StructuredCloneHolder {
  /**
   * Serializes the given value to an opaque structured clone blob, and
   * returns the result.
   *
   * The serialization happens in the compartment of the given global or, if no
   * global is provided, the compartment of the data value.
   *
   * The name argument is added to the path of the object in
   * memory reports, to make it easier to determine the source of leaks. In
   * anonymized memory reports, the anonymized name is used instead. If
   * anonymizedName is null, name is used in anonymized reports as well.
   * Anonymized names should not contain any potentially private information,
   * such as web URLs or user-provided data.
   */
  [Throws]
  constructor(UTF8String name, UTF8String? anonymizedName,
              any data, optional object? global = null);

  /**
   * Deserializes the structured clone data in the scope of the given global,
   * and returns the result.
   *
   * If `keepData` is true, the structured clone data is preserved, and can be
   * deserialized repeatedly. Otherwise, it is immediately discarded.
   */
  [Throws]
  any deserialize(object global, optional boolean keepData = false);
};
