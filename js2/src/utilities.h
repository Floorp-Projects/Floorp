/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef utilities_h___
#define utilities_h___

#ifdef MSC_VER
// disable long identifier warnings
#    pragma warning(disable: 4786)
#endif

#include "systemtypes.h"

namespace JavaScript
{
    
//
// Assertions
//

#ifdef DEBUG
    void Assert(const char *s, const char *file, int line);

#   define ASSERT(_expr)                                                       \
        ((_expr) ? (void)0 : JavaScript::Assert(#_expr, __FILE__, __LINE__))
#   define NOT_REACHED(_reasonStr)                                             \
        JavaScript::Assert(_reasonStr, __FILE__, __LINE__)
#   define DEBUG_ONLY(_stmt) _stmt
#else
#   define ASSERT(expr)
#   define NOT_REACHED(reasonStr)
#   define DEBUG_ONLY(_stmt)
#endif


//
// Mathematics
//

    template<class N> N min(N v1, N v2) {return v1 <= v2 ? v1 : v2;}
    template<class N> N max(N v1, N v2) {return v1 >= v2 ? v1 : v2;}

    uint ceilingLog2(uint32 n);
    uint floorLog2(uint32 n);
}
#endif /* utilities_h___ */
