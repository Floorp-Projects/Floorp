/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Label_h
#define jit_Label_h

namespace js {
namespace jit {

struct LabelBase
{
  protected:
    // offset_ >= 0 means that the label is either bound or has incoming
    // uses and needs to be bound.
    int32_t offset_ : 31;
    bool bound_   : 1;

    // Disallow assignment.
    void operator =(const LabelBase &label);
  public:
    static const int32_t INVALID_OFFSET = -1;

    LabelBase() : offset_(INVALID_OFFSET), bound_(false)
    { }
    LabelBase(const LabelBase &label)
      : offset_(label.offset_),
        bound_(label.bound_)
    { }

    // If the label is bound, all incoming edges have been patched and any
    // future incoming edges will be immediately patched.
    bool bound() const {
        return bound_;
    }
    int32_t offset() const {
        JS_ASSERT(bound() || used());
        return offset_;
    }
    // Returns whether the label is not bound, but has incoming uses.
    bool used() const {
        return !bound() && offset_ > INVALID_OFFSET;
    }
    // Binds the label, fixing its final position in the code stream.
    void bind(int32_t offset) {
        JS_ASSERT(!bound());
        offset_ = offset;
        bound_ = true;
        JS_ASSERT(offset_ == offset);
    }
    // Marks the label as neither bound nor used.
    void reset() {
        offset_ = INVALID_OFFSET;
        bound_ = false;
    }
    // Sets the label's latest used position, returning the old use position in
    // the process.
    int32_t use(int32_t offset) {
        JS_ASSERT(!bound());

        int32_t old = offset_;
        offset_ = offset;
        JS_ASSERT(offset_ == offset);

        return old;
    }
};

// A label represents a position in an assembly buffer that may or may not have
// already been generated. Labels can either be "bound" or "unbound", the
// former meaning that its position is known and the latter that its position
// is not yet known.
//
// A jump to an unbound label adds that jump to the label's incoming queue. A
// jump to a bound label automatically computes the jump distance. The process
// of binding a label automatically corrects all incoming jumps.
class Label : public LabelBase
{
  public:
    Label()
    { }
    Label(const Label &label) : LabelBase(label)
    { }
};

// Label's destructor asserts that if it has been used it has also been bound.
// In the case long-lived labels, however, failed compilation (e.g. OOM) will
// trigger this failure innocuously. This Label silences the assertion.
class NonAssertingLabel : public Label
{
};

} } // namespace js::jit

#endif // jit_Label_h
