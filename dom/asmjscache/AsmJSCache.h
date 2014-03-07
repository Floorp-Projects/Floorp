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
#include "jsapi.h"

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

// Each origin stores a fixed size (kNumEntries) LRU cache of compiled asm.js
// modules. Each compiled asm.js module is stored in a separate file with one
// extra metadata file that stores the LRU cache and enough information for a
// client to pick which cached module's file to open.
struct Metadata
{
  static const unsigned kNumEntries = 16;
  static const unsigned kLastEntry = kNumEntries - 1;

  struct Entry
  {
    uint32_t mFastHash;
    uint32_t mNumChars;
    uint32_t mFullHash;
    unsigned mModuleIndex;

    void clear() {
      mFastHash = -1;
      mNumChars = -1;
      mFullHash = -1;
    }
  };

  Entry mEntries[kNumEntries];
};

// Parameters specific to opening a cache entry for writing
struct WriteParams
{
  int64_t mSize;
  int64_t mFastHash;
  int64_t mNumChars;
  int64_t mFullHash;
  bool mInstalled;

  WriteParams()
  : mSize(0),
    mFastHash(0),
    mNumChars(0),
    mFullHash(0),
    mInstalled(false)
  { }
};

// Parameters specific to opening a cache entry for reading
struct ReadParams
{
  const jschar* mBegin;
  const jschar* mLimit;

  ReadParams()
  : mBegin(nullptr),
    mLimit(nullptr)
  { }
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
                  bool aInstalled,
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
GetBuildId(JS::BuildIdCharVector* aBuildId);

// Called from QuotaManager.cpp:

quota::Client*
CreateClient();

// Called from ipc/ContentParent.cpp:

PAsmJSCacheEntryParent*
AllocEntryParent(OpenMode aOpenMode, WriteParams aWriteParams,
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

template <>
struct ParamTraits<mozilla::dom::asmjscache::Metadata>
{
  typedef mozilla::dom::asmjscache::Metadata paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
  static void Log(const paramType& aParam, std::wstring* aLog);
};

template <>
struct ParamTraits<mozilla::dom::asmjscache::WriteParams>
{
  typedef mozilla::dom::asmjscache::WriteParams paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
  static void Log(const paramType& aParam, std::wstring* aLog);
};

} // namespace IPC

#endif  // mozilla_dom_asmjscache_asmjscache_h
