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

#include <stdio.h>
#include <string>
#include <iterator>

#include "icodemap.h"

#define iter string::iterator
//std::iterator<std::random_access_iterator_tag, char, ptrdiff_t>

namespace JavaScript {
namespace ICodeASM {
    enum TokenEstimation {
        /* guess at tokentype, based on first character of token */
        teEOF,
        teUnknown,
        teIllegal,
        teComma,
        teNumeric,
        teAlpha,
        teString,
        teNotARegister
    };

    enum TokenType {
        /* verified token type */
        ttUndetermined,
        ttLabel,
        ttInstruction,
        ttRegister,
        ttRegisterList,
        ttNotARegister,
        ttString,
        ttNumber,
        ttOffsetKeyword
    };

    struct ICodeParserException {
        ICodeParserException (string aMsg, iter aPos = 0)
                : msg(aMsg), pos(aPos) {}
        string msg;
        iter aPos;
    };
        
    struct TokenLocation {
        TokenLocation () : begin(0), end(0), estimate(teIllegal),
                           type(ttUndetermined) {}
        iter begin, end;
        TokenEstimation estimate;
        TokenType type;
    };
    
    class ICodeParser 
    {
    private:
        uint mMaxRegister;
        
    public:
        void ParseSourceFromString (string source);
        TokenLocation SeekTokenStart (iter begin, iter end);
        iter ParseLine (iter begin, iter end);
        void ParseLabelOrInstruction (TokenLocation tl, iter end);
        iter ParseOperands (uint icodeID, iter start, iter end);
        void ParseRegisterOperand (TokenLocation tl, iter end);
        void ParseDoubleOperand (TokenLocation tl, iter end);
    };

    void
    ICodeParser::ParseSourceFromString (string source)
    {
        uint lineNo = 0;
        
        try
        {
            ++lineNo;
            ParseLine (source.begin(), source.end());
        }
        catch (ICodeParserException e)
        {
            fprintf (stderr, "Parse Error: %s at %u\n", e.msg.c_str(), lineNo);
            return;
        }
    }

    TokenLocation
    ICodeParser::SeekTokenStart (iter begin, iter end)
    {
        TokenLocation tl;
        iter curpos;

        tl.type = ttUndetermined;
        
        for (curpos = begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case ' ':
                case '\t':
                case '\n':
                    /* look past the whitespace */
                    break;

                case 'a'...'z':
                case 'A'...'Z':
                    tl.estimate = teAlpha;
                    tl.begin = curpos;
                    return tl;

                case '0'...'1':
                    tl.estimate = teNumeric;
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
                    tl.estimate = teNotARegister;
                    tl.begin = curpos;
                    return tl;

                default:
                    tl.estimate = teUnknown;
                    tl.begin = curpos;
                    return tl;
            }
        }

        tl.begin = curpos;
        tl.estimate = teEOF;
        return tl;
    }

    iter
    ICodeParser::ParseLine (iter begin, iter end)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParserException ("Expected an alphanumeric token (like maybe an instruction or an opcode.)");

        ParseLabelOrInstruction (tl, end);

        if (tl.type = ttLabel) {
            /* XXX Do label-like things */
            return tl.end;
        } else {
            string icode_str (tl.begin, tl.end);
            for (uint i = 0; i <= icodemap_size; ++i)
                if (icode_str.compare (icode_map[i]) == 0)
                    return ParseOperands (i, tl.end, end);
            
            throw new ICodeParserException ("Unknown ICode " + icode_str);
        }
    }

    void
    ICodeParser::ParseLabelOrInstruction (TokenLocation tl, iter end)
    {
        iter curpos;
        
        for (curpos = tl.begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case ':':
                    tl.type = ttLabel;
                    tl.end = ++curpos;
                    return;

                case 'a'...'z':
                case 'A'...'Z':
                case '0'...'9':
                    break;

                default:
                    tl.type = ttInstruction;
                    tl.end = curpos;
                    return;
            }
        }

        tl.type = ttInstruction;
        tl.end = curpos;
        return;
    }

    iter ICodeParser::ParseOperands (uint icodeID, iter start, iter end)
    {
        iter pos = start;
        
        if (icode_map[icodeID].otype1 != otNone)
}
}
