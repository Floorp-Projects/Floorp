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
 *   David Anderson <danderson@mozilla.com>
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
#ifndef jsion_assembler_shared_h__
#define jsion_assembler_shared_h__

#include "ion/IonRegisters.h"

namespace js {
namespace ion {

// Used for 32-bit immediates which do not require relocation.
struct Imm32
{
    int32_t value;

    Imm32(int32_t value) : value(value)
    { }
};

struct ImmWord
{
    uintptr_t value;

    ImmWord(uintptr_t value) : value(value)
    { }
};

// Used for immediates which require relocation.
struct ImmGCPtr
{
    uintptr_t value;

    ImmGCPtr(uintptr_t value) : value(value)
    { }
    ImmGCPtr(void *ptr) : value(reinterpret_cast<uintptr_t>(ptr))
    { }
};

// A label represents a position in an assembly buffer that may or may not have
// already been generated. Labels can either be "bound" or "unbound", the
// former meaning that its position is known and the latter that its position
// is not yet known.
//
// A jump to an unbound label adds that jump to the label's incoming queue. A
// jump to a bound label automatically computes the jump distance. The process
// of binding a label automatically corrects all incoming jumps.
struct Label
{
  private:
    // offset_ >= 0 means that the label is either bound or has incoming
    // edges and needs to be bound.
    int32 offset_ : 31;
    bool bound_   : 1;

    // Disallow assignment.
    void operator =(const Label &label);

  public:
    static const int32 INVALID_OFFSET = -1;

    Label() : offset_(INVALID_OFFSET), bound_(false)
    { }
    Label(const Label &label)
      : offset_(label.offset_),
        bound_(label.bound_)
    { }
    ~Label()
    {
        JS_ASSERT(!used());
    }

    // If the label is bound, all incoming edges have been patched and any
    // future incoming edges will be immediately patched.
    bool bound() const {
        return bound_;
    }
    int32 offset() const {
        JS_ASSERT(bound() || used());
        return offset_;
    }
    // Returns whether the label is not bound, but has incoming uses.
    bool used() const {
        return !bound() && offset_ > INVALID_OFFSET;
    }
    // Binds the label, fixing its final position in the code stream.
    void bind(int32 offset) {
        JS_ASSERT(!bound());
        offset_ = offset;
        bound_ = true;
        JS_ASSERT(offset_ == offset);
    }
    // Sets the label's latest used position, returning the old use position in
    // the process.
    int32 use(int32 offset) {
        JS_ASSERT(!bound());

        int32 old = offset_;
        offset_ = offset;
        JS_ASSERT(offset_ == offset);

        return old;
    }
};

} // namespace ion
} // namespace js

#endif // jsion_assembler_shared_h__

