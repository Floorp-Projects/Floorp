
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
                    pushNumber(~meta->valToInt32(a));
                }
            }
        }
        break;    
    case eLeftShift:
        {
            b = pop();
            a = pop();
            a = meta->toGeneralNumber(a);
            int32 count = meta->valToInt32(b);
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
            pushNumber(meta->valToInt32(a) << (count & 0x1F));
        }
        break;
    case eRightShift:
        {
            b = pop();
            a = pop();
            a = meta->toGeneralNumber(a);
            int32 count = meta->valToInt32(b);
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
            pushNumber(meta->valToInt32(a) >> (count & 0x1F));
        }
        break;
    case eLogicalRightShift:
        {
            b = pop();
            a = pop();
            a = meta->toGeneralNumber(a);
            int32 count = meta->valToInt32(b);
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
            pushNumber(meta->valToUInt32(a) >> (count & 0x1F));
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
                        int64 y = meta->truncateToInteger(b);
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
                    pushNumber(meta->valToInt32(a) & meta->valToInt32(b));
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
                    pushNumber(meta->valToInt32(a) ^ meta->valToInt32(b));
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
                    pushNumber(meta->valToInt32(a) | meta->valToInt32(b));
            }
        }
        break;

    case eAdd: 
        {
            b = pop();
            a = pop();
            a = meta->toPrimitive(a, NoHint);
            b = meta->toPrimitive(b, NoHint);
            if (JS2VAL_IS_STRING(a) || JS2VAL_IS_STRING(b)) {
                astr = meta->toString(a);
                bstr = meta->toString(b);
                push(STRING_TO_JS2VAL(concatStrings(astr, bstr)));
                astr = NULL;
                bstr = NULL;
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
            else {
                if (JS2VAL_IS_INT(a) && JS2VAL_IS_INT(b))
                    rval = JS2VAL_TO_INT(a) < JS2VAL_TO_INT(b);
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);                
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x < y));
                }
            }
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
            else {
                if (JS2VAL_IS_INT(a) && JS2VAL_IS_INT(b))
                    rval = (JS2VAL_TO_INT(a) <= JS2VAL_TO_INT(b));
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);                
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x <= y));
                }
            }
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
            else {
                if (JS2VAL_IS_INT(a) && JS2VAL_IS_INT(b))
                    rval = (JS2VAL_TO_INT(a) > JS2VAL_TO_INT(b));
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);                
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x > y));
                }
            }
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
            else {
                if (JS2VAL_IS_INT(a) && JS2VAL_IS_INT(b))
                    rval = (JS2VAL_TO_INT(a) >= JS2VAL_TO_INT(b));
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);                
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x >= y));
                }
            }
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
                    else {
                        float64 x = meta->toFloat64(a);
                        float64 y = meta->toFloat64(b);
                        rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x == y));
                    }
                }
            }
            else
            if (JS2VAL_IS_NUMBER(a)) {
                if (JS2VAL_IS_INT(a) && JS2VAL_IS_INT(b))
                    rval = (JS2VAL_TO_INT(a) == JS2VAL_TO_INT(b));
                else {
                    b = meta->toPrimitive(b, NumberHint);
                    if (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b))
                        rval = false;
                    else {
                        float64 x = meta->toFloat64(a);
                        float64 y = meta->toFloat64(b);
                        rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x == y));
                    }
                }
            }
            else 
            if (JS2VAL_IS_STRING(a)) {
                b = meta->toPrimitive(b, NumberHint);
                if (JS2VAL_IS_NULL(b) || JS2VAL_IS_UNDEFINED(b))
                    rval = false;
                else
                if (JS2VAL_IS_BOOLEAN(b) || JS2VAL_IS_NUMBER(b)) {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x == y));
                }
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
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x == y));
                }
            }
            else
            if (JS2VAL_IS_NUMBER(b)) {
                a = meta->toPrimitive(a, NumberHint);
                if (JS2VAL_IS_NULL(a) || JS2VAL_IS_UNDEFINED(a))
                    rval = false;
                else {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x == y));
                }
            }
            else
            if (JS2VAL_IS_STRING(b)) {
                a = meta->toPrimitive(a, NumberHint);
                if (JS2VAL_IS_NULL(a) || JS2VAL_IS_UNDEFINED(a))
                    rval = false;
                else
                if (JS2VAL_IS_BOOLEAN(a) || JS2VAL_IS_NUMBER(a)) {
                    float64 x = meta->toFloat64(a);
                    float64 y = meta->toFloat64(b);
                    rval = (!JSDOUBLE_IS_NaN(x) && !JSDOUBLE_IS_NaN(y) && (x == y));
                }
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

    case eIdentical:
    case eNotIdentical:
        {
            bool rval;
            b = pop();
            a = pop();
            if (JS2VAL_IS_PRIMITIVE(a) != JS2VAL_IS_PRIMITIVE(b))
                rval = false;
            else
                if (meta->objectType(a) != meta->objectType(b))
                    rval = false;
                else {
                    if (JS2VAL_IS_UNDEFINED(a) || JS2VAL_IS_NULL(a))
                        rval = true;
                    if (JS2VAL_IS_NUMBER(a)) {
                        float64 x = meta->toFloat64(a);
                        float64 y = meta->toFloat64(b);
                        if ((x != x) || (y != y))
                            rval = false;
                        else
                            if (x == y)
                                rval = true;
                            else
                                if ((x == 0) && (y == 0))
                                    rval = true;
                                else
                                    rval = false;
                    }
                    else {
                        if (JS2VAL_IS_STRING(a))
                            rval = (*JS2VAL_TO_STRING(a) == *JS2VAL_TO_STRING(b));
                        else
                            if (JS2VAL_IS_BOOLEAN(a))
                                rval = (JS2VAL_TO_BOOLEAN(a) == JS2VAL_TO_BOOLEAN(b));
                            else
                                rval = (a == b);
                    }
                }
            if (op == eIdentical)
                push(BOOLEAN_TO_JS2VAL(rval));
            else
                push(BOOLEAN_TO_JS2VAL(!rval));
        }
        break;

    case eLexicalPostInc:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            meta->env->lexicalRead(meta, mn, phase, &a, NULL);
            if (JS2VAL_IS_LONG(a)) {
                int64 i = *JS2VAL_TO_LONG(a);
                JSLL_ADD(i, i, 1);
                meta->env->lexicalWrite(meta, mn, allocLong(i), true);
                push(a);
            }
            else {
                if (JS2VAL_IS_ULONG(a)) {
                    uint64 i = *JS2VAL_TO_ULONG(a);
                    JSLL_ADD(i, i, 1);
                    meta->env->lexicalWrite(meta, mn, allocULong(i), true);
                    push(a);
                }
                else {
                    float64 num = meta->toFloat64(a);
                    meta->env->lexicalWrite(meta, mn, allocNumber(num + 1.0), true);
                    pushNumber(num);
                }
            }
        }
        break;
    case eLexicalPostDec:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            meta->env->lexicalRead(meta, mn, phase, &a, NULL);
            float64 num = meta->toFloat64(a);
            meta->env->lexicalWrite(meta, mn, allocNumber(num - 1.0), true);
            pushNumber(num);
        }
        break;
    case eLexicalPreInc:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            meta->env->lexicalRead(meta, mn, phase, &a, NULL);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            meta->env->lexicalWrite(meta, mn, a, true);
        }
        break;
    case eLexicalPreDec:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            meta->env->lexicalRead(meta, mn, phase, &a, NULL);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            meta->env->lexicalWrite(meta, mn, a, true);
        }
        break;

    case eDotPostInc:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->Read(meta, &baseVal, mn, NULL, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            if (!limit->Write(meta, baseVal, mn, NULL, true, allocNumber(num + 1.0), false))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eDotPostDec:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->Read(meta, &baseVal, mn, NULL, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            if (!limit->Write(meta, baseVal, mn, NULL, true, allocNumber(num - 1.0), false))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            pushNumber(num);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eDotPreInc:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->Read(meta, &baseVal, mn, NULL, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            if (!limit->Write(meta, baseVal, mn, NULL, true, a, false))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            baseVal = JS2VAL_VOID;
        }
        break;
    case eDotPreDec:
        {
            Multiname *mn = bCon->mMultinameList[BytecodeContainer::getShort(pc)];
            pc += sizeof(short);
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->Read(meta, &baseVal, mn, NULL, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            if (!limit->Write(meta, baseVal, mn, NULL, true, a, false))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), mn->name);
            baseVal = JS2VAL_VOID;
        }
        break;

    case eBracketPostInc:
        {
            indexVal = pop();
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->BracketRead(meta, &baseVal, indexVal, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            float64 num = meta->toFloat64(a);
            if (!limit->BracketWrite(meta, baseVal, indexVal, allocNumber(num + 1.0)))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            pushNumber(num);
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
        }
        break;
    case eBracketPostDec:
        {
            indexVal = pop();
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->BracketRead(meta, &baseVal, indexVal, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            float64 num = meta->toFloat64(a);
            if (!limit->BracketWrite(meta, baseVal, indexVal, allocNumber(num - 1.0)))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            pushNumber(num);
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
            astr = NULL;
        }
        break;
    case eBracketPreInc:
        {
            indexVal = pop();
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->BracketRead(meta, &baseVal, indexVal, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            if (!limit->BracketWrite(meta, baseVal, indexVal, a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
            astr = NULL;
        }
        break;
    case eBracketPreDec:
        {
            indexVal = pop();
            baseVal = pop();
            JS2Class *limit = meta->objectType(baseVal);
            if (!limit->BracketRead(meta, &baseVal, indexVal, RunPhase, &a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            if (!limit->BracketWrite(meta, baseVal, indexVal, a))
                meta->reportError(Exception::propertyAccessError, "No property named {0}", errorPos(), meta->toString(indexVal));
            baseVal = JS2VAL_VOID;
            indexVal = JS2VAL_VOID;
            astr = NULL;
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
            a = checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value = allocNumber(num + 1.0);
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
            a = checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value = allocNumber(num - 1.0);
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
            a = checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value = a;
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
            a = checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value;
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            checked_cast<SimpleInstance *>(obj)->fixedSlots[slotIndex].value = a;
            baseVal = JS2VAL_VOID;
        }
        break;

    case eFrameSlotPostInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < localFrame->frameSlots->size());
            a = (*localFrame->frameSlots)[slotIndex];
            if (JS2VAL_IS_INT(a) && (a != INT_TO_JS2VAL(JS2VAL_INT_MAX))) {
                (*localFrame->frameSlots)[slotIndex] += 2;      // pre-shifted INT as JS2VAL
                push(a);
            }
            else {
                float64 num = meta->toFloat64(a);
                (*localFrame->frameSlots)[slotIndex] = allocNumber(num + 1.0);
                pushNumber(num);
            }
        }
        break;
    case eFrameSlotPostDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < localFrame->frameSlots->size());
            a = (*localFrame->frameSlots)[slotIndex];
            if (JS2VAL_IS_INT(a) && (a != INT_TO_JS2VAL(JS2VAL_INT_MIN))) {
                (*localFrame->frameSlots)[slotIndex] -= 2;
                push(a);
            }
            else {
                float64 num = meta->toFloat64(a);
                (*localFrame->frameSlots)[slotIndex] = allocNumber(num - 1.0);
                pushNumber(num);
            }
        }
        break;
    case eFrameSlotPreInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < localFrame->frameSlots->size());
            a = (*localFrame->frameSlots)[slotIndex];
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            (*localFrame->frameSlots)[slotIndex] = a;
        }
        break;
    case eFrameSlotPreDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < localFrame->frameSlots->size());
            a = (*localFrame->frameSlots)[slotIndex];
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            (*localFrame->frameSlots)[slotIndex] = a;
        }
        break;

    case ePackageSlotPostInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < packageFrame->frameSlots->size());
            a = (*packageFrame->frameSlots)[slotIndex];
            float64 num = meta->toFloat64(a);
            (*packageFrame->frameSlots)[slotIndex] = allocNumber(num + 1.0);
            pushNumber(num);
        }
        break;
    case ePackageSlotPostDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < packageFrame->frameSlots->size());
            a = (*packageFrame->frameSlots)[slotIndex];
            float64 num = meta->toFloat64(a);
            (*packageFrame->frameSlots)[slotIndex] = allocNumber(num - 1.0);
            pushNumber(num);
        }
        break;
    case ePackageSlotPreInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < packageFrame->frameSlots->size());
            a = (*packageFrame->frameSlots)[slotIndex];
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            (*packageFrame->frameSlots)[slotIndex] = a;
        }
        break;
    case ePackageSlotPreDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(slotIndex < packageFrame->frameSlots->size());
            a = (*packageFrame->frameSlots)[slotIndex];
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            (*packageFrame->frameSlots)[slotIndex] = a;
        }
        break;

    case eParameterSlotPostInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(parameterSlots && slotIndex < parameterCount);
            a = parameterSlots[slotIndex];
            float64 num = meta->toFloat64(a);
            parameterSlots[slotIndex] = allocNumber(num + 1.0);
            pushNumber(num);
        }
        break;
    case eParameterSlotPostDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(parameterSlots && slotIndex < parameterCount);
            a = parameterSlots[slotIndex];
            float64 num = meta->toFloat64(a);
            parameterSlots[slotIndex] = allocNumber(num - 1.0);
            pushNumber(num);
        }
        break;
    case eParameterSlotPreInc:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(parameterSlots && slotIndex < parameterCount);
            a = parameterSlots[slotIndex];
            float64 num = meta->toFloat64(a);
            a = pushNumber(num + 1.0);
            parameterSlots[slotIndex] = a;
        }
        break;
    case eParameterSlotPreDec:
        {
            uint16 slotIndex = BytecodeContainer::getShort(pc);
            pc += sizeof(short);
            ASSERT(parameterSlots && slotIndex < parameterCount);
            a = parameterSlots[slotIndex];
            float64 num = meta->toFloat64(a);
            a = pushNumber(num - 1.0);
            parameterSlots[slotIndex] = a;
        }
        break;
