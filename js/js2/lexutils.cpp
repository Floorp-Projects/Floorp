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

#include "lexutils.h"

namespace JavaScript {
namespace LexUtils {

    int cmp_nocase (const string8& s1, string8_citer s2_begin,
                    string8_citer s2_end)
    {
        string8_citer p1 = s1.begin();
        string8_citer p2 = s2_begin;
        uint s2_size = s2_end - s2_begin - 1;

        while (p1 != s1.end() && p2 != s2_end) {
            if (toupper(*p1) != toupper(*p2))
                return (toupper(*p1) < toupper(*p2)) ? -1 : 1;
            ++p1; ++p2;
        }

        return (s1.size() == s2_size) ? 0 : (s1.size() < s2_size) ? -1 : 1;
    }
        
    TokenLocation
    seekTokenStart (string8_citer begin, string8_citer end)
    {
        TokenLocation tl;
        string8_citer curpos;
        bool inComment = false;

        for (curpos = begin; curpos < end; ++curpos) {
            if (!inComment) {
                switch (*curpos)
                {
                    case ' ':
                    case '\t':
                        /* look past the whitespace */
                        break;
                        
                    case ';':
                        inComment = true;
                        break;

                    case '\n':
                        tl.estimate = teNewline;
                        tl.begin = curpos;
                        return tl;
                        
                    case '-':
                        tl.estimate = teMinus;
                        tl.begin = curpos;
                        return tl;
                        
                    case '+':
                        tl.estimate = tePlus;
                        tl.begin = curpos;
                        return tl;
                        
                    case ',':
                        tl.estimate = teComma;
                        tl.begin = curpos;
                        return tl;
                        
                    case '"':
                    case '\'':
                        tl.estimate = teString;
                        tl.begin = curpos;
                        return tl;
                        
                    case '<':
                        tl.estimate = teLessThan;
                        tl.begin = curpos;
                        return tl;
                        
                    case '(':
                        tl.estimate = teOpenParen;
                        tl.begin = curpos;
                        return tl;
                        
                    case ')':
                        tl.estimate = teCloseParen;
                        tl.begin = curpos;
                        return tl;
                        
                    case ':':
                        tl.estimate = teColon;
                        tl.begin = curpos;
                        return tl;
                        
                    default:
                        if (IS_ALPHA(*curpos)) {
                            tl.estimate = teAlpha;
                            tl.begin = curpos;
                            return tl;
                        } else if (IS_NUMBER(*curpos)) {
                            tl.estimate = teNumeric;
                            tl.begin = curpos;
                            return tl;
                        } else {
                            tl.estimate = teUnknown;
                            tl.begin = curpos;
                            return tl;
                        }
                }
            } else if (*curpos == '\n') {
                tl.estimate = teNewline;
                tl.begin = curpos;
                return tl;
            }
        }

        tl.begin = curpos;
        tl.estimate = teEOF;
        return tl;
    }

    /**********************************************************************
     * general purpose lexer functions (see comment in the .h file) ...
     */

    string8_citer
    lexAlpha (string8_citer begin, string8_citer end, string8 **rval)
    {
        string8_citer curpos;
        string8 *str = new string8();
        
        for (curpos = begin; curpos < end; ++curpos) {
            if (IS_ALPHA(*curpos))
                *str += *curpos;
            else
                break;
        }

        *rval = str;
        return curpos;
    }

    string8_citer
    lexBool (string8_citer begin, string8_citer end, bool *rval)
    {
        string8_citer curpos = begin;
        
        if ((curpos != end) && (*curpos == 'T' || *curpos == 't')) {
            if ((++curpos != end) && (*curpos == 'R' || *curpos == 'r'))
                if ((++curpos != end) && (*curpos == 'U' || *curpos == 'u'))
                    if ((++curpos != end) &&
                        (*curpos == 'E' || *curpos == 'e')) {
                        *rval = true;
                        return ++curpos;
                    }
        } else if ((curpos != end) && (*curpos == 'F' || *curpos == 'f')) {
            if ((++curpos != end) && (*curpos == 'A' || *curpos == 'a'))
                if ((++curpos != end) && (*curpos == 'L' || *curpos == 'l'))
                    if ((++curpos != end) && (*curpos == 'S' || *curpos == 's'))
                        if ((++curpos != end) && 
                            (*curpos == 'E' || *curpos == 'e')) {
                            *rval = false;
                            return ++curpos;
                        }
        }

        throw JSLexException (eidExpectBool);
    }
    
