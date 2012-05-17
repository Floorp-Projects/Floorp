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
 * The Original Code is Mozilla SpiderMonkey JavaScript (IonMonkey).
 *
 * The Initial Developer of the Original Code is
 *   Nicolas Pierron <nicolas.b.pierron@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Nicolas B. Pierron <nicolas.b.pierron@mozilla.com>
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

#ifndef jsion_frame_iterator_inl_h__
#define jsion_frame_iterator_inl_h__

#include "ion/IonFrameIterator.h"

namespace js {
namespace ion {

template <class Op>
inline bool
InlineFrameIterator::forEachCanonicalActualArg(Op op, unsigned start, unsigned count)
{
    unsigned nactual = numActualArgs();
    if (count == unsigned(-1))
        count = nactual - start;

    unsigned end = start + count;
    JS_ASSERT(start <= end && end <= nactual);

    SnapshotIterator s(si_);
    
    s.skip(); // scopeChain
    s.skip(); // this

    unsigned i = 0;
    for (; i < start; i++)
        s.skip();
    for (; i < end; i++) {
        // We are not always able to read values from the snapshots, some values
        // such as non-gc things may still be live in registers and cause an
        // error while reading the machine state.
        Value v = s.maybeRead();
        if (!op(i, &v))
            return false;
    }
    return true;
}

} // namespace ion
} // namespace js

#endif // jsion_frame_iterator_inl_h__
