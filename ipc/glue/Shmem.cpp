/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <math.h>

#include "Shmem.h"

#include "nsAutoPtr.h"


#if defined(DEBUG)
static const char sMagic[] =
    "This little piggy went to market.\n"
    "This little piggy stayed at home.\n"
    "This little piggy has roast beef,\n"
    "This little piggy had none.\n"
    "And this little piggy cried \"Wee! Wee! Wee!\" all the way home";
#endif

namespace mozilla {
namespace ipc {


#if defined(DEBUG)

namespace {

struct Header
{
  size_t mSize;
  char mMagic[sizeof(sMagic)];
};

void
GetSections(Shmem::SharedMemory* aSegment,
            char** aFrontSentinel,
            char** aData,
            char** aBackSentinel)
{
  NS_ABORT_IF_FALSE(aSegment && aFrontSentinel && aData && aBackSentinel,
                    "NULL param(s)");

  *aFrontSentinel = reinterpret_cast<char*>(aSegment->memory());
  NS_ABORT_IF_FALSE(*aFrontSentinel, "NULL memory()");

  size_t pageSize = Shmem::SharedMemory::SystemPageSize();
  *aData = *aFrontSentinel + pageSize;

  *aBackSentinel = *aFrontSentinel + aSegment->Size() - pageSize;
}

} // namespace <anon>

//
// In debug builds, we specially allocate shmem segments.  The layout
// is as follows
//
//   Page 0: "front sentinel"
//     size of mapping
//     magic bytes
//   Page 1 through n-1:
//     user data
//   Page n: "back sentinel"
//     [nothing]
//
// The mapping can be in one of the following states, wrt to the
// current process.
//
//   State "unmapped": all pages are mapped with no access rights.
//
//   State "mapping": all pages are mapped with read/write access.
//
//   State "mapped": the front and back sentinels are mapped with no
//     access rights, and all the other pages are mapped with
//     read/write access.
//
// When a SharedMemory segment is first allocated, it starts out in
// the "mapping" state for the process that allocates the segment, and
// in the "unmapped" state for the other process.  The allocating
// process will then create a Shmem, which takes the segment into the
// "mapped" state, where it can be accessed by clients.
//
// When a Shmem is sent to another process in an IPDL message, the
// segment transitions into the "unmapped" state for the sending
// process, and into the "mapping" state for the receiving process.
// The receiving process will then create a Shmem from the underlying
// segment, and take the segment into the "mapped" state.
//
// In the "mapping" state, we use the front sentinel to verify the
// integrity of the shmem segment.  If valid, it has a size_t
// containing the number of bytes the user allocated followed by the
// magic bytes above.
//
// In the "mapped" state, the front and back sentinels have no access
// rights.  They act as guards against buffer overflows and underflows
// in client code; if clients touch a sentinel, they die with SIGSEGV.
//
// The "unmapped" state is used to enforce single-owner semantics of
// the shmem segment.  If a process other than the current owner tries
// to touch the segment, it dies with SIGSEGV.
//

Shmem::Shmem(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
             SharedMemory* aSegment, id_t aId) :
    mSegment(aSegment),
    mData(0),
    mSize(0)
{
  NS_ABORT_IF_FALSE(mSegment, "NULL segment");
  NS_ABORT_IF_FALSE(aId != 0, "invalid ID");

  Unprotect(mSegment);

  char* frontSentinel;
  char* data;
  char* backSentinel;
  GetSections(aSegment, &frontSentinel, &data, &backSentinel);

  // do a quick validity check to avoid weird-looking crashes in libc
  char check = *frontSentinel;
  (void)check;

  Header* header = reinterpret_cast<Header*>(frontSentinel);
  NS_ABORT_IF_FALSE(!strncmp(header->mMagic, sMagic, sizeof(sMagic)),
                      "invalid segment");
  mSize = header->mSize;

  size_t pageSize = SharedMemory::SystemPageSize();
  // transition into the "mapped" state by protecting the front and
  // back sentinels (which guard against buffer under/overflows)
  mSegment->Protect(frontSentinel, pageSize, RightsNone);
  mSegment->Protect(backSentinel, pageSize, RightsNone);

  // don't set these until we know they're valid
  mData = data;
  mId = aId;
}

void
Shmem::AssertInvariants() const
{
  NS_ABORT_IF_FALSE(mSegment, "NULL segment");
  NS_ABORT_IF_FALSE(mData, "NULL data pointer");
  NS_ABORT_IF_FALSE(mSize > 0, "invalid size");
  // if the segment isn't owned by the current process, these will
  // trigger SIGSEGV
  char checkMappingFront = *reinterpret_cast<char*>(mData);
  char checkMappingBack = *(reinterpret_cast<char*>(mData) + mSize - 1);
  checkMappingFront = checkMappingBack; // avoid "unused" warnings
}

void
Shmem::Protect(SharedMemory* aSegment)
{
  NS_ABORT_IF_FALSE(aSegment, "NULL segment");
  aSegment->Protect(reinterpret_cast<char*>(aSegment->memory()),
                    aSegment->Size(),
                    RightsNone);
}

void
Shmem::Unprotect(SharedMemory* aSegment)
{
  NS_ABORT_IF_FALSE(aSegment, "NULL segment");
  aSegment->Protect(reinterpret_cast<char*>(aSegment->memory()),
                    aSegment->Size(),
                    RightsRead | RightsWrite);
}

Shmem::SharedMemory*
Shmem::Alloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
             size_t aNBytes,
             bool aProtect)
{
  size_t pageSize = SharedMemory::SystemPageSize();
  // |2*pageSize| is for the front and back sentinel
  SharedMemory* segment = CreateSegment(PageAlignedSize(aNBytes + 2*pageSize));
  if (!segment)
    return 0;

  char *frontSentinel;
  char *data;
  char *backSentinel;
  GetSections(segment, &frontSentinel, &data, &backSentinel);

  // initialize the segment with Shmem-internal information
  Header* header = reinterpret_cast<Header*>(frontSentinel);
  memcpy(header->mMagic, sMagic, sizeof(sMagic));
  header->mSize = aNBytes;

  if (aProtect)
    Protect(segment);

  return segment;
}

Shmem::SharedMemory*
Shmem::OpenExisting(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
                    SharedMemoryHandle aHandle,
                    size_t aNBytes,
                    bool aProtect)
{
  if (!SharedMemory::IsHandleValid(aHandle))
    NS_RUNTIMEABORT("trying to open invalid handle");

  size_t pageSize = SharedMemory::SystemPageSize();
  // |2*pageSize| is for the front and back sentinels
  SharedMemory* segment = CreateSegment(PageAlignedSize(aNBytes + 2*pageSize),
                                        aHandle);
  if (!segment)
    return 0;

  if (aProtect)
    Protect(segment);

  return segment;
}

void
Shmem::Dealloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
               SharedMemory* aSegment)
{
  if (!aSegment)
    return;

  size_t pageSize = SharedMemory::SystemPageSize();
  char *frontSentinel;
  char *data;
  char *backSentinel;
  GetSections(aSegment, &frontSentinel, &data, &backSentinel);

  aSegment->Protect(frontSentinel, pageSize, RightsWrite | RightsRead);
  Header* header = reinterpret_cast<Header*>(frontSentinel);
  memset(header->mMagic, 0, sizeof(sMagic));
  header->mSize = 0;

  DestroySegment(aSegment);
}


