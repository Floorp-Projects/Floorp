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

#include "icodeasm.h"
#include "icodemap.h"

namespace JavaScript {
namespace ICodeASM {

    void
    ICodeParser::ParseSourceFromString (string source)
    {
        uint statementNo = 0;
        
        try
        {
            ++statementNo;
            ParseStatement (source.begin(), source.end());
        }
        catch (ICodeParseException e)
        {
            fprintf (stderr, "Parse Error: %s at statement %u\n", e.msg.c_str(),
                     statementNo);
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
                case '_':
                    tl.estimate = teAlpha;
                    tl.begin = curpos;
                    return tl;

                case '0'...'9':
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
    ICodeParser::ParseStatement (iter begin, iter end)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected an alphanumeric token (like maybe an instruction or an opcode.)");

    scan_loop:
        for (iter curpos = tl.begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case ':':
                    tl.type = ttLabel;
                    tl.end = ++curpos;
                    break scan_loop;

                case 'a'...'z':
                case 'A'...'Z':
                case '0'...'9':
                case '_':
                    break;

                default:
                    tl.type = ttInstruction;
                    tl.end = curpos;
                    break scan_loop;
            }
        }

        if (tl.type = ttUndetermined) {
            tl.type = ttInstruction;
            tl.end = end;
        }
        
        if (tl.type == ttLabel) {
            string label_str(tl.begin, tl.end - 1);  /* ignore the trailing : */
            mLabels[label_str] = mStatementNodes.end();
            return tl.end;
        } else if (tl.type == ttInstruction) {
            string icode_str(tl.begin, tl.end);
            for (uint i = 0; i < icodemap_size; ++i)
                if (icode_str.compare(icodemap[i]) == 0)
                    return ParseInstruction (i, tl.end, end);
            throw ("Unknown ICode " + icode_str);
        }
    }

    iter ICodeParser::ParseInstruction (uint icodeID, iter start, iter end)
    {
        iter curpos = start;
        StatementNode *node = new StatementNode();
        node->icodeID = icodeID;
        
#       define CASE_TYPE(T)                                                \
           case ot##T:                                                     \
           {                                                               \
              curpos = Parse##T##Operand (curpos, end, &node->operand[i]); \
              break;                                                       \
           }
 
        for (uint i = 0; i < 4; ++i)
        {
            switch (icodemap[icodeID].otype[i])
            {
                CASE_TYPE(ArgumentList);
                CASE_TYPE(BinaryOp);
                CASE_TYPE(Bool);
                CASE_TYPE(Double);
                CASE_TYPE(ICodeModule);
                CASE_TYPE(JSClass);
                CASE_TYPE(JSString);
                CASE_TYPE(JSFunction);
                CASE_TYPE(JSType);
                CASE_TYPE(Label);
                CASE_TYPE(UInt32);
                CASE_TYPE(Register);
                CASE_TYPE(StringAtom);
            }
        }
    }
}
}
