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
    static char *keyword_offset = "offset";
    
    int cmp_nocase (const string& s1, string::const_iterator s2_begin,
                    string::const_iterator s2_end)
    {
        string::const_iterator p1 = s1.begin();
        string::const_iterator p2 = s2_begin;
        uint s2_size = s2_end - s2_begin;

        while (p1 != s1.end() && p2 != s2_end) {
            if (toupper(*p1) != toupper(*p2))
                return (toupper(*p1) < toupper(*p2)) ? -1 : 1;
            ++p1; ++p2;
        }

        return (s1.size() == s2_size) ? 0 : (s1.size() < s2_size) ? -1 : 1;
    }
        
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
    ICodeParser::ParseAlpha (iter begin, iter end, string **rval)
    {
        iter curpos;
        string *str = new string();
        
        for (curpos = begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case 'a'...'z':
                case 'A'...'Z':
                case '0'...'9':
                case '_':
                    *str += *curpos;
                    break;

                default:
                    goto scan_done;
            }
        }
      scan_done:

        *rval = str;
        return curpos;
    }

    iter
    ICodeParser::ParseBool (iter begin, iter end, bool *rval)
    {
        iter curpos = begin;
        
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

        throw new ICodeParseException ("Expected boolean value.");
    }
    
    iter
    ICodeParser::ParseDouble (iter begin, iter end, double *rval)
    {
        /* XXX add overflow checking */
        *rval = 0;
        uint32 integer;
        int sign = 1;

        /* pay no attention to the assignment of sign in the test condition :O */
        if (*begin == '+' || (*begin == '-' && (sign = -1)) {
            TokenLocation tl = SeekTokenStart (++begin, end);
            if (tl.estimate != teNumeric)
                throw new ICodeParseException ("Expected double value");
            begin = tl.begin;
        }


        iter curpos = ParseUInt32 (begin, end, &integer);
        *rval = static_cast<double>(integer);
        if (*curpos != '.')
        {
            *rval *= sign;
            return curpos;
        }

        ++curpos;
        int32 position = 0;
        
        for (; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case '0'...'9':
                    *rval += (*curpos - '0') * (1 / pow (10, ++position));
                    break;
                    
                default:
                    goto scan_done;
            }
        }
      scan_done:

        *rval *= sign;
        
        return curpos;
    }
        
    iter
    ICodeParser::ParseInt32 (iter begin, iter end, int32 *rval)
    {
        *rval = 0;
        int sign = 1;
        
        /* pay no attention to the assignment of sign in the test condition :O */
        if ((*begin == '+') || (*begin == '-' && (sign = -1))) {
            TokenLocation tl = SeekTokenStart (++begin, end);
            if (tl.estimate != teNumeric)
                throw new ICodeParseException ("Expected int32 value");
            begin = tl.begin;
        }

        end = ParseUInt32 (begin, end, rval);
        
        *rval *= sign;
        
        return end;
    }

    iter
    ICodeParser::ParseString (iter begin, iter end, string **rval)
    {
        char delim = *begin;
        bool isTerminated = false;
        /* XXX not exactly exception safe, string may never get deleted */
        string *str = new string();
        *rval = 0;
        
        if (delim != '\'' && delim != '"') {
            ASSERT ("|begin| does not point at a string");
            delete str;
            return 0;
        }

        iter curpos = 0;
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
            throw new ICodeParseException ("Unterminated string literal.");
        }

        *rval = str;
        return curpos;
    }
    
    iter
    ICodeParser::ParseUInt32 (iter begin, iter end, uint32 *rval)
    {
        /* XXX add overflow checking */
        *rval = 0;
        int32 position = -1;
        iter curpos;
        
        for (curpos = begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case '0'...'9':
                    position++;
                    break;

                default:
                    goto scan_done;

            }
        }
    scan_done:

        for (curpos = begin; position >= 0; --position)
            *rval += (*curpos++ - '0') * static_cast<uint32>(pow (10, position));

        return curpos;
    }

    iter ICodeParser::ParseInstruction (uint icodeID, iter begin, iter end)
    {
        iter curpos = begin;
        StatementNode *node = new StatementNode();
        node->begin = begin;
        node->icodeID = icodeID;

        /* add the node now, so the parse*operand functions can see it (to add
         * it to the fixup vector, for example.)
         */
        mStatementNodes.push_back (node);
        
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
                default:
                    break;                    
            }
        }

