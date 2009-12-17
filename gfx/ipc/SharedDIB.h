/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>
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

#ifndef gfx_SharedDIB_h__
#define gfx_SharedDIB_h__

#include "base/shared_memory.h"
#include "prtypes.h"
#include "nscore.h"

namespace mozilla {
namespace gfx {

class SharedDIB
{
public:
  typedef base::SharedMemoryHandle Handle;

public:
  SharedDIB();
  ~SharedDIB();

  // Create and allocate a new shared dib.
  nsresult Create(PRUint32 aSize);

  // Destroy or release resources associated with this dib.
  nsresult Close();

  // Returns true if this object contains a valid dib.
  bool IsValid();

  // Wrap a new shared dib around allocated shared memory. Note aHandle must point
  // to a memory section large enough to hold a dib of size aSize, otherwise this
  // will fail.
  nsresult Attach(Handle aHandle, PRUint32 aSize);

  // Returns a SharedMemoryHandle suitable for sharing with another process.
  nsresult ShareToProcess(base::ProcessHandle aChildProcess, Handle *aChildHandle);

protected:
  base::SharedMemory *mShMem;
};

} // gfx
} // mozilla

#endif