    string8_citer
    lexDouble (string8_citer begin, string8_citer end, double *rval)
    {
        /* XXX add overflow checking */
        *rval = 0;
        uint32 integer;
        int sign = 1;

        /* pay no attention to the assignment of sign in the test condition :O */
        if (*begin == '+' || (*begin == '-' && (sign = -1))) {
            TokenLocation tl = seekTokenStart (++begin, end);
            if (tl.estimate != teNumeric)
                throw new JSLexException (eidExpectDouble);
            begin = tl.begin;
        }


        string8_citer curpos = lexUInt32 (begin, end, &integer);
        *rval = static_cast<double>(integer);
        if (*curpos != '.') {
            *rval *= sign;
            return curpos;
        }

        ++curpos;
        int32 position = 0;
        
        for (; curpos < end; ++curpos) {
            if (IS_NUMBER(*curpos))
                *rval += (*curpos - '0') * (1 / pow (10, ++position));
            else
                break;
        }

        *rval *= sign;
        
        return curpos;
    }
        
    string8_citer
    lexInt32 (string8_citer begin, string8_citer end, int32 *rval)
    {
        *rval = 0;
        int sign = 1;
        
        /* pay no attention to the assignment of sign in the test condition :O */
        if ((*begin == '+') || (*begin == '-' && (sign = -1))) {
            TokenLocation tl = seekTokenStart (++begin, end);
            if (tl.estimate != teNumeric)
                throw new JSLexException (eidExpectInt32);
            begin = tl.begin;
        }

        uint32 i;
        end = lexUInt32 (begin, end, &i);
        
        *rval = i * sign;
        
        return end;
    }

    string8_citer
    lexRegister (string8_citer begin, string8_citer end,
                 JSTypes::Register *rval)
    {
        if (*begin == 'R' || *begin == 'r') {
            if (++begin != end) {
                try
                {
                    end = lexUInt32 (begin, end, static_cast<uint32 *>(rval));
                    return end;
                }
                catch (JSLexException &e)
                {
                    /* rethrow as an "expected register" in fall through case */
                }
            }
        } else if (*begin == '<') {
            if ((++begin != end) && (*begin == 'N' || *begin == 'n'))
                if ((++begin != end) && (*begin == 'A' || *begin == 'a'))
                    if ((++begin != end) && (*begin == 'R' || *begin == 'r'))
                        if ((++begin != end) && *begin == '>') {
                            *rval = VM::NotARegister;
                            return ++begin;
                        }
        }

        throw JSLexException (eidExpectRegister);
    }

    string8_citer
    lexString8 (string8_citer begin, string8_citer end, string8 **rval)
    {
        char delim = *begin;
        bool isTerminated = false;
        /* XXX not exactly exception safe, string may never get deleted */
        string8 *str = new string8();
        *rval = 0;
        
        if (delim != '\'' && delim != '"') {
            NOT_REACHED ("|begin| does not point at a string");
            delete str;
            return 0;
        }

        string8_citer curpos = 0;
        bool isEscaped = false;
        for (curpos = ++begin; curpos < end; ++curpos) {

            switch (*curpos) {
                case '\\':
                    if (isEscaped) {
                        *str += '\\';
                        isEscaped = false;
                    } else {
                        isEscaped = true;
                    }
                    break;
                    
                case 't':
                    if (isEscaped) {
                        *str += '\t';
                        isEscaped = false;
                    } else {
                        *str += 't';
                    }
                    break;
                    
                case 'n':
                    if (isEscaped) {
                        *str += '\n';
                        isEscaped = false;
                    } else {
                        *str += 'n';
                    }
                    break;

                case 'r':
                    if (isEscaped) {
                        *str += '\r';
                        isEscaped = false;
                    } else {
                        *str += 'r';
                    }
                    break;

                case '\n':
                    if (isEscaped) {
                        *str += '\n';
                        isEscaped = false;
                    } else {
                        /* unescaped newline == unterminated string */
                        goto scan_done;
                    }
                    break;
                    
                case '\'':
                case '"':
                    if (*curpos == delim) {
                        if (isEscaped) {
                            *str += delim;
                            isEscaped = false;
                        } else {
                            ++curpos;
                            isTerminated = true;
                            goto scan_done;
                        }
                        break;
                    }

                default:
                    isEscaped = false;
                    *str += *curpos;
            }
        }
      scan_done:

        if (!isTerminated)
        {
            delete str;
            throw new JSLexException (eidUnterminatedString);
        }

        *rval = str;
        return curpos;
    }
    
    string8_citer
    lexUInt32 (string8_citer begin, string8_citer end, uint32 *rval)
    {
        /* XXX add overflow checking */
        *rval = 0;
        int32 position = -1;
        string8_citer curpos;
        
        for (curpos = begin; curpos < end; ++curpos) {
            if (IS_NUMBER(*curpos))
                position++;
            else
                break;
        }

        for (curpos = begin; position >= 0; --position)
            *rval += (*curpos++ - '0') * static_cast<uint32>(pow (10, position));

        return curpos;
    }
    
}
}