#       undef CASE_TYPE

        return curpos;
    }

    iter
    ICodeParser::ParseStatement (iter begin, iter end)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected an alphanumeric token (like maybe an instruction or a label.)");

        for (iter curpos = tl.begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case ':':
                    tl.type = ttLabel;
                    tl.end = ++curpos;
                    goto scan_done;

                case 'a'...'z':
                case 'A'...'Z':
                case '0'...'9':
                case '_':
                    break;

                default:
                    tl.type = ttInstruction;
                    tl.end = curpos;
                    goto scan_done;
            }
        }
    scan_done:

        if (tl.type == ttUndetermined) {
            tl.type = ttInstruction;
            tl.end = end;
        }
        
        if (tl.type == ttLabel) {
            string label_str(tl.begin, tl.end - 1); /* ignore the trailing : */
            mLabels[label_str] = mStatementNodes.end();
            return tl.end;
        } else if (tl.type == ttInstruction) {
            string icode_str(tl.begin, tl.end);
            for (uint i = 0; i < icodemap_size; ++i)
                if (cmp_nocase(icode_str, &icodemap[i].name[0],
                               &icodemap[i].name[0] +
                               strlen(icodemap[i].name) + 1))
                    return ParseInstruction (i, tl.end, end);
            throw new ICodeParseException ("Unknown ICode " + icode_str);
        } else {
            throw new ICodeParseException ("Internal error in ParseStatement.");
        }

    }

    iter
    ICodeParser::ParseArgumentListOperand (iter begin, iter /*end*/,
                                           AnyOperand */*o*/)
    {
        /* XXX this is hard, lets go shopping */
        ASSERT ("Not Implemented.");
        return begin;
    }

    iter
    ICodeParser::ParseBinaryOpOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected BinaryOp as a quoted string.");
        string *str;
        end = ParseString (tl.begin, end, &str);
        o->asString = str;
        return end;
    }

    iter
    ICodeParser::ParseBoolOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected boolean value.");

        return ParseBool (tl.begin, end, &o->asBoolean);
    }

    iter
    ICodeParser::ParseDoubleOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if ((tl.estimate != teNumeric) && (tl.estimate != teMinus) &&
            (tl.estimate != tePlus))
            throw new ICodeParseException ("Expected double value.");

        return ParseDouble (tl.begin, end, &o->asDouble);
    }

    iter
    ICodeParser::ParseICodeModuleOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected ICode Module as a quoted string.");

        string *str;
        end = ParseString (tl.begin, end, &str);
        o->asString = str;
        return end;
    }
    
    iter
    ICodeParser::ParseJSClassOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSClass as a quoted string.");

        string *str;
        end = ParseString (tl.begin, end, &str);
        o->asString = str;
        return end;
    }

    iter
    ICodeParser::ParseJSStringOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSString as a quoted string.");

        string *str;
        end = ParseString (tl.begin, end, &str);
        o->asString = str;
        return end;
    }
    
    iter
    ICodeParser::ParseJSFunctionOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSFunction as a quoted string.");

        string *str;
        end = ParseString (tl.begin, end, &str);
        o->asString = str;
        return end;
    }

    iter
    ICodeParser::ParseJSTypeOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSType as a quoted string.");
        string *str;
        end = ParseString (tl.begin, end, &str);
        o->asString = str;
        return end;
    }

    iter
    ICodeParser::ParseLabelOperand (iter begin, iter end, AnyOperand *o)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected Label Identifier or Offset keyword.");

        string *str;
        begin = ParseAlpha (tl.begin, end, &str);
        
        if (cmp_nocase(*string, keyword_offset, keyword_offset +
                       strlen(keyword_offset) + 1) == 0) {
            /* got the "Offset" keyword, treat next thing as a jump offset
             * expressed as "Offset +/-N" */
            tl = SeekTokenStart (begin, end);

            if ((tl.estimate != teNumeric) && (tl.estimate != teMinus) &&
                (tl.estimate != tePlus)) 
                throw new ICodeParseException ("Expected numeric value after Offset keyword.");
            int32 ofs;
            begin = ParseInt32 (tl.begin, end, &ofs);
            Label *l = new VM::Label(0);
            l->mOffset = mStatementNodes.size() + ofs - 1;
            mUnnamedLabels.push_back (l);
            o->asLabel = l;
        } else {
            /* label expressed as "label_name", look for it in the
             * namedlabels map */
            LabelMap::const_iterator l = mLabels.find(str->c_str());
            if (l != mLabels.end()) {
                /* found the label, use it */
                o->asLabel = *l;
            } else {
                /* havn't seen the label definition yet, put a placeholder
                 * in the namedlabels map */
                VM::Label *new_label = new VM::Label(0);
                new_label->mOffset = VM::NotALabel;
                o->asLabel = new_label;
                mNamedLabels[str] = new_label;
            }       
        }
        return begin;
    }

    iter
    ICodeParser::ParseUInt32Operand (iter begin, iter /*end*/,
                                     AnyOperand */*o*/)
    {
        ASSERT ("Not Implemented.");
        return begin;
    }

    iter
    ICodeParser::ParseRegisterOperand (iter begin, iter /*end*/,
                                       AnyOperand */*o*/)
    {
        ASSERT ("Not Implemented.");
        return begin;
    }

    iter
    ICodeParser::ParseStringAtomOperand (iter begin, iter /*end*/,
                                         AnyOperand */*o*/)
    {
        ASSERT ("Not Implemented.");
        return begin;
    }

}
}
