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
#include "utilities.h"
#include "lexutils.h"
#include "exception.h"

namespace JavaScript {
namespace ICodeASM {
    using namespace LexUtils;
    
    static char *keyword_offset = "offset";
    static char *keyword_binaryops[] = {"add", "subtract", "multiply", "divide",
                                        "remainder", "leftshift", "rightshift",
                                        "logicalrightshift", "bitwiseor",
                                        "bitwisexor", "bitwiseand", "less",
                                        "lessorequal", "equal", "identical", 0};

    void
    ICodeParser::parseSourceFromString (const string8 &source)
    {
        uint statementNo = 0;
        string8_citer begin = source.begin();
        string8_citer end = source.end();

        mInstructions = new VM::InstructionStream();
        mMaxRegister = 0;
        mInstructionCount = 0;
        mLabels.clear();
        mNamedLabels.clear();
        
        while (begin < end)
        {
            try
            {
                ++statementNo;
                begin = parseNextStatement (begin, end);
            }
            catch (JSException &e)
            {
                string8 etext;
                e.toString8(etext);
                
                fprintf (stderr, "%s at statement %u\n",
                         etext.c_str(), statementNo);
                return;
            }
        }

        if (begin > end)
            NOT_REACHED ("Overran source buffer!");
    }

    /**********************************************************************
     * operand parse functions (see comment in the .h file) ... 
     */

    string8_citer
    ICodeParser::parseArgumentListOperand (string8_citer begin,
                                           string8_citer end,
                                           VM::ArgumentList **rval)
    {
        /* parse argument list on the format "(['argname': ]register[, ...])" */
        TokenLocation tl = seekTokenStart (begin, end);
        VM::ArgumentList *al = new VM::ArgumentList();
        
        if (tl.estimate != teOpenParen)
            throw JSParseException (eidExpectArgList);
        
        tl = seekTokenStart (tl.begin + 1, end);
        while (tl.estimate == teString || tl.estimate == teAlpha) {
            string *argName = 0;

            if (tl.estimate == teString) {
                /* look for the argname in quotes */
                begin = lexString8 (tl.begin, end, &argName);

                /* look for the : */
                tl = seekTokenStart (begin, end);
                if (tl.estimate != teColon)
                    throw JSParseException (eidExpectColon);

                /* and now the register */
                tl = seekTokenStart (tl.begin + 1, end);
            }

            if (tl.estimate != teAlpha)
                throw JSParseException (eidExpectRegister);

            JSTypes::Register r;
            begin = lexRegister (tl.begin, end, &r);
            if (r != VM::NotARegister && r > mMaxRegister)
                mMaxRegister = r;

            /* pass 0 (null) as the "type" because it is
             * not actually used by the interpreter, only in (the current)
             * codegen (acording to rogerl.)
             */
            VM::TypedRegister tr = VM::TypedRegister(r, 0);

            StringAtom *sap = 0;
            if (argName) {
                sap = &(mCx->getWorld().identifiers[argName->c_str()]);
                delete argName;
            }
            VM::Argument arg = VM::Argument (tr, sap);
            
            al->push_back(arg);

            tl = seekTokenStart (begin, end);
            /* if the next token is a comma,
             * seek to the next one and go again */
            if (tl.estimate == teComma) {
                tl = seekTokenStart (tl.begin + 1, end);
            }
        }

        if (tl.estimate != teCloseParen)
            throw JSParseException (eidExpectCloseParen);

        *rval = al;

        return tl.begin + 1;
    }

    string8_citer
    ICodeParser::parseBinaryOpOperand (string8_citer begin, string8_citer end,
                                       VM::BinaryOperator::BinaryOp *rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw JSParseException (eidExpectBinaryOp);
        string8 *str;
        end = lexAlpha (tl.begin, end, &str);

        for (int i = 0; keyword_binaryops[i] != 0; ++i)
            if (cmp_nocase (*str, keyword_binaryops[i], keyword_binaryops[i] +
                            strlen (keyword_binaryops[i]) + 1) == 0) {
                *rval = static_cast<VM::BinaryOperator::BinaryOp>(i);
                delete str;
                return end;
            }
        
        delete str;
        throw JSParseException (eidUnknownBinaryOp);
    }

    string8_citer
    ICodeParser::parseBoolOperand (string8_citer begin, string8_citer end,
                                   bool *rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw JSParseException (eidExpectBool);

        return lexBool (tl.begin, end, rval);
    }

