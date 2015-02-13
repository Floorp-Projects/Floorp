/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* Copyright © 2014 Deutsche Telekom, Inc. */

[Pref="dom.secureelement.enabled",
 CheckPermissions="secureelement-manage",
 AvailableIn="CertifiedApps",
 JSImplementation="@mozilla.org/secureelement/manager;1",
 NavigatorProperty="seManager",
 NoInterfaceObject]
interface SEManager {

  /**
   * Retrieves all the readers available on the device.
   *
   * @return If success, the promise is resolved to  a sequence
   *        of SEReaders Otherwise, rejected with an error.
   */
  [Throws]
  Promise<sequence<SEReader>> getSEReaders();
};
