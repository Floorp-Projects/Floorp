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
    static char *keyword_binaryops[] = {"add", "subtract", "multiply", "divide",
                                        "remainder", "leftshift", "rightshift",
                                        "lessorequal", "equal", "identical", 0};
    
    int cmp_nocase (const string& s1, string::const_iterator s2_begin,
                    string::const_iterator s2_end)
    {
        string::const_iterator p1 = s1.begin();
        string::const_iterator p2 = s2_begin;
        uint s2_size = s2_end - s2_begin - 1;

        while (p1 != s1.end() && p2 != s2_end) {
            if (toupper(*p1) != toupper(*p2))
                return (toupper(*p1) < toupper(*p2)) ? -1 : 1;
            ++p1; ++p2;
        }

        return (s1.size() == s2_size) ? 0 : (s1.size() < s2_size) ? -1 : 1;
    }
        
    void
    ICodeParser::ParseSourceFromString (const string &source)
    {
        uint statementNo = 0;
        iter begin = source.begin();
        iter end = source.end();

        while (begin != end)
        {
            try
            {
                ++statementNo;
                begin = ParseStatement (begin, end);
            }
            catch (ICodeParseException *e)
            {
                fprintf (stderr, "Parse Error: %s at statement %u\n",
                         e->msg.c_str(), statementNo);
                delete e;
                return;
            }
        }
    }

    TokenLocation
    ICodeParser::SeekTokenStart (iter begin, iter end)
    {
        TokenLocation tl;
        iter curpos;

        //        tl.type = ttUndetermined;
        
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
                    tl.estimate = teUnknown;
                    tl.begin = curpos;
                    return tl;
            }
        }

        tl.begin = curpos;
        tl.estimate = teEOF;
        return tl;
    }

    /**********************************************************************
     * general purpose parse functions (see comment in the .h file) ...
     */

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

        throw new ICodeParseException ("Expected boolean value");
    }
    
    iter
    ICodeParser::ParseDouble (iter begin, iter end, double *rval)
    {
        /* XXX add overflow checking */
        *rval = 0;
        uint32 integer;
        int sign = 1;

        /* pay no attention to the assignment of sign in the test condition :O */
        if (*begin == '+' || (*begin == '-' && (sign = -1))) {
            TokenLocation tl = SeekTokenStart (++begin, end);
            if (tl.estimate != teNumeric)
                throw new ICodeParseException ("Expected double value");
            begin = tl.begin;
        }


        iter curpos = ParseUInt32 (begin, end, &integer);
        *rval = static_cast<double>(integer);
        if (*curpos != '.') {
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

        uint32 i;
        end = ParseUInt32 (begin, end, &i);
        
        *rval = i * sign;
        
        return end;
    }

    iter
    ICodeParser::ParseRegister (iter begin, iter end, JSTypes::Register *rval)
    {
        if (*begin == 'R' || *begin == 'r') {
            if (++begin != end) {
                try
                {
                    return ParseUInt32 (++begin, end, 
                                        static_cast<uint32 *>(rval));
                }
                catch (ICodeParseException *e)
                {
                    /* rethrow as an "expected register" in fall through case */
                    delete e;
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

        throw new ICodeParseException ("Expected Register value");
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
            NOT_REACHED ("|begin| does not point at a string");
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
            throw new ICodeParseException ("Unterminated string literal");
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

    /**********************************************************************
     * operand parse functions (see comment in the .h file) ... 
     */

    iter
    ICodeParser::ParseArgumentListOperand (iter begin, iter end,
                                           VM::ArgumentList **rval)
    {
        /* parse argument list on the format "('argname': register[, ...])" */
        TokenLocation tl = SeekTokenStart (begin, end);
        VM::ArgumentList *al = new VM::ArgumentList();
        
        if (tl.estimate != teOpenParen)
            throw new ICodeParseException ("Expected Argument List");
        
        tl = SeekTokenStart (tl.begin + 1, end);
        while (tl.estimate == teString) {
            /* look for the argname in quotes */
            string *argName;
            begin = ParseString (tl.begin, end, &argName);

            /* look for the : */
            tl = SeekTokenStart (begin, end);
            if (tl.estimate != teColon)
                throw new ICodeParseException ("Expected colon");

            /* and now the register */
            tl = SeekTokenStart (tl.begin + 1, end);
            if (tl.estimate != teAlpha)
                throw new ICodeParseException ("Expected Register value");

            JSTypes::Register r;
            begin = ParseRegister (tl.begin, end, &r);
            /* pass 0 (null) as the "type" because it is
             * not actually used by the interpreter, only in (the current)
             * codegen (acording to rogerl.)
             */
            VM::TypedRegister tr = VM::TypedRegister(r, 0);
            VM::Argument arg = VM::Argument (tr, 0 /* XXX convert argName to a stringatom somehow */);
            
            al->push_back(arg);

            tl = SeekTokenStart (begin, end);
            /* if the next token is a comma,
             * seek to the next one and go again */
            if (tl.estimate == teComma) {
                tl = SeekTokenStart (tl.begin, end);
            }
        }

        if (tl.estimate != teCloseParen)
            throw new ICodeParseException ("Expected close paren");

        *rval = al;

        return tl.begin + 1;
    }

    iter
    ICodeParser::ParseBinaryOpOperand (iter begin, iter end,
                                       VM::BinaryOperator::BinaryOp *rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected BinaryOp");
        string *str;
        end = ParseAlpha (tl.begin, end, &str);

        for (int i = 0; keyword_binaryops[i] != 0; ++i)
            if (cmp_nocase (*str, keyword_binaryops[i], keyword_binaryops[i] +
                            strlen (keyword_binaryops[i]) + 1) == 0) {
                *rval = static_cast<VM::BinaryOperator::BinaryOp>(i);
                delete str;
                return end;
            }
        
        delete str;
        throw new ICodeParseException ("Unknown BinaryOp");
    }

    iter
    ICodeParser::ParseBoolOperand (iter begin, iter end, bool *rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected boolean value");

        return ParseBool (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseDoubleOperand (iter begin, iter end, double *rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if ((tl.estimate != teNumeric) && (tl.estimate != teMinus) &&
            (tl.estimate != tePlus))
            throw new ICodeParseException ("Expected double value");

        return ParseDouble (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseICodeModuleOperand (iter begin, iter end, string **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected ICode Module as a quoted string");
        return ParseString (tl.begin, end, rval);
    }
    
    iter
    ICodeParser::ParseJSClassOperand (iter begin, iter end, string **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSClass as a quoted string");
        return ParseString (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseJSStringOperand (iter begin, iter end, string **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSString as a quoted string");
        return ParseString (tl.begin, end, rval);
    }
    
    iter
    ICodeParser::ParseJSFunctionOperand (iter begin, iter end, string **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSFunction as a quoted string");
        return ParseString (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseJSTypeOperand (iter begin, iter end, string **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected JSType as a quoted string");
        return ParseString (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseLabelOperand (iter begin, iter end, VM::Label **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected Label Identifier or Offset keyword");

        string *str;
        begin = ParseAlpha (tl.begin, end, &str);
        
        if (cmp_nocase(*str, keyword_offset, keyword_offset +
                       strlen(keyword_offset) + 1) == 0) {
            /* got the "Offset" keyword, treat next thing as a jump offset
             * expressed as "Offset +/-N" */
            tl = SeekTokenStart (begin, end);

            if ((tl.estimate != teNumeric) && (tl.estimate != teMinus) &&
                (tl.estimate != tePlus)) 
                throw new ICodeParseException ("Expected numeric value after Offset keyword");
            int32 ofs;
            begin = ParseInt32 (tl.begin, end, &ofs);
            VM::Label *new_label = new VM::Label(0);
            new_label->mOffset = mStatementNodes.size() + ofs;
            mUnnamedLabels.push_back (new_label);
            *rval = new_label;
        } else {
            /* label expressed as "label_name", look for it in the
             * namedlabels map */
            LabelMap::const_iterator l = mNamedLabels.find(str->c_str());
            if (l != mNamedLabels.end()) {
                /* found the label, use it */
                *rval = (*l).second;
            } else {
                /* havn't seen the label definition yet, put a placeholder
                 * in the namedlabels map */
                VM::Label *new_label = new VM::Label(0);
                new_label->mOffset = VM::NotALabel;
                *rval = new_label;
                mNamedLabels[str->c_str()] = new_label;
            }       
        }
        return begin;
    }

    iter
    ICodeParser::ParseUInt32Operand (iter begin, iter end, uint32 *rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teNumeric)
            throw new ICodeParseException ("Expected UInt32 value");

        return ParseUInt32 (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseRegisterOperand (iter begin, iter end,
                                       JSTypes::Register *rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        return ParseRegister (tl.begin, end, rval);
    }

    iter
    ICodeParser::ParseStringAtomOperand (iter begin, iter end, string **rval)
    {
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw new ICodeParseException ("Expected StringAtom as a quoted string");
        return ParseString (tl.begin, end, rval);
    }

    /* "High Level" parse functions ... */
    iter ICodeParser::ParseInstruction (uint icodeID, iter begin, iter end)
    {
        iter curpos = begin;
        StatementNode *node = new StatementNode();
        node->begin = begin;
        node->icodeID = icodeID;

        fprintf (stderr, "parsing instruction %s\n", icodemap[icodeID].name);
        
        /* add the node now, so the parse*operand functions can see it */
        mStatementNodes.push_back (node);

#       define CASE_TYPE(T, C, CTYPE)                                      \
           case ot##T:                                                     \
           {                                                               \
              C rval;                                                      \
              fprintf (stderr, "parsing operand type %u\n",                \
                       static_cast<uint32>(ot##T));                        \
              node->operand[i].type = ot##T;                               \
              curpos = Parse##T##Operand (curpos, end, &rval);             \
              node->operand[i].data = CTYPE<int64>(rval);                  \
              break;                                                       \
           }
 
        for (uint i = 0; i < 4; ++i)
        {
            switch (icodemap[icodeID].otype[i])
            {
                CASE_TYPE(ArgumentList, VM::ArgumentList *, reinterpret_cast);
                CASE_TYPE(BinaryOp, VM::BinaryOperator::BinaryOp,
                          static_cast);
                CASE_TYPE(Bool, bool, static_cast);
                CASE_TYPE(Double, double, static_cast);
                CASE_TYPE(ICodeModule, string *, reinterpret_cast);
                CASE_TYPE(JSClass, string *, reinterpret_cast);
                CASE_TYPE(JSString, string *, reinterpret_cast);
                CASE_TYPE(JSFunction, string *, reinterpret_cast);
                CASE_TYPE(JSType, string *, reinterpret_cast);
                CASE_TYPE(Label, VM::Label *, reinterpret_cast);
                CASE_TYPE(UInt32, uint32, static_cast);
                CASE_TYPE(Register, JSTypes::Register, static_cast);
                CASE_TYPE(StringAtom, string *, reinterpret_cast);
                default:
                    break;                    
            }
            if (i != 3 && icodemap[icodeID].otype[i + 1] != otNone) {
                TokenLocation tl = SeekTokenStart (curpos, end);
                if (tl.estimate != teComma)
                    throw new ICodeParseException ("Expected comma");
                tl = SeekTokenStart (tl.begin + 1, end);
                curpos = tl.begin;
            }   
        }

#       undef CASE_TYPE

        return curpos;
    }

    iter
    ICodeParser::ParseStatement (iter begin, iter end)
    {
        bool isLabel = false;
        iter firstTokenEnd = end;
        TokenLocation tl = SeekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw new ICodeParseException ("Expected an alphanumeric token (like maybe an instruction or a label.)");

        for (iter curpos = tl.begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case ':':
                    isLabel = true;
                    firstTokenEnd = ++curpos;
                    goto scan_done;

                case 'a'...'z':
                case 'A'...'Z':
                case '0'...'9':
                case '_':
                    break;

                default:
                    firstTokenEnd = curpos;
                    goto scan_done;
            }
        }
    scan_done:
        
        if (isLabel) {
            /* the thing we scanned was a label...
             * ignore the trailing : */
            string label_str(tl.begin, firstTokenEnd - 1);
            /* check to see if it was already referenced... */
            LabelMap::const_iterator l = mNamedLabels.find(label_str.c_str());
            if (l == mNamedLabels.end()) {
                /* if it wasn't already referenced, add it */
                VM::Label *new_label = new VM::Label (0);
                new_label->mOffset = mStatementNodes.size();
                mNamedLabels[label_str.c_str()] = new_label;
            } else {
                /* if it was already referenced, check to see if the offset
                 * was already set */
                if ((*l).second->mOffset == VM::NotALabel) {
                    /* offset not set yet, set it and move along */
                    (*l).second->mOffset = mStatementNodes.size();
                } else {
                    /* offset was already set, this must be a dupe! */
                    throw new ICodeParseException ("Duplicate label");
                }
            }
            return firstTokenEnd;
        } else {
            /* the thing we scanned was an instruction, search the icode map
             * for a matching instruction */
            string icode_str(tl.begin, firstTokenEnd);
            for (uint i = 0; i < icodemap_size; ++i)
                if (cmp_nocase(icode_str, &icodemap[i].name[0],
                               &icodemap[i].name[0] +
                               strlen(icodemap[i].name) + 1) == 0)
                    /* if match found, parse it's operands */
                    return ParseInstruction (i, firstTokenEnd, end);
            /* otherwise, choke on it */
            throw new ICodeParseException ("Unknown ICode " + icode_str);
        }

    }

}
}
