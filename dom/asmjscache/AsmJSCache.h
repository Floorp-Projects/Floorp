/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_asmjscache_asmjscache_h
#define mozilla_dom_asmjscache_asmjscache_h

#include "ipc/IPCMessageUtils.h"
#include "js/TypeDecls.h"
#include "js/Vector.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

namespace quota {
class Client;
}

namespace asmjscache {

class PAsmJSCacheEntryChild;
class PAsmJSCacheEntryParent;

enum OpenMode
{
  eOpenForRead,
  eOpenForWrite,
  NUM_OPEN_MODES
};

// Implementation of AsmJSCacheOps, installed for the main JSRuntime by
// nsJSEnvironment.cpp and DOM Worker JSRuntimes in RuntimeService.cpp.
//
// The Open* functions cannot be called directly from AsmJSCacheOps: they take
// an nsIPrincipal as the first argument instead of a Handle<JSObject*>. The
// caller must map the object to an nsIPrincipal.
//
// These methods may be called off the main thread and guarantee not to
// access the given aPrincipal except on the main thread. In exchange, the
// caller must ensure the given principal is alive from when OpenEntryForX is
// called to when CloseEntryForX returns.

bool
OpenEntryForRead(nsIPrincipal* aPrincipal,
                 const jschar* aBegin,
                 const jschar* aLimit,
                 size_t* aSize,
                 const uint8_t** aMemory,
                 intptr_t *aHandle);
void
CloseEntryForRead(JS::Handle<JSObject*> aGlobal,
                  size_t aSize,
                  const uint8_t* aMemory,
                  intptr_t aHandle);
bool
OpenEntryForWrite(nsIPrincipal* aPrincipal,
                  const jschar* aBegin,
                  const jschar* aEnd,
                  size_t aSize,
                  uint8_t** aMemory,
                  intptr_t* aHandle);
void
CloseEntryForWrite(JS::Handle<JSObject*> aGlobal,
                   size_t aSize,
                   uint8_t* aMemory,
                   intptr_t aHandle);
bool
GetBuildId(js::Vector<char>* aBuildId);

// Called from QuotaManager.cpp:

quota::Client*
CreateClient();

// Called from ipc/ContentParent.cpp:

PAsmJSCacheEntryParent*
AllocEntryParent(OpenMode aOpenMode, uint32_t aSizeToWrite,
                 nsIPrincipal* aPrincipal);

void
DeallocEntryParent(PAsmJSCacheEntryParent* aActor);

// Called from ipc/ContentChild.cpp:

void
DeallocEntryChild(PAsmJSCacheEntryChild* aActor);

} // namespace asmjscache
} // namespace dom
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::asmjscache::OpenMode> :
  public EnumSerializer<mozilla::dom::asmjscache::OpenMode,
                        mozilla::dom::asmjscache::eOpenForRead,
                        mozilla::dom::asmjscache::NUM_OPEN_MODES>
{ };

} // namespace IPC

#endif  // mozilla_dom_asmjscache_asmjscache_h