    string8_citer
    ICodeParser::parseDoubleOperand (string8_citer begin, string8_citer end,
                                     double *rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if ((tl.estimate != teNumeric) && (tl.estimate != teMinus) &&
            (tl.estimate != tePlus))
            throw JSParseException (eidExpectDouble);

        return lexDouble (tl.begin, end, rval);
    }

    string8_citer
    ICodeParser::parseICodeModuleOperand (string8_citer /*begin*/,
                                          string8_citer end, string ** /*rval*/)
    {
        NOT_REACHED ("ICode modules are hard, lets go shopping.");
        return end;
    }
    
    string8_citer
    ICodeParser::parseJSClassOperand (string8_citer /*begin*/,
                                      string8_citer end, string ** /*rval*/)
    {
        NOT_REACHED ("JSClasses are hard, lets go shopping.");
        return end;
    }

    string8_citer
    ICodeParser::parseJSStringOperand (string8_citer begin, string8_citer end,
                                       JSTypes::JSString **rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw JSParseException (eidExpectString);
        string8 *str;
        end = lexString8 (tl.begin, end, &str);
        *rval = new JSTypes::JSString (str->c_str());
        delete str;
        return end;
    }
    
    string8_citer
    ICodeParser::parseJSFunctionOperand (string8_citer /*begin*/,
                                         string8_citer end,
                                         string ** /*rval*/)
    {
        NOT_REACHED ("JSFunctions are hard, lets go shopping.");
        return end;
    }

    string8_citer
    ICodeParser::parseJSTypeOperand (string8_citer begin, string8_citer end,
                                     JSTypes::JSType **rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw JSParseException (eidExpectString);

        string8 *str;
        end = lexString8 (tl.begin, end, &str);
        StringAtom &typename_atom = mCx->getWorld().identifiers[str->c_str()];
        delete str;
        JSTypes::JSValue jsv = 
            mCx->getGlobalObject()->getVariable(typename_atom);
        if (jsv.isType())
            *rval = jsv.type;
        else
            *rval = &(JSTypes::Any_Type);

        return end;
    }

    string8_citer
    ICodeParser::parseLabelOperand (string8_citer begin, string8_citer end,
                                    VM::Label **rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teAlpha)
            throw JSParseException (eidExpectLabel);

        string8 *str;
        begin = lexAlpha (tl.begin, end, &str);
        
        if (cmp_nocase(*str, keyword_offset, keyword_offset +
                       strlen(keyword_offset) + 1) == 0) {
            delete str;
            /* got the "Offset" keyword, treat next thing as a jump offset
             * expressed as "Offset +/-N" */
            tl = seekTokenStart (begin, end);

            if (tl.estimate != teNumeric)
                throw JSParseException (eidExpectUInt32);
            
            uint32 ofs;
            begin = lexUInt32 (tl.begin, end, &ofs);
            VM::Label *new_label = new VM::Label(mInstructions);
            new_label->mOffset = ofs;
            mLabels.push_back (new_label);
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
                VM::Label *new_label = new VM::Label(mInstructions);
                new_label->mOffset = VM::NotALabel;
                *rval = new_label;
                mNamedLabels[str->c_str()] = new_label;
                mLabels.push_back (new_label);
            }       
            delete str;
        }
        return begin;
    }

    string8_citer
    ICodeParser::parseUInt32Operand (string8_citer begin,
                                     string8_citer end, uint32 *rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teNumeric)
            throw JSParseException (eidExpectUInt32);

        return lexUInt32 (tl.begin, end, rval);
    }

    string8_citer
    ICodeParser::parseRegisterOperand (string8_citer begin, string8_citer end,
                                       JSTypes::Register *rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        end = lexRegister (tl.begin, end, rval);
        if (*rval != VM::NotARegister && *rval > mMaxRegister)
            mMaxRegister = *rval;

        return end;    
    }

    string8_citer
    ICodeParser::parseStringAtomOperand (string8_citer begin, string8_citer end,
                                         StringAtom **rval)
    {
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate != teString)
            throw JSParseException (eidExpectString);
        string8 *str;
        end = lexString8 (tl.begin, end, &str);
        *rval = &(mCx->getWorld().identifiers[str->c_str()]);
        delete str;
        return end;
    }

    /* "High Level" parse functions ... */
    string8_citer
    ICodeParser::parseInstruction (uint icodeID, string8_citer begin,
                                   string8_citer end)
    {
        string8_citer curpos = begin;
        StatementNode node;
        node.icodeID = icodeID;

#       define CASE_TYPE(T, C, CTYPE)                                      \
           case ot##T:                                                     \
           {                                                               \
              C rval;                                                      \
              node.operand[i].type = ot##T;                                \
              curpos = parse##T##Operand (curpos, end, &rval);             \
              node.operand[i].data = CTYPE<int64>(rval);                   \
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
                CASE_TYPE(JSString, JSTypes::JSString *, reinterpret_cast);
                CASE_TYPE(JSFunction, string *, reinterpret_cast);
                CASE_TYPE(JSType, JSTypes::JSType *, reinterpret_cast);
                CASE_TYPE(Label, VM::Label *, reinterpret_cast);
                CASE_TYPE(UInt32, uint32, static_cast);
                CASE_TYPE(Register, JSTypes::Register, static_cast);
                CASE_TYPE(StringAtom, StringAtom *, reinterpret_cast);
                default:
                    node.operand[i].type = otNone;
                    break;                    
            }
            if (i != 3 && icodemap[icodeID].otype[i + 1] != otNone) {
                /* if the instruction has more arguments, eat a comma and
                 * locate the next token */
                TokenLocation tl = seekTokenStart (curpos, end);
                if (tl.estimate != teComma)
                    throw JSParseException (eidExpectComma);
                tl = seekTokenStart (tl.begin + 1, end);
                curpos = tl.begin;
            }   
        }