#else  // !defined(DEBUG)

Shmem::SharedMemory*
Shmem::Alloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
             size_t aNBytes, 
             bool /*unused*/)
{
  SharedMemory* segment =
    CreateSegment(PageAlignedSize(aNBytes + sizeof(size_t)));
  if (!segment)
    return 0;

  *PtrToSize(segment) = aNBytes;

  return segment;
}

Shmem::SharedMemory*
Shmem::OpenExisting(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
                    SharedMemoryHandle aHandle,
                    size_t aNBytes,
                    bool /* unused */)
{
  SharedMemory* segment =
    CreateSegment(PageAlignedSize(aNBytes + sizeof(size_t)), aHandle);
  if (!segment)
    return 0;

  // this is the only validity check done OPT builds
  if (aNBytes != *PtrToSize(segment))
    NS_RUNTIMEABORT("Alloc() segment size disagrees with OpenExisting()'s");

  return segment;
}

void
Shmem::Dealloc(IHadBetterBeIPDLCodeCallingThis_OtherwiseIAmADoodyhead,
               SharedMemory* aSegment)
{
  DestroySegment(aSegment);
}


#endif  // if defined(DEBUG)


Shmem::SharedMemory*
Shmem::CreateSegment(size_t aNBytes, SharedMemoryHandle aHandle)
{
  nsAutoPtr<SharedMemory> segment;

  if (SharedMemory::IsHandleValid(aHandle)) {
    segment = new SharedMemory(aHandle);
  }
  else {
    segment = new SharedMemory();
    if (!segment->Create("", false, false, aNBytes))
      return 0;
  }
  if (!segment->Map(aNBytes))
    return 0;
  return segment.forget();
}

void
Shmem::DestroySegment(SharedMemory* aSegment)
{
  // the SharedMemory dtor closes and unmaps the actual OS shmem segment
  delete aSegment;
}

size_t
Shmem::PageAlignedSize(size_t aSize)
{
  size_t pageSize = SharedMemory::SystemPageSize();
  size_t nPagesNeeded = int(ceil(double(aSize) / double(pageSize)));
  return pageSize * nPagesNeeded;
}


} // namespace ipc
} // namespace mozilla
