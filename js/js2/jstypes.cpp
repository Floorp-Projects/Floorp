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

#include "jstypes.h"
#include "numerics.h"
#include "icodegenerator.h"

namespace JavaScript {
namespace JSTypes {

//    using JavaScript::StringAtom;

// the canonical undefined value.
const JSValue kUndefinedValue;
const JSValue kNaN(0.0, 0.0);


       
JSValue::JSValue(float64 a, float64 b)
{
    f64 = a/b;
    tag = f64_tag;
}
       
int JSValue::operator==(const JSValue& value) const
{
    if (this->tag == value.tag) {
        #define CASE(T) case T##_tag: return (this->T == value.T)
        switch (tag) {
        CASE(i8); CASE(u8);
        CASE(i16); CASE(u16);
        CASE(i32); CASE(u32); CASE(f32);
        CASE(i64); CASE(u64); CASE(f64);
        CASE(object); CASE(array); CASE(function); CASE(string);
        #undef CASE
        // question:  are all undefined values equal to one another?
        case undefined_tag: return 1;
        }
    }
    return 0;
}

Formatter& operator<<(Formatter& f, const JSValue& value)
{
    switch (value.tag) {
    case JSValue::i32_tag:
        f << float64(value.i32);
        break;
    case JSValue::f64_tag:
        f << value.f64;
        break;
    case JSValue::object_tag:
    case JSValue::array_tag:
    case JSValue::function_tag:
        printFormat(f, "0x%08X", value.object);
        break;
    case JSValue::string_tag:
        f << *value.string;
        break;
    default:
        f << "undefined";
        break;
    }
    return f;
}


JSValue JSValue::valueToString(const JSValue& value) // can assume value is not a string
{
    char *chrp;
    char buf[dtosStandardBufferSize];
    switch (value.tag) {
    case JSValue::i32_tag:
        chrp = doubleToStr(buf, dtosStandardBufferSize, value.i32, dtosStandard, 0);
        break;
    case JSValue::f64_tag:
        chrp = doubleToStr(buf, dtosStandardBufferSize, value.f64, dtosStandard, 0);
        break;
    case JSValue::object_tag:
        chrp = "object";
        break;
    case JSValue::array_tag:
        chrp = "array";
        break;
    case JSValue::function_tag:
        chrp = "function";
        break;
    case JSValue::string_tag:
        return value;
    default:
        chrp = "undefined";
        break;
    }
    return JSValue(new JSString(chrp));
}

JSValue JSValue::valueToNumber(const JSValue& value) // can assume value is not a number
{
    switch (value.tag) {
    case JSValue::i32_tag:
    case JSValue::f64_tag:
        return value;
    case JSValue::string_tag: 
        {
            JSString* str = value.string;
            const char16 *numEnd;
	        double d = stringToDouble(str->begin(), str->end(), numEnd);
            return JSValue(d);
        }
    case JSValue::object_tag:
    case JSValue::array_tag:
    case JSValue::function_tag:
        break;
    default:
        return kNaN;
        break;
    }
    return kUndefinedValue;
}

JSFunction::~JSFunction()
{
    delete mICode;
}

JSString::JSString(const String& str)
{
    size_t n = str.size();
    resize(n);
    traits_type::copy(begin(), str.begin(), n);
}

JSString::JSString(const String* str)
{
    size_t n = str->size();
    resize(n);
    traits_type::copy(begin(), str->begin(), n);
}

JSString::JSString(const char* str)
{
    size_t n = ::strlen(str);
    resize(n);
    std::transform(str, str + n, begin(), JavaScript::widen);
}

} /* namespace JSTypes */    
} /* namespace JavaScript */
