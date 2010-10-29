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

#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/process_util.h"

#include "SharedMemoryBasic.h"

//
// Temporarily go directly to the kernel interface until we can
// interact better with libcutils.
//
#define ASHMEM_DEVICE  		"/dev/ashmem"
#define ASHMEM_NAME_LEN		256
#define __ASHMEMIOC 0x77
#define ASHMEM_SET_NAME		_IOW(__ASHMEMIOC, 1, char[ASHMEM_NAME_LEN])
#define ASHMEM_GET_NAME		_IOR(__ASHMEMIOC, 2, char[ASHMEM_NAME_LEN])
#define ASHMEM_SET_SIZE		_IOW(__ASHMEMIOC, 3, size_t)
#define ASHMEM_GET_SIZE		_IO(__ASHMEMIOC, 4)
#define ASHMEM_SET_PROT_MASK	_IOW(__ASHMEMIOC, 5, unsigned long)
#define ASHMEM_GET_PROT_MASK	_IO(__ASHMEMIOC, 6)

namespace mozilla {
namespace ipc {

static void
LogError(const char* what)
{
  __android_log_print(ANDROID_LOG_ERROR, "Gecko",
                      "%s: %s (%d)", what, strerror(errno), errno);
}

SharedMemoryBasic::SharedMemoryBasic()
  : mShmFd(-1)
  , mSize(0)
  , mMemory(nsnull)
{ }

SharedMemoryBasic::SharedMemoryBasic(const Handle& aHandle)
  : mShmFd(aHandle.fd)
  , mSize(0)
  , mMemory(nsnull)
{ }

SharedMemoryBasic::~SharedMemoryBasic()
{
  Unmap();
  if (mShmFd > 0) {
    close(mShmFd);
  }
}

bool
SharedMemoryBasic::Create(size_t aNbytes)
{
  NS_ABORT_IF_FALSE(-1 == mShmFd, "Already Create()d");

  // Carve a new instance off of /dev/ashmem
  int shmfd = open(ASHMEM_DEVICE, O_RDWR, 0600);
  if (-1 == shmfd) {
    LogError("ShmemAndroid::Create():open");
    return false;
  }

  if (ioctl(shmfd, ASHMEM_SET_SIZE, aNbytes)) {
    LogError("ShmemAndroid::Unmap():ioctl(SET_SIZE)");
    close(shmfd);
    return false;
  }

  mShmFd = shmfd;
  return true;
}

bool
SharedMemoryBasic::Map(size_t nBytes)
{
  NS_ABORT_IF_FALSE(nsnull == mMemory, "Already Map()d");

  mMemory = mmap(nsnull, nBytes,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 mShmFd,
                 0);
  if (MAP_FAILED == mMemory) {
    LogError("ShmemAndroid::Map()");
    mMemory = nsnull;
    return false;
  }

  mSize = nBytes;
  return true;
}

bool
SharedMemoryBasic::ShareToProcess(base::ProcessHandle/*unused*/,
                                  Handle* aNewHandle)
{
  NS_ABORT_IF_FALSE(mShmFd >= 0, "Should have been Create()d by now");

  int shmfdDup = dup(mShmFd);
  if (-1 == shmfdDup) {
    LogError("ShmemAndroid::ShareToProcess()");
    return false;
  }

  aNewHandle->fd = shmfdDup;
  aNewHandle->auto_close = true;
  return true;
}

void
SharedMemoryBasic::Unmap()
{
  if (!mMemory) {
    return;
  }

  if (munmap(mMemory, mSize)) {
    LogError("ShmemAndroid::Unmap()");
  }
  mMemory = nsnull;
  mSize = 0;
}

} // namespace ipc
} // namespace mozilla
