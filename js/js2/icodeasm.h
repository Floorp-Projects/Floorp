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

#define iter string::const_iterator

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

    struct ICodeParseException {
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

    union AnyOperand {
        /* eww */
        double asDouble;
        uint32 asUInt32;
        Register asRegister;
        bool asBool;
        ArgumentList *asArgumentList;
        string *asString;
    };
        
    struct StatementNode {
        uint icodeID;
        AnyOperand operand[3];
    }       
    
    class ICodeParser 
    {
    private:
        uint mMaxRegister;
        std::vector<StatementNode *> mStatementNodes;
        std::map<string *, uint32> mLabels;

    public:
        void ParseSourceFromString (const string source);
        TokenLocation SeekTokenStart (iter begin, iter end);
        iter ParseUInt32  (iter begin, iter end, uint32 *rval);
        iter ParseDouble  (iter begin, iter end, double *rval);
        iter ParseAlpha (iter begin, iter end, string *rval);
        iter ParseString  (iter begin, iter end, string *rval);
        iter ParseStatement (iter begin, iter end);
        iter ParseInstruction (uint icodeID, iter start, iter end);

        iter ParseArgumentListOperand (iter begin, iter end, AnyOperand *o);
        iter ParseBinaryOpOperand (iter begin, iter end, AnyOperand *o);
        iter ParseBoolOperand (iter begin, iter end, AnyOperand *o);
        iter ParseDoubleOperand (iter begin, iter end, AnyOperand *o);
        iter ParseICodeModuleOperand (iter begin, iter end, AnyOperand *o);
        iter ParseJSClassOperand (iter begin, iter end, AnyOperand *o);
        iter ParseJSStringOperand (iter begin, iter end, AnyOperand *o);
        iter ParseJSFunctionOperand (iter begin, iter end, AnyOperand *o);
        iter ParseJSTypeOperand (iter begin, iter end, AnyOperand *o);
        iter ParseLabelOperand (iter begin, iter end, AnyOperand *o);
        iter ParseUInt32Operand (iter begin, iter end, AnyOperand *o);
        iter ParseRegisterOperand (iter begin, iter end, AnyOperand *o);
        iter ParseStringAtomOperand (iter begin, iter end, AnyOperand *o);
    };
    
}
}
