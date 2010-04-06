/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the
 * "License"); you may not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific
 * language governing rights and limitations under the License.
 *
 * The Original Code is [Open Source Virtual Machine.]
 *
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights
 * Reserved.
 *
 * Contributor(s): Adobe AS3 Team
 *                 Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of either the GNU
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions
 * above and replace them with the notice and other provisions required by the GPL or the
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file
 * under the terms of any one of the MPL, the GPL or the LGPL.
 *
 ***** END LICENSE BLOCK ***** */

#include "nanojit.h"

#ifdef SOLARIS
    typedef caddr_t maddr_ptr;
#else
    typedef void *maddr_ptr;
#endif

using namespace avmplus;

size_t
VMPI_getVMPageSize()
{
    return 4096;
}

#ifdef WIN32
void
VMPI_setPageProtection(void *address,
                       size_t size,
                       bool executableFlag,
                       bool writeableFlag)
{
    DWORD oldProtectFlags = 0;
    DWORD newProtectFlags = 0;
    if ( executableFlag && writeableFlag ) {
        newProtectFlags = PAGE_EXECUTE_READWRITE;
    } else if ( executableFlag ) {
        newProtectFlags = PAGE_EXECUTE_READ;
    } else if ( writeableFlag ) {
        newProtectFlags = PAGE_READWRITE;
    } else {
        newProtectFlags = PAGE_READONLY;
    }

    BOOL retval;
    MEMORY_BASIC_INFORMATION mbi;
    do {
        VirtualQuery(address, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
        size_t markSize = size > mbi.RegionSize ? mbi.RegionSize : size;

        retval = VirtualProtect(address, markSize, newProtectFlags, &oldProtectFlags);
        NanoAssert(retval);

        address = (char*) address + markSize;
        size -= markSize;
    } while(size > 0 && retval);

    // We should not be clobbering PAGE_GUARD protections
    NanoAssert((oldProtectFlags & PAGE_GUARD) == 0);
}

#elif defined(AVMPLUS_OS2)

void
VMPI_setPageProtection(void *address,
                       size_t size,
                       bool executableFlag,
                       bool writeableFlag)
{
    ULONG flags = PAG_READ;
    if (executableFlag) {
        flags |= PAG_EXECUTE;
    }
    if (writeableFlag) {
        flags |= PAG_WRITE;
    }
    address = (void*)((size_t)address & ~(0xfff));
    size = (size + 0xfff) & ~(0xfff);

    ULONG attribFlags = PAG_FREE;
    while (size) {
        ULONG attrib;
        ULONG range = size;
        ULONG retval = DosQueryMem(address, &range, &attrib);
        NanoAssert(retval == 0);

        // exit if this is the start of the next memory object
        if (attrib & attribFlags) {
            break;
        }
        attribFlags |= PAG_BASE;

        range = size > range ? range : size;
        retval = DosSetMem(address, range, flags);
        NanoAssert(retval == 0);

        address = (char*)address + range;
        size -= range;
    }
}

#else // !WIN32 && !AVMPLUS_OS2

void VMPI_setPageProtection(void *address,
                            size_t size,
                            bool executableFlag,
                            bool writeableFlag)
{
  int bitmask = sysconf(_SC_PAGESIZE) - 1;
  // mprotect requires that the addresses be aligned on page boundaries
  void *endAddress = (void*) ((char*)address + size);
  void *beginPage = (void*) ((size_t)address & ~bitmask);
  void *endPage   = (void*) (((size_t)endAddress + bitmask) & ~bitmask);
  size_t sizePaged = (size_t)endPage - (size_t)beginPage;

  int flags = PROT_READ;
  if (executableFlag) {
    flags |= PROT_EXEC;
  }
  if (writeableFlag) {
    flags |= PROT_WRITE;
  }
  int retval = mprotect((maddr_ptr)beginPage, (unsigned int)sizePaged, flags);
  NanoAssert(retval == 0);
  (void)retval;
}

#endif // WIN32
