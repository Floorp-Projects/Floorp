/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */


// UPDATE IPDL::sPropertyNames IF YOU ADD METHODS OR ATTRIBUTES HERE.

[ChromeConstructor, ChromeOnly, Exposed=(Window, System, Worker), NeedResolve]
interface IPDL {
  void registerProtocol(DOMString protocolName, DOMString protocolURI, optional boolean eagerLoad = false);
  void registerTopLevelClass(object classConstructor);
  object getTopLevelInstance(optional unsigned long id);
  sequence<object> getTopLevelInstances();
};
