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

#include <string>
#include <iterator>

#include "vmtypes.h"
#include "jstypes.h"

#define iter string::const_iterator

namespace JavaScript {
namespace ICodeASM {
    enum TokenEstimation {
        /* guess at tokentype, based on first character of token */
        teEOF,
        teUnknown,
        teIllegal,
        teComma,
        teMinus,
        teNumeric,
        tePlus,
        teAlpha,
        teString,
        teNotARegister
    };

/*
    enum TokenType {
        // verified token type
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
*/

    enum OperandType {
        otNone = 0,
        otArgumentList,
        otBinaryOp,
        otBool,
        otDouble,
        otICodeModule,
        otJSClass,
        otJSString,
        otJSFunction,
        otJSType,
        otLabel,
        otUInt32,
        otRegister,
        otStringAtom
    };

    struct ICodeParseException {
        ICodeParseException (string aMsg, iter aPos = 0)
            : msg(aMsg), pos(aPos) {}
        string msg;
        iter pos;
    };
        
    struct TokenLocation {
        TokenLocation () : begin(0), estimate(teIllegal) /*,
                           type(ttUndetermined) */ {}
        iter begin;
        TokenEstimation estimate;
//        TokenType type;
    };

    struct AnyOperand {
        OperandType type;
        int64 data;
        /*void *data;*/
    };
    
    struct StatementNode {
        iter begin;
        uint icodeID;
        AnyOperand operand[3];
    };
    
    class ICodeParser 
    {
    private:
        uint mMaxRegister;
        std::vector<StatementNode *> mStatementNodes;
        VM::LabelList mUnnamedLabels;
        typedef std::map<const char *, VM::Label*> LabelMap;
        LabelMap mNamedLabels;

    public:
        void ParseSourceFromString (const string &source);

        /* locate the beginning of the next token and take a guess at what it
         * might be */
        TokenLocation SeekTokenStart (iter begin, iter end);

        /* general purpose parse functions; |begin| is expected to point
         * at the start of the token to be processed (eg, these routines
         * don't call |SeekTokenStart|, and (currently, this might change) no
         * initial check is done to ensure that |begin| != |end|.
         */
        iter ParseAlpha (iter begin, iter end, string **rval);
        iter ParseBool (iter begin, iter end, bool *rval);
        iter ParseDouble (iter begin, iter end, double *rval);
        iter ParseInt32 (iter begin, iter end, int32 *rval);
        iter ParseRegister (iter begin, iter end, JSTypes::Register *rval);
        iter ParseString (iter begin, iter end, string **rval);
        iter ParseUInt32 (iter begin, iter end, uint32 *rval);

        /* operand parse functions;  These functions take care of finding
         * the start of the token with |SeekTokenStart|, and checking the
         * "estimation" (explicit checking takes care of |begin| == |end|,
         * aka EOF, because EOF is a token estimate.)  Once the start of the
         * token is found, and it is the expected type, the actual parsing is
         * carried out by one of the general purpose parse functions.
         */
        iter ParseArgumentListOperand (iter begin, iter end,
                                       VM::ArgumentList **rval);
        iter ParseBinaryOpOperand (iter begin, iter end,
                                   VM::BinaryOperator::BinaryOp *rval);
        iter ParseBoolOperand (iter begin, iter end, bool *rval);
        iter ParseDoubleOperand (iter begin, iter end, double *rval);
        iter ParseICodeModuleOperand (iter begin, iter end, string **rval);
        iter ParseJSClassOperand (iter begin, iter end, string **rval);
        iter ParseJSStringOperand (iter begin, iter end, string **rval);
        iter ParseJSFunctionOperand (iter begin, iter end, string **rval);
        iter ParseJSTypeOperand (iter begin, iter end, string **rval);
        iter ParseLabelOperand (iter begin, iter end, VM::Label **rval);
        iter ParseUInt32Operand (iter begin, iter end, uint32 *rval);
        iter ParseRegisterOperand (iter begin, iter end,
                                   JSTypes::Register *rval);
        iter ParseStringAtomOperand (iter begin, iter end, string **rval);

        /* "high level" parse functions */
        iter ParseInstruction (uint icodeID, iter start, iter end);
        iter ParseStatement (iter begin, iter end);

    };
    
}
}
