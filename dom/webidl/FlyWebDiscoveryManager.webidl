/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary FlyWebDiscoveredService {
  DOMString serviceId = "";
  DOMString displayName = "";
  DOMString transport = "";
  DOMString serviceType = "";
  DOMString cert = "";
  DOMString path = "";
};

dictionary FlyWebPairedService {
  FlyWebDiscoveredService discoveredService;
  DOMString hostname = "";
  DOMString uiUrl = "";
};

callback interface FlyWebPairingCallback {
  void pairingSucceeded(optional FlyWebPairedService service);
  void pairingFailed(DOMString error);
};

callback interface FlyWebDiscoveryCallback {
  void onDiscoveredServicesChanged(sequence<FlyWebDiscoveredService> serviceList);
};

[ChromeOnly, ChromeConstructor, Exposed=(Window,System)]
interface FlyWebDiscoveryManager {
    sequence<FlyWebDiscoveredService> listServices();

    unsigned long startDiscovery(FlyWebDiscoveryCallback aCallback);
    void stopDiscovery(unsigned long aId);

    void pairWithService(DOMString serviceId, FlyWebPairingCallback callback);
};
