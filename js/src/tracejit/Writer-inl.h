/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 *
 * Contributor(s):
 *   Jeff Walden <jwalden+code@mit.edu>
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

#ifndef tracejit_Writer_inl_h___
#define tracejit_Writer_inl_h___

#include "Writer.h"

namespace js {
namespace tjit {

namespace nj = nanojit;

nj::LIns *
Writer::getArgsLength(nj::LIns *args) const
{
    uint32 offset = JSObject::getFixedSlotOffset(ArgumentsObject::INITIAL_LENGTH_SLOT) + sPayloadOffset;
    return name(lir->insLoad(nj::LIR_ldi, args, offset, ACCSET_SLOTS),
                "argsLength");
}

inline nj::LIns *
Writer::ldpIterCursor(nj::LIns *iter) const
{
    return name(lir->insLoad(nj::LIR_ldp, iter, offsetof(NativeIterator, props_cursor),
                             ACCSET_ITER),
                "cursor");
}

inline nj::LIns *
Writer::ldpIterEnd(nj::LIns *iter) const
{
    return name(lir->insLoad(nj::LIR_ldp, iter, offsetof(NativeIterator, props_end), ACCSET_ITER),
                "end");
}

inline nj::LIns *
Writer::stpIterCursor(nj::LIns *cursor, nj::LIns *iter) const
{
    return lir->insStore(nj::LIR_stp, cursor, iter, offsetof(NativeIterator, props_cursor),
                         ACCSET_ITER);
}

} /* namespace tjit */
} /* namespace js */

#endif /* tracejit_Writer_inl_h___ */
