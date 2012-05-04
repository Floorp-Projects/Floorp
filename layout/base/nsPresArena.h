/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark <buster@netscape.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Dan Rosen <dr@netscape.com>
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

#ifndef nsPresArena_h___
#define nsPresArena_h___

#include "nscore.h"
#include "nsQueryFrame.h"

#include "mozilla/StandardInteger.h"

// Uncomment this to disable arenas, instead forwarding to
// malloc for every allocation.
//#define DEBUG_TRACEMALLOC_PRESARENA 1

// The debugging version of nsPresArena does not free all the memory it
// allocated when the arena itself is destroyed.
#ifdef DEBUG_TRACEMALLOC_PRESARENA
#define PRESARENA_MUST_FREE_DURING_DESTROY true
#else
#define PRESARENA_MUST_FREE_DURING_DESTROY false
#endif

class nsPresArena {
public:
  nsPresArena();
  ~nsPresArena();

  // Pool allocation with recycler lists indexed by object size, aSize.
  NS_HIDDEN_(void*) AllocateBySize(size_t aSize);
  NS_HIDDEN_(void)  FreeBySize(size_t aSize, void* aPtr);

  // Pool allocation with recycler lists indexed by frame-type ID.
  // Every aID must always be used with the same object size, aSize.
  NS_HIDDEN_(void*) AllocateByFrameID(nsQueryFrame::FrameIID aID, size_t aSize);
  NS_HIDDEN_(void)  FreeByFrameID(nsQueryFrame::FrameIID aID, void* aPtr);

  enum ObjectID {
    nsLineBox_id = nsQueryFrame::NON_FRAME_MARKER,

    // The PresArena implementation uses this bit to distinguish objects
    // allocated by size from objects allocated by type ID (that is, frames
    // using AllocateByFrameID and other objects using AllocateByObjectID).
    // It should not collide with any Object ID (above) or frame ID (in
    // nsQueryFrame.h).  It is not 0x80000000 to avoid the question of
    // whether enumeration constants are signed.
    NON_OBJECT_MARKER = 0x40000000
  };

  // Pool allocation with recycler lists indexed by object-type ID (see above).
  // Every aID must always be used with the same object size, aSize.
  NS_HIDDEN_(void*) AllocateByObjectID(ObjectID aID, size_t aSize);
  NS_HIDDEN_(void)  FreeByObjectID(ObjectID aID, void* aPtr);

  size_t SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

  /**
   * Get the poison value that can be used to fill a memory space with
   * an address that leads to a safe crash when dereferenced.
   *
   * The caller is responsible for ensuring that a pres shell has been
   * initialized before calling this.
   */
  static uintptr_t GetPoisonValue();

private:
  struct State;
  State* mState;
};

#endif
