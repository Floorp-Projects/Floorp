/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ClientManagerService_h
#define _mozilla_dom_ClientManagerService_h

namespace mozilla {

namespace dom {

// Define a singleton service to manage client activity throughout the
// browser.  This service runs on the PBackground thread.  To interact
// it with it please use the ClientManager and ClientHandle classes.
class ClientManagerService final
{
  ClientManagerService();
  ~ClientManagerService();

public:
  static already_AddRefed<ClientManagerService>
  GetOrCreateInstance();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::ClientManagerService)
};

} // namespace dom
} // namespace mozilla

#endif // _mozilla_dom_ClientManagerService_h
