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

#ifndef stlcfg_h___
#define stlcfg_h___

#include <iterator>
#include <string>
#include <memory>
 
#ifndef _WIN32
// Microsoft Visual C++ 6.0 bug: standard identifiers should be in std namespace
    using std::va_list;
    using std::strlen;
    using std::strcpy;
    using std::FILE;
    using std::getc;
    using std::fgets;
    using std::fputc;
    using std::fputs;
    using std::sprintf;
    using std::snprintf;
    using std::vsnprintf;
    using std::fprintf;
#   define STD std
#else
#   define STD
    // Microsoft Visual C++ 6.0 bug: these identifiers should not begin with
    // underscores
#   define snprintf _snprintf
#   define vsnprintf _vsnprintf
#endif

using std::string;
using std::auto_ptr;

#ifdef __GNUC__ // why doesn't g++ support iterator?
namespace std {
    template<class Category, class T, class Distance = ptrdiff_t,
        class Pointer = T*, class Reference = T&>
    struct iterator {
        typedef T value_type;
        typedef Distance difference_type;
        typedef Pointer pointer;
        typedef Reference reference;
        typedef Category iterator_category;
    };
};
#endif

#endif /* stlcfg_h___ */
