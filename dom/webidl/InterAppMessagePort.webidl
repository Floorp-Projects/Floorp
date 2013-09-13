/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TODO Bug 907060 Per off-line discussion, after the MessagePort is done
// at Bug 643325, we will start to refactorize the common logic of both
// Inter-App Communication and Shared Worker. For now, we hope to design an
// MozInterAppMessagePort to meet the timeline, which still follows exactly
// the same interface and semantic as the MessagePort is. In the future,
// we can then align it back to MessagePort with backward compatibility.

[HeaderFile="mozilla/dom/InterAppComm.h",
 Func="mozilla::dom::InterAppComm::EnabledForScope",
 Constructor(DOMString messagePortID),
 JSImplementation="@mozilla.org/dom/inter-app-message-port;1"]
interface MozInterAppMessagePort : EventTarget {
  void postMessage(any message);

  void start();

  void close();

  attribute EventHandler onmessage;
};
