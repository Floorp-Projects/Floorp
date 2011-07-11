/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <adrake@adrake.org>
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
 * ***** END LICENSE BLOCK ***** */

#ifndef js_ion_movegroup_h__
#define js_ion_movegroup_h__

#include "IonLIR.h"

namespace js {
namespace ion {

// Register spill/restore/resolve marker.
class MoveGroup : public TempObject
{
    RegisterSet freeRegs;

  public:
    struct Entry {
        LAllocation *from;
        LAllocation *to;

        Entry () { }
        Entry(LAllocation *from, LAllocation *to)
          : from(from),
            to(to)
        { }
    };

  private:
    Vector<Entry, 1, IonAllocPolicy> entries_;

  public:
    bool add(LAllocation *from, LAllocation *to) {
        return entries_.append(Entry(from, to));
    }
    bool add(const Entry &ent) {
        return entries_.append(ent);
    }
    size_t numEntries() {
        return entries_.length();
    }
    Entry *getEntry(size_t i) {
        return &entries_[i];
    }
    void setEntry(size_t i, Entry ent) {
        entries_[i] = ent;
    }
    void setFreeRegisters(const RegisterSet &freeRegs) {
        this->freeRegs = freeRegs;
    }
    bool toInstructionsBefore(LBlock *block, LInstruction *ins, uint32 stack);
    bool toInstructionsAfter(LBlock *block, LInstruction *ins, uint32 stack);
#ifdef DEBUG
    void spewWorkStack(const Vector<Entry, 0, IonAllocPolicy>& workStack);
#else
    void spewWorkStack(const Vector<Entry, 0, IonAllocPolicy>& workStack) { };
#endif
};

}
}

#endif
