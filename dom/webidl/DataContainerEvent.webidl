/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface nsIVariant;

interface DataContainerEvent : Event {
  /**
   * Return the data associated with the given key.
   *
   * @param  key  the key
   * @return      the data associated with the key
   */
  nsIVariant? getData(DOMString? key);

  /**
   * Set the data for the given key.
   *
   * @param  key   the data key
   * @param  data  the data
   * @throws       NS_ERROR_UNEXPECTED if the method is called during event
   *               dispatch
   */
  [Throws]
  void setData(DOMString? key, any data);
};

