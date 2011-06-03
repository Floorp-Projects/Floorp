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

#include <unistd.h>

#include <string>

#include "chrome/common/child_process_info.h"

#include "mozilla/ipc/Transport.h"

using namespace base;
using namespace std;

namespace mozilla {
namespace ipc {

bool
CreateTransport(ProcessHandle /*unused*/, ProcessHandle /*unused*/,
                TransportDescriptor* aOne, TransportDescriptor* aTwo)
{
  // Gecko doesn't care about this random ID, and the argument to this
  // function isn't really necessary, it can be just any random
  // pointer value
  wstring id = ChildProcessInfo::GenerateRandomChannelID(aOne);
  // Use MODE_SERVER to force creation of the socketpair
  Transport t(id, Transport::MODE_SERVER, nsnull);
  int fd1 = t.GetServerFileDescriptor();
  int fd2, dontcare;
  t.GetClientFileDescriptorMapping(&fd2, &dontcare);
  if (fd1 < 0 || fd2 < 0) {
    return false;
  }

  // The Transport closes these fds when it goes out of scope, so we
  // dup them here
  fd1 = dup(fd1);
  fd2 = dup(fd2);
  if (fd1 < 0 || fd2 < 0) {
    return false;
  }

  aOne->mFd = FileDescriptor(fd1, true/*close after sending*/);
  aTwo->mFd = FileDescriptor(fd2, true/*close after sending*/);
  return true;
}

Transport*
OpenDescriptor(const TransportDescriptor& aTd, Transport::Mode aMode)
{
  return new Transport(aTd.mFd.fd, aMode, nsnull);
}

void
CloseDescriptor(const TransportDescriptor& aTd)
{
  close(aTd.mFd.fd);
}

} // namespace ipc
} // namespace mozilla