#       undef CASE_TYPE

        mInstructions->push_back (InstructionFromNode(&node));
        ++mInstructionCount;
        
        TokenLocation tl = seekTokenStart (curpos, end);
        if (tl.estimate != teNewline && tl.estimate != teEOF)
            throw JSParseException (eidExpectNewline);
        
        if (tl.estimate == teEOF)
            return tl.begin;
        else
            return tl.begin + 1;
    }

    string8_citer
    ICodeParser::parseNextStatement (string8_citer begin, string8_citer end)
    {
        bool isLabel = false;
        string8_citer firstTokenEnd = end;
        TokenLocation tl = seekTokenStart (begin, end);

        if (tl.estimate == teNewline) {
            /* empty statement, do nothing */
            return tl.begin + 1;
        }

        if (tl.estimate != teAlpha)
            throw JSParseException (eidExpectIdentifier);

        for (string8_citer curpos = tl.begin; curpos < end; ++curpos) {
            switch (*curpos)
            {
                case ':':
                    isLabel = true;
                    firstTokenEnd = ++curpos;
                    goto scan_done;

                default:
                    if (!IS_ALPHA(*curpos)) {
                        firstTokenEnd = curpos;
                        goto scan_done;
                    }
            }
        }
    scan_done:
        
        if (isLabel) {
            /* the thing we scanned was a label...
             * ignore the trailing : */
            string8 label_str(tl.begin, firstTokenEnd - 1);
            /* check to see if it was already referenced... */
            LabelMap::const_iterator l = mNamedLabels.find(label_str.c_str());
            if (l == mNamedLabels.end()) {
                /* if it wasn't already referenced, add it */
                VM::Label *new_label = new VM::Label (mInstructions);
                new_label->mOffset = mInstructionCount;
                mNamedLabels[label_str.c_str()] = new_label;
                mLabels.push_back(new_label);
            } else {
                /* if it was already referenced, check to see if the offset
                 * was already set */
                if ((*l).second->mOffset == VM::NotALabel) {
                    /* offset not set yet, set it and move along */
                    (*l).second->mOffset = mInstructionCount;
                } else {
                    /* offset was already set, this must be a dupe! */
                    throw JSParseException (eidDuplicateLabel);
                }
            }
        } else {
            /* the thing we scanned was an instruction, search the icode map
             * for a matching instruction */
            string8 icode_str(tl.begin, firstTokenEnd);
            for (uint i = 0; i < icodemap_size; ++i)
                if (cmp_nocase(icode_str, &icodemap[i].name[0],
                               &icodemap[i].name[0] +
                               strlen(icodemap[i].name) + 1) == 0)
                    /* if match found, parse it's operands */
                    return parseInstruction (i, firstTokenEnd, end);
            /* otherwise, choke on it */
            throw JSParseException (eidUnknownICode);
        }
        return firstTokenEnd;
    }
    
}
}
