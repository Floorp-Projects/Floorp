
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the JavaScript 2 Prototype.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.   Portions created by Netscape are
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


    // XXX need to check for Long/ULong overflow throughout XXX

    case eMinus:
        {
	    a = pop();
            a = meta->toGeneralNumber(a);
            if (JS2VAL_IS_LONG(a)) {
                int64 v = *JS2VAL_TO_LONG(a);
                if (JSLL_EQ(v, JSLL_MININT))
                    meta->reportError(Exception::rangeError, "Arithmetic overflow", errorPos());
                JSLL_NEG(v, v);
                pushLong(v);
            }
            else
            if (JS2VAL_IS_ULONG(a)) {
                uint64 v = *JS2VAL_TO_ULONG(a);
                if (JSLL_UCMP(v, >, JSLL_MAXINT))
                    meta->reportError(Exception::rangeError, "Arithmetic overflow", errorPos());
                JSLL_NEG(v, v);
                pushLong(v);
            }
            else
                pushNumber(-meta->toFloat64(a));
        }
        break;

    case ePlus:
        {
	    a = pop();
            push(meta->toGeneralNumber(a));
        }
        break;

    case eComplement:
        {
	    a = pop();
            a = meta->toGeneralNumber(a);
            if (JS2VAL_IS_LONG(a)) {
                int64 i = *JS2VAL_TO_LONG(a);
                JSLL_NOT(i, i);
                pushLong(i);
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 i = *JS2VAL_TO_ULONG(a);
                    JSLL_NOT(i, i);
                    pushULong(i);
                }
                else {
                    pushNumber(~meta->toInteger(a));
                }
            }
        }
        break;    
    case eLeftShift:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            int32 count = meta->toInteger(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 r;
                JSLL_SHL(r, *JS2VAL_TO_LONG(a), count & 0x3F);
                pushLong(r);
            }
            else
            if (JS2VAL_IS_ULONG(a)) {
                uint64 r;
                JSLL_SHL(r, *JS2VAL_TO_ULONG(a), count & 0x3F);
                pushULong(r);
            }
            else
            pushNumber(meta->toInteger(a) << (count & 0x1F));
        }
        break;
    case eRightShift:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            int32 count = meta->toInteger(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 r;
                JSLL_SHR(r, *JS2VAL_TO_LONG(a), count & 0x3F);
                pushLong(r);
            }
            else
            if (JS2VAL_IS_ULONG(a)) {
                uint64 r;
                JSLL_USHR(r, *JS2VAL_TO_ULONG(a), count & 0x3F);
                pushULong(r);
            }
            else
            pushNumber(meta->toInteger(a) >> (count & 0x1F));
        }
        break;
    case eLogicalRightShift:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            int32 count = meta->toInteger(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 r;
                JSLL_SHR(r, *JS2VAL_TO_LONG(a), count & 0x3F);
                pushLong(r);
            }
            else
            if (JS2VAL_IS_ULONG(a)) {
                uint64 r;
                JSLL_USHR(r, *JS2VAL_TO_ULONG(a), count & 0x3F);
                pushULong(r);
            }
            else
            pushNumber(toUInt32(meta->toInteger(a)) >> (count & 0x1F));
        }
        break;
    case eBitwiseAnd:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_AND(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_AND(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_AND(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_AND(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_AND(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_AND(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else
                    pushNumber(meta->toInteger(a) & meta->toInteger(b));
            }
        }
        break;
    case eBitwiseXor:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_XOR(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_XOR(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_XOR(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_XOR(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_XOR(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_XOR(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else
                    pushNumber(meta->toInteger(a) ^ meta->toInteger(b));
            }
        }
        break;
    case eBitwiseOr:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_OR(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_OR(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_OR(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_OR(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_OR(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_OR(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else
                    pushNumber(meta->toInteger(a) | meta->toInteger(b));
            }
        }
        break;

    case eAdd: 
        {
	    b = pop();
	    a = pop();
	    a = meta->toPrimitive(a, NumberHint);
	    b = meta->toPrimitive(b, NumberHint);
	    if (JS2VAL_IS_STRING(a) || JS2VAL_IS_STRING(b)) {
	        const String *astr = meta->toString(a);
	        const String *bstr = meta->toString(b);
                String *c = allocStringPtr(astr);
                *c += *bstr;
	        push(STRING_TO_JS2VAL(c));
	    }
	    else {
                a = meta->toGeneralNumber(a);
                b = meta->toGeneralNumber(b);
                if (JS2VAL_IS_LONG(a)) {
                    int64 x = *JS2VAL_TO_LONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        int64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_ADD(z, x, y);
                        pushLong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_MUL(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            int64 y = checkInteger(b);
                            int64 z;
                            JSLL_ADD(z, x, y);
                            pushLong(z);   
                        }
                    }
                }
                else {
                    if (JS2VAL_IS_ULONG(a)) {
                        uint64 x = *JS2VAL_TO_ULONG(a);
                        if (JS2VAL_IS_LONG(b)) {
                            uint64 z;
                            int64 y = *JS2VAL_TO_LONG(b);
                            JSLL_ADD(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            if (JS2VAL_IS_ULONG(b)) {
                                uint64 z;
                                uint64 y = *JS2VAL_TO_ULONG(b);
                                JSLL_ADD(z, x, y);
                                pushULong(z);   
                            }
                            else {
                                uint64 y = checkInteger(b);
                                uint64 z;
                                JSLL_ADD(z, x, y);
                                pushULong(z);   
                            }
                        }
                    }
                    else {
                        float64 x = meta->toFloat64(a);
                        float64 y = meta->toFloat64(b);
                        float64 z = x + y;
                        if (JS2VAL_IS_FLOAT(a) || JS2VAL_IS_FLOAT(b))
                            pushFloat((float32)z);
                        else
                            pushNumber(z);
                    }
                }
	    } 
        }
        break;

    case eSubtract: 
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_SUB(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_SUB(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_SUB(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_SUB(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_SUB(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_SUB(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    float64 z = x - y;
                    if (JS2VAL_IS_FLOAT(a) || JS2VAL_IS_FLOAT(b))
                        pushFloat((float32)z);
                    else
                        pushNumber(z);
                }
            }
        }
        break;

    case eMultiply:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_MUL(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_MUL(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_MUL(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_MUL(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_MUL(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_MUL(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    float64 z = x * y;
                    if (JS2VAL_IS_FLOAT(a) || JS2VAL_IS_FLOAT(b))
                        pushFloat((float32)z);
                    else
                        pushNumber(z);
                }
            }
        }
        break;

    case eDivide:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_DIV(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_DIV(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_DIV(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_DIV(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_DIV(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_DIV(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    float64 z = x / y;
                    if (JS2VAL_IS_FLOAT(a) || JS2VAL_IS_FLOAT(b))
                        pushFloat((float32)z);
                    else
                        pushNumber(z);
                }
            }
        }
        break;

    case eModulo:
        {
	    b = pop();
	    a = pop();
            a = meta->toGeneralNumber(a);
            b = meta->toGeneralNumber(b);
            if (JS2VAL_IS_LONG(a)) {
                int64 x = *JS2VAL_TO_LONG(a);
                if (JS2VAL_IS_LONG(b)) {
                    int64 z;
                    int64 y = *JS2VAL_TO_LONG(b);
                    JSLL_MOD(z, x, y);
                    pushLong(z);   
                }
                else {
                    if (JS2VAL_IS_ULONG(b)) {
                        uint64 z;
                        uint64 y = *JS2VAL_TO_ULONG(b);
                        JSLL_MOD(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        int64 y = checkInteger(b);
                        int64 z;
                        JSLL_MOD(z, x, y);
                        pushLong(z);   
                    }
                }
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 x = *JS2VAL_TO_ULONG(a);
                    if (JS2VAL_IS_LONG(b)) {
                        uint64 z;
                        int64 y = *JS2VAL_TO_LONG(b);
                        JSLL_MOD(z, x, y);
                        pushULong(z);   
                    }
                    else {
                        if (JS2VAL_IS_ULONG(b)) {
                            uint64 z;
                            uint64 y = *JS2VAL_TO_ULONG(b);
                            JSLL_MOD(z, x, y);
                            pushULong(z);   
                        }
                        else {
                            uint64 y = checkInteger(b);
                            uint64 z;
                            JSLL_MOD(z, x, y);
                            pushULong(z);   
                        }
                    }
                }
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    float64 z;
#ifdef XP_PC
    /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
                    if (JSDOUBLE_IS_FINITE(x) && JSDOUBLE_IS_INFINITE(y))
                         z = x;
                    else
#endif
                        z = fd::fmod(x, y);
                    if (JS2VAL_IS_FLOAT(a) || JS2VAL_IS_FLOAT(b))
                        pushFloat((float32)z);
                    else
                        pushNumber(z);
                }
            }
        }
        break;

    case eLogicalXor: 
        {
	    b = pop();
	    a = pop();
            push(BOOLEAN_TO_JS2VAL(meta->toBoolean(a) ^ meta->toBoolean(b)));
        }
        break;

    case eLogicalNot: 
        {
	    a = pop();
            push(BOOLEAN_TO_JS2VAL(!meta->toBoolean(a)));
        }
        break;

    case eLess:
        {
	    b = pop();
	    a = pop();
            a = meta->toPrimitive(a, NumberHint);
            b = meta->toPrimitive(b, NumberHint);
            bool rval;
            if (JS2VAL_IS_STRING(a) && JS2VAL_IS_STRING(b))
                rval = (*JS2VAL_TO_STRING(a) < *JS2VAL_TO_STRING(b));
            else
                rval = meta->toFloat64(a) < meta->toFloat64(b);
            push(BOOLEAN_TO_JS2VAL(rval));
        }
        break;

    case eLessEqual:
        {
	    b = pop();
	    a = pop();
            a = meta->toPrimitive(a, NumberHint);
            b = meta->toPrimitive(b, NumberHint);
            bool rval;
            if (JS2VAL_IS_STRING(a) && JS2VAL_IS_STRING(b))
                rval = (*JS2VAL_TO_STRING(a) <= *JS2VAL_TO_STRING(b));
            else
                rval = meta->toFloat64(a) <= meta->toFloat64(b);
            push(BOOLEAN_TO_JS2VAL(rval));
        }
        break;

    case eGreater:
        {
	    b = pop();
	    a = pop();
            a = meta->toPrimitive(a, NumberHint);
            b = meta->toPrimitive(b, NumberHint);
            bool rval;
            if (JS2VAL_IS_STRING(a) && JS2VAL_IS_STRING(b))
                rval = (*JS2VAL_TO_STRING(a) > *JS2VAL_TO_STRING(b));
            else
                rval = meta->toFloat64(a) > meta->toFloat64(b);
            push(BOOLEAN_TO_JS2VAL(rval));
        }
        break;
    
    case eGreaterEqual:
        {
	    b = pop();
	    a = pop();
            a = meta->toPrimitive(a, NumberHint);
            b = meta->toPrimitive(b, NumberHint);
            bool rval;
            if (JS2VAL_IS_STRING(a) && JS2VAL_IS_STRING(b))
                rval = (*JS2VAL_TO_STRING(a) >= *JS2VAL_TO_STRING(b));
            else
                rval = meta->toFloat64(a) >= meta->toFloat64(b);
            push(BOOLEAN_TO_JS2VAL(rval));
        }
        break;
    
    case eNotEqual:
    case eEqual:
        {
            bool rval;
	    b = pop();
	    a = pop();
            if (JS2VAL_IS_NULL(a) || JS2VAL_IS_UNDEFINED(a))
                rval = (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b));
            else
            if (JS2VAL_IS_BOOLEAN(a)) {
                if (JS2VAL_IS_BOOLEAN(b))
                    rval = (JS2VAL_TO_BOOLEAN(a) == JS2VAL_TO_BOOLEAN(b));
                else {
                    b = meta->toPrimitive(b, NumberHint);
                    if (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b))
                        rval = false;
                    else
                        rval = (meta->toFloat64(a) == meta->toFloat64(b));
                }
            }
            else
            if (JS2VAL_IS_NUMBER(a)) {
                b = meta->toPrimitive(b, NumberHint);
                if (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b))
                    rval = false;
                else
                    rval = (meta->toFloat64(a) == meta->toFloat64(b));
            }
            else 
            if (JS2VAL_IS_STRING(a)) {
                b = meta->toPrimitive(b, NumberHint);
                if (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b))
                    rval = false;
                else
                if (JS2VAL_IS_BOOLEAN(b) || JS2VAL_IS_NUMBER(b))
                    rval = (meta->toFloat64(a) == meta->toFloat64(b));
                else
                    rval = (*JS2VAL_TO_STRING(a) == *JS2VAL_TO_STRING(b));
            }
            else     // a is not a primitive at this point, see if b is...
            if (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b))
                rval = false;
            else
            if (JS2VAL_IS_BOOLEAN(b)) {
                a = meta->toPrimitive(a, NumberHint);
                if (JS2VAL_IS_NULL(a) || JS2VAL_IS_UNDEFINED(a))
                    rval = false;
                else
                if (JS2VAL_IS_BOOLEAN(a))
                    rval = (JS2VAL_TO_BOOLEAN(a) == JS2VAL_TO_BOOLEAN(b));
                else
                    rval = (meta->toFloat64(a) == meta->toFloat64(b));
            }
            else
            if (JS2VAL_IS_NUMBER(b)) {
                a = meta->toPrimitive(a, NumberHint);
                if (JS2VAL_IS_NULL(a) || JS2VAL_IS_UNDEFINED(a))
                    rval = false;
                else
                    rval = (meta->toFloat64(a) == meta->toFloat64(b));
            }
            else
            if (JS2VAL_IS_STRING(b)) {
                a = meta->toPrimitive(a, NumberHint);
                if (JS2VAL_IS_NULL(a) || JS2VAL_IS_UNDEFINED(a))
                    rval = false;
                else
                if (JS2VAL_IS_BOOLEAN(a) || JS2VAL_IS_NUMBER(a))
                    rval = (meta->toFloat64(a) == meta->toFloat64(b));
                else
                    rval = (*JS2VAL_TO_STRING(a) == *JS2VAL_TO_STRING(b));
            }
            else
                rval = (JS2VAL_TO_OBJECT(a) == JS2VAL_TO_OBJECT(b));
               
            if (op == eEqual)
                push(BOOLEAN_TO_JS2VAL(rval));
            else
                push(BOOLEAN_TO_JS2VAL(!rval));
        }
        break;

    case eLexicalPostInc:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            a = meta->env->lexicalRead(meta, mn, phase);
            if (JS2VAL_IS_LONG(a)) {
                int64 i = *JS2VAL_TO_LONG(a);
                JSLL_ADD(i, i, 1);
                meta->env->lexicalWrite(meta, mn, allocLong(i), true, phase);
                push(a);
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 i = *JS2VAL_TO_ULONG(a);
                    JSLL_ADD(i, i, 1);
                    meta->env->lexicalWrite(meta, mn, allocULong(i), true, phase);
                    push(a);
                }
                else {
                    float64 num = meta->toFloat64(a);
                    meta->env->lexicalWrite(meta, mn, allocNumber(num + 1.0), true, phase);
                    pushNumber(num);
                }
            }
        }
        break;
    case eLexicalPostDec:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            a = meta->env->lexicalRead(meta, mn, phase);
            float64 num = meta->toFloat64(a);
            meta->env->lexicalWrite(meta, mn, allocNumber(num - 1.0), true, phase);
            pushNumber(num);
        }
        break;
    case eLexicalPreInc:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            a = meta->env->lexicalRead(meta, mn, phase);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            meta->env->lexicalWrite(meta, mn, a, true, phase);
        }
        break;
    case eLexicalPreDec:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            a = meta->env->lexicalRead(meta, mn, phase);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            meta->env->lexicalWrite(meta, mn, a, true, phase);
        }
        break;

    case eDotPostInc:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            if (!meta->readProperty(&baseVal, mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            meta->writeProperty(baseVal, mn, &lookup, true, allocNumber(num + 1.0), RunPhase);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eDotPostDec:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            if (!meta->readProperty(&baseVal, mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            meta->writeProperty(baseVal, mn, &lookup, true, allocNumber(num - 1.0), RunPhase);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eDotPreInc:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            if (!meta->readProperty(&baseVal, mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            meta->writeProperty(baseVal, mn, &lookup, true, a, RunPhase);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eDotPreDec:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            if (!meta->readProperty(&baseVal, mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            meta->writeProperty(baseVal, mn, &lookup, true, a, RunPhase);
            baseVal = JS2VAL_VOID;
        }
        break;

    case eBracketPostInc:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            indexVal = pop();
            baseVal = pop();
            const String *indexStr = meta->toString(indexVal);
            Multiname mn(&meta->world.identifiers[*indexStr], meta->publicNamespace);
            if (!meta->readProperty(&baseVal, &mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn.name);
            float64 num = meta->toFloat64(a);
            meta->writeProperty(baseVal, &mn, &lookup, true, allocNumber(num + 1.0), RunPhase);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
        }
        break;
    case eBracketPostDec:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            indexVal = pop();
            baseVal = pop();
            const String *indexStr = meta->toString(indexVal);
            Multiname mn(&meta->world.identifiers[*indexStr], meta->publicNamespace);
            if (!meta->readProperty(&baseVal, &mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn.name);
            float64 num = meta->toFloat64(a);
            meta->writeProperty(baseVal, &mn, &lookup, true, allocNumber(num - 1.0), RunPhase);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
        }
        break;
    case eBracketPreInc:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            indexVal = pop();
            baseVal = pop();
            const String *indexStr = meta->toString(indexVal);
            Multiname mn(&meta->world.identifiers[*indexStr], meta->publicNamespace);
            if (!meta->readProperty(&baseVal, &mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn.name);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            meta->writeProperty(baseVal, &mn, &lookup, true, a, RunPhase);
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
        }
        break;
    case eBracketPreDec:
        {
            LookupKind lookup(false, JS2VAL_NULL);
            indexVal = pop();
            baseVal = pop();
            const String *indexStr = meta->toString(indexVal);
            Multiname mn(&meta->world.identifiers[*indexStr], meta->publicNamespace);
            if (!meta->readProperty(&baseVal, &mn, &lookup, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn.name);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            meta->writeProperty(baseVal, &mn, &lookup, true, a, RunPhase);
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
        }
        break;

    case eSlotPostInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            baseVal = pop();
            ASSERT(JS2VAL_IS_OBJECT(baseVal));
            JS2Object *obj = JS2VAL_TO_OBJECT(baseVal);
            ASSERT(obj->kind == SimpleInstanceKind);
            a = checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value = allocNumber(num + 1.0);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eSlotPostDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            baseVal = pop();
            ASSERT(JS2VAL_IS_OBJECT(baseVal));
            JS2Object *obj = JS2VAL_TO_OBJECT(baseVal);
            ASSERT(obj->kind == SimpleInstanceKind);
            a = checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value = allocNumber(num - 1.0);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eSlotPreInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            baseVal = pop();
            ASSERT(JS2VAL_IS_OBJECT(baseVal));
            JS2Object *obj = JS2VAL_TO_OBJECT(baseVal);
            ASSERT(obj->kind == SimpleInstanceKind);
            a = checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value = a;
            baseVal = JS2VAL_VOID;
        }
        break;
    case eSlotPreDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            baseVal = pop();
            ASSERT(JS2VAL_IS_OBJECT(baseVal));
            JS2Object *obj = JS2VAL_TO_OBJECT(baseVal);
            ASSERT(obj->kind == SimpleInstanceKind);
            a = checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            checked_cast<SimpleInstance *>(obj)->slots[slotIndex].value = a;
            baseVal = JS2VAL_VOID;
        }
        break;
