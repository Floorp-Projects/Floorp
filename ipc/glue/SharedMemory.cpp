/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "nsIMemoryReporter.h"
#include "mozilla/ipc/SharedMemory.h"

namespace mozilla {
namespace ipc {

static PRInt64 gShmemAllocated;
static PRInt64 gShmemMapped;
static PRInt64 GetShmemAllocated(void*) { return gShmemAllocated; }
static PRInt64 GetShmemMapped(void*) { return gShmemMapped; }

NS_MEMORY_REPORTER_IMPLEMENT(ShmemAllocated,
                             "shmem/allocated",
                             "Shmem bytes accessible (not necessarily mapped)",
                             GetShmemAllocated,
                             nsnull)
NS_MEMORY_REPORTER_IMPLEMENT(ShmemMapped,
                             "shmem/mapped",
                             "Shmem bytes mapped into address space",
                             GetShmemMapped,
                             nsnull)

SharedMemory::SharedMemory()
  : mAllocSize(0)
  , mMappedSize(0)
{
  // NB: SharedMemory is main-thread-only at the moment, but that may
  // change soon
  static bool registered;
  if (!registered) {
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(ShmemAllocated));
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(ShmemMapped));
    registered = true;
  }
}

/*static*/ size_t
SharedMemory::PageAlignedSize(size_t aSize)
{
  size_t pageSize = SystemPageSize();
  size_t nPagesNeeded = size_t(ceil(double(aSize) / double(pageSize)));
  return pageSize * nPagesNeeded;
}

void
SharedMemory::Created(size_t aNBytes)
{
  mAllocSize = aNBytes;
  gShmemAllocated += mAllocSize;
}

void
SharedMemory::Mapped(size_t aNBytes)
{
  mMappedSize = aNBytes;
  gShmemMapped += mMappedSize;
}

void
SharedMemory::Unmapped()
{
  NS_ABORT_IF_FALSE(gShmemMapped >= PRInt64(mMappedSize),
                    "Can't unmap more than mapped");
  gShmemMapped -= mMappedSize;
  mMappedSize = 0;
}

/*static*/ void
SharedMemory::Destroyed()
{
  NS_ABORT_IF_FALSE(gShmemAllocated >= PRInt64(mAllocSize),
                    "Can't destroy more than allocated");
  gShmemAllocated -= mAllocSize;
  mAllocSize = 0;
}

} // namespace ipc
} // namespace mozilla
