
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

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <assert.h>
#include <map>
#include <list>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "jslong.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"


namespace JavaScript {
namespace MetaData {


    js2val JS2Metadata::readEvalString(const String &str, const String& fileName)
    {
        js2val result = JS2VAL_VOID;

        Arena a;
        Pragma::Flags flags = Pragma::es4;
        Parser p(world, a, flags, str, fileName);
        CompilationData *oldData = NULL;
        try {
            StmtNode *parsedStatements = p.parseProgram();
            ASSERT(p.lexer.peek(true).hasKind(Token::end));
            if (showTrees)
            {
                PrettyPrinter f(stdOut, 80);
                {
                    PrettyPrinter::Block b(f, 2);
                    f << "Program =";
                    f.linearBreak(1);
                    StmtNode::printStatements(f, parsedStatements);
                }
                f.end();
                stdOut << '\n';
            }
            if (parsedStatements) {
                oldData = startCompilationUnit(NULL, str, fileName);
                ValidateStmtList(parsedStatements);
                result = ExecuteStmtList(RunPhase, parsedStatements);
            }
        }
        catch (Exception &x) {
            if (oldData)
                restoreCompilationUnit(oldData);
            throw x;
        }
        if (oldData)
            restoreCompilationUnit(oldData);
        return result;
    }

    js2val JS2Metadata::readEvalFile(const String& fileName)
    {
        String buffer;
        int ch;

        js2val result = JS2VAL_VOID;

        std::string str(fileName.length(), char());
        std::transform(fileName.begin(), fileName.end(), str.begin(), narrow);
        FILE* f = fopen(str.c_str(), "r");
        if (f) {
            while ((ch = getc(f)) != EOF)
                buffer += static_cast<char>(ch);
            fclose(f);
            result = readEvalString(buffer, fileName);
        }
        return result;
    }

    js2val JS2Metadata::readEvalFile(const char *fileName)
    {
        String buffer;
        int ch;

        js2val result = JS2VAL_VOID;

        FILE* f = fopen(fileName, "r");
        if (f) {
            while ((ch = getc(f)) != EOF)
                buffer += static_cast<char>(ch);
            fclose(f);
            result = readEvalString(buffer, widenCString(fileName));
        }
        return result;
    }

    /*
     * Evaluate the linked list of statement nodes beginning at 'p' 
     * (generate bytecode and then execute that bytecode
     */
    js2val JS2Metadata::ExecuteStmtList(Phase phase, StmtNode *p)
    {
        size_t lastPos = p->pos;
        while (p) {
            SetupStmt(env, phase, p);
            lastPos = p->pos;
            p = p->next;
        }
        bCon->emitOp(eReturnVoid, lastPos);
        uint8 *savePC = engine->pc;
        engine->pc = NULL;
        js2val retval = engine->interpret(phase, bCon);
        engine->pc = savePC;
        return retval;
    }

    /*
     * Evaluate an expression 'p' AND execute the associated bytecode
     */
    js2val JS2Metadata::EvalExpression(Environment *env, Phase phase, ExprNode *p)
    {
        js2val retval;
        uint8 *savePC = NULL;
        JS2Class *exprType;

        CompilationData *oldData = startCompilationUnit(NULL, bCon->mSource, bCon->mSourceLocation);
        try {
            Reference *r = SetupExprNode(env, phase, p, &exprType);
            if (r) r->emitReadBytecode(bCon, p->pos);
            bCon->emitOp(eReturn, p->pos);
            savePC = engine->pc;
            engine->pc = NULL;
            retval = engine->interpret(phase, bCon);
        }
        catch (Exception &x) {
            engine->pc = savePC;
            restoreCompilationUnit(oldData);
            throw x;
        }
        engine->pc = savePC;
        restoreCompilationUnit(oldData);
        return retval;
    }


    // Execute an expression and return the result, which must be a type
    JS2Class *JS2Metadata::EvalTypeExpression(Environment *env, Phase phase, ExprNode *p)
    {
        js2val retval = EvalExpression(env, phase, p);
        if (JS2VAL_IS_PRIMITIVE(retval))
            reportError(Exception::badValueError, "Type expected", p->pos);
        JS2Object *obj = JS2VAL_TO_OBJECT(retval);
        if (obj->kind != ClassKind)
            reportError(Exception::badValueError, "Type expected", p->pos);
        return checked_cast<JS2Class *>(obj);        
    }


}; // namespace MetaData
}; // namespace Javascript



