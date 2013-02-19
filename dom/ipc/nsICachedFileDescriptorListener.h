/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ipc_nsICachedFileDescriptorListener_h
#define mozilla_dom_ipc_nsICachedFileDescriptorListener_h

#include "nsISupports.h"

#ifndef NS_NO_VTABLE
#define NS_NO_VTABLE
#endif

#define NS_ICACHEDFILEDESCRIPTORLISTENER_IID \
  {0x2cedaee0, 0x6ef2, 0x4f60, {0x9a, 0x6c, 0xdf, 0x4e, 0x4d, 0x65, 0x6a, 0xf7}}

class nsAString;

namespace mozilla {
namespace ipc {
class FileDescriptor;
}
}

class NS_NO_VTABLE nsICachedFileDescriptorListener : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICACHEDFILEDESCRIPTORLISTENER_IID)

  virtual void
  OnCachedFileDescriptor(const nsAString& aPath,
                         const mozilla::ipc::FileDescriptor& aFD) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICachedFileDescriptorListener,
                              NS_ICACHEDFILEDESCRIPTORLISTENER_IID)

#endif // mozilla_dom_ipc_nsICachedFileDescriptorListener_h
