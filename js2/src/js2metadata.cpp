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
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#pragma warning(disable: 4711)
#pragma warning(disable: 4710)
#endif


#include <algorithm>


#include "js2metadata.h"



namespace JavaScript {
namespace MetaData {

/************************************************************************************
 *
 *  Statements and statement lists
 *
 ************************************************************************************/


    /*
     * Validate the linked list of statement nodes beginning at 'p'
     */
    void JS2Metadata::ValidateStmtList(Context *cxt, Environment *env, StmtNode *p) {
        while (p) {
            ValidateStmt(cxt, env, p);
            p = p->next;
        }
    }

    /*
     * Validate an individual statement 'p', including it's children
     */
    void JS2Metadata::ValidateStmt(Context *cxt, Environment *env, StmtNode *p) {
        switch (p->getKind()) {
        case StmtNode::block:
        case StmtNode::group:
            {
                BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
                ValidateStmtList(cxt, env, b->statements);
            }
            break;
        case StmtNode::label:
            {
                LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                ValidateStmt(cxt, env, l->stmt);
            }
            break;
        case StmtNode::Var:
        case StmtNode::Const:
            {
                Attribute *attr = NULL;
                VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);

                ValidateAttributeExpression(cxt, env, vs->attributes);
                if (vs->attributes)
                    attr = EvalAttributeExpression(env, CompilePhase, vs->attributes);
                
                
                VariableBinding *v = vs->bindings;
                Frame *regionalFrame = env->getRegionalFrame();
                while (v)  {
                    ValidateTypeExpression(v->type);

                    if (cxt->strict && ((regionalFrame->kind == Frame::GlobalObject)
                                        || (regionalFrame->kind == Frame::Function))
                                    && (p->getKind() == StmtNode::Var)  // !immutable
                                    && (vs->attributes == NULL)
                                    && (v->type == NULL)) {
                        defineHoistedVar(env, *v->name, p);
                    }
                    else {
                    }

                    v = v->next;
                }
                if (attr)
                    delete attr;
            }
            break;
        case StmtNode::expression:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                ValidateExpression(cxt, env, e->expr);
            }
            break;
        }   // switch (p->getKind())
    }

    /*
     * Evaluate the linked list of statement nodes beginning at 'p'
     */
    jsval JS2Metadata::EvalStmtList(Environment *env, Phase phase, StmtNode *p) {
        jsval retval = JSVAL_VOID;
        while (p) {
            retval = EvalStmt(env, phase, p);
            p = p->next;
        }
        return retval;
    }

    /*
     * Evaluate an individual statement 'p', including it's children
     */
    jsval JS2Metadata::EvalStmt(Environment *env, Phase phase, StmtNode *p) 
    {
        jsval retval = JSVAL_VOID;
        switch (p->getKind()) {
        case StmtNode::block:
        case StmtNode::group:
            {
                BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
                retval = EvalStmtList(env, phase, b->statements);
            }
            break;
        case StmtNode::label:
            {
                LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                retval = EvalStmt(env, phase, l->stmt);
            }
            break;
        case StmtNode::Var:
        case StmtNode::Const:
            {
                VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);
                
                VariableBinding *v = vs->bindings;
                while (v)  {

                    v = v->next;
                }
            }
            break;
        case StmtNode::expression:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                retval = EvalExpression(env, phase, e->expr);
                if (JSVAL_IS_OBJECT(retval)) {
                    JSObject *obj = JSVAL_TO_OBJECT(retval);
                }

            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        }   // switch (p->getKind())
        return retval;
    }


/************************************************************************************
 *
 *  Attributes
 *
 ************************************************************************************/

    //
    // Validate the Attribute expression at p
    // An attribute expression can only be a list of 'juxtaposed' attribute elements
    //
    // Note : "AttributeExpression" here is a different beast than in the spec. - here it
    // describes the entire attribute part of a directive, not just the qualified identifier
    // and other references encountered in an attribute.
    //
    void JS2Metadata::ValidateAttributeExpression(Context *cxt, Environment *env, ExprNode *p)
    {
        switch (p->getKind()) {
        case ExprNode::boolean:
            break;
        case ExprNode::juxtapose:
            {
                BinaryExprNode *j = checked_cast<BinaryExprNode *>(p);
                ValidateAttributeExpression(cxt, env, j->op1);
                ValidateAttributeExpression(cxt, env, j->op2);
            }
            break;
        case ExprNode::identifier:
            {
                const StringAtom &name = checked_cast<IdentifierExprNode *>(p)->name;
                CompoundAttribute *ca = NULL;
                switch (name.tokenKind) {
                case Token::Public:
                    return;
                case Token::Abstract:
                    return;
                case Token::Final:
                    return;
                case Token::Private:
                    {
                        JS2Class *c = env->getEnclosingClass();
                        if (!c)
                            reportError(Exception::syntaxError, "Private can only be used inside a class definition", p->pos);
                    }
                    return;
                case Token::Static:
                    return;
                }
                // fall thru to handle as generic expression element...
            }            
        default:
            {
                ValidateExpression(cxt, env, p);
            }
            break;

        } // switch (p->getKind())
    }

    // Evaluate the Attribute expression rooted at p.
    // An attribute expression can only be a list of 'juxtaposed' attribute elements
    Attribute *JS2Metadata::EvalAttributeExpression(Environment *env, Phase phase, ExprNode *p)
    {
        switch (p->getKind()) {
        case ExprNode::boolean:
            if (checked_cast<BooleanExprNode *>(p)->value)
                return new TrueAttribute();
            else
                return new FalseAttribute();
        case ExprNode::juxtapose:
            {
                BinaryExprNode *j = checked_cast<BinaryExprNode *>(p);
                Attribute *a = EvalAttributeExpression(env, phase, j->op1);
                if (a && (a->kind == Attribute::FalseKind))
                    return a;
                Attribute *b = EvalAttributeExpression(env, phase, j->op2);
                try {
                    return Attribute::combineAttributes(a, b);
                }
                catch (char *err) {
                    reportError(Exception::badValueError, err, p->pos);
                }
            }
            break;

        case ExprNode::identifier:
            {
                const StringAtom &name = checked_cast<IdentifierExprNode *>(p)->name;
                CompoundAttribute *ca = NULL;
                switch (name.tokenKind) {
                case Token::Public:
                    return publicNamespace;
                case Token::Abstract:
                    ca = new CompoundAttribute();
                    ca->memberMod = Attribute::Abstract;
                    return ca;
                case Token::Final:
                    ca = new CompoundAttribute();
                    ca->memberMod = Attribute::Final;
                    return ca;
                case Token::Private:
                    {
                        JS2Class *c = env->getEnclosingClass();
                        return c->privateNamespace;
                    }
                case Token::Static:
                    ca = new CompoundAttribute();
                    ca->memberMod = Attribute::Static;
                    return ca;
                }
            }            
            // fall thru to execute a readReference on the identifier...
        default:
            {
                // anything else (just references of one kind or another) must
                // be compile-time constant values that resolve to namespaces
                jsval av = EvalExpression(env, CompilePhase, p);
                if (JSVAL_IS_NULL(av) || JSVAL_IS_VOID(av) || !JSVAL_IS_OBJECT(av))
                    reportError(Exception::badValueError, "Namespace expected in attribute", p->pos);
                JSObject *obj = JSVAL_TO_OBJECT(av);

            }
            break;

        } // switch (p->getKind())
        return NULL;
    }

    // Combine attributes a & b, reporting errors for incompatibilities
    // a is not false
    Attribute *Attribute::combineAttributes(Attribute *a, Attribute *b)
    {
        if (b && (b->kind == FalseKind)) {
            if (a) delete a;
            return b;
        }
        if (!a || (a->kind == TrueKind)) {
            if (a) delete a;
            return b;
        }
        if (!b || (b->kind == TrueKind)) {
            if (b) delete b;
            return a;
        }
        if (a->kind == NamespaceKind) {
            if (a == b) {
                delete b;
                return a;
            }
            Namespace *na = checked_cast<Namespace *>(a);
            if (b->kind == NamespaceKind) {
                Namespace *nb = checked_cast<Namespace *>(b);
                CompoundAttribute *c = new CompoundAttribute();
                c->addNamespace(na);
                c->addNamespace(nb);
                delete a;
                delete b;
                return (Attribute *)c;
            }
            else {
                ASSERT(b->kind == CompoundKind);
                CompoundAttribute *cb = checked_cast<CompoundAttribute *>(b);
                cb->addNamespace(na);
                delete a;
                return b;
            }
        }
        else {
            // Both a and b are compound attributes. Ensure that they have no conflicting contents.
            ASSERT((a->kind == CompoundKind) && (b->kind == CompoundKind));
            CompoundAttribute *ca = checked_cast<CompoundAttribute *>(a);
            CompoundAttribute *cb = checked_cast<CompoundAttribute *>(b);
            if ((ca->memberMod != NoModifier) && (cb->memberMod != NoModifier) && (ca->memberMod != cb->memberMod))
                throw("Illegal combination of member modifier attributes");
            if ((ca->overrideMod != NoOverride) && (cb->overrideMod != NoOverride) && (ca->overrideMod != cb->overrideMod))
                throw("Illegal combination of override attributes");
            for (NamespaceListIterator i = cb->namespaces->begin(), end = cb->namespaces->end(); (i != end); i++)
                ca->addNamespace(*i);
            ca->xplicit |= cb->xplicit;
            ca->dynamic |= cb->dynamic;
            if (ca->memberMod == NoModifier)
                ca->memberMod = cb->memberMod;
            if (ca->overrideMod == NoModifier)
                ca->overrideMod = cb->overrideMod;
            ca->prototype |= cb->prototype;
            ca->unused |= cb->unused;
            delete b;
            return a;
        }
    }

    // add the namespace to our list, but only if it's not there already
    void CompoundAttribute::addNamespace(Namespace *n)
    {
        for (NamespaceListIterator i = namespaces->begin(), end = namespaces->end(); (i != end); i++)
            if (*i == n)
                return;
        namespaces->push_back(n);
    }

    CompoundAttribute::CompoundAttribute() : Attribute(CompoundKind),
            namespaces(NULL), xplicit(false), dynamic(false), memberMod(NoModifier), 
            overrideMod(NoOverride), prototype(false), unused(false) 
    { 
    }



/************************************************************************************
 *
 *  Expressions
 *
 ************************************************************************************/

    // Validate the entire expression rooted at p
    void JS2Metadata::ValidateExpression(Context *cxt, Environment *env, ExprNode *p)
    {
        switch (p->getKind()) {
        case ExprNode::identifier:
            {
                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
                // Just cloning the context here, rather than constructing a multiname
                // since this way is cheaper. 
                // XXX - Additionally we can reuse pointers to duplicate
                // contexts rather than having to have many of them.
                i->cxt = new Context(cxt);
            }
            break;
        } // switch (p->getKind())
    }



    /*
     * Evaluate the expression rooted at p.
     * Works by generating a JS string that represents the expression and then using
     * the JS interpreter to execute that string against the current environment. The
     * result is the value of this expression.
     *
     */
    jsval JS2Metadata::EvalExpression(Environment *env, Phase phase, ExprNode *p)
    {
        String s = EvalExprNode(env, phase, p);
        return execute(&s);
    }


    String JS2Metadata::EvalExprNode(Environment *env, Phase phase, ExprNode *p)
    {
        switch (p->getKind()) {
        case ExprNode::index:
            {
            }
            break;

/*

  pause

  char*

    [FullPostfixExpression .PostfixExpressionOrSuper [no line break] ++] do
    if phase = compile then throw compileExpressionError end if;
    r: OBJORREFOPTIONALLIMIT .Eval[PostfixExpressionOrSuper](env, phase);
    a: OBJOPTIONALLIMIT .readRefWithLimit(r, phase);
    b: OBJECT .unaryDispatch(incrementTable, null, a, ARGUMENTLIST <<<<positional: [], named: {} >>>, phase);
    writeReference(r, b, phase);
    return getObject(a);

            
                    
*/
        case ExprNode::add:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                s = EvalExprNode(b->op1);
                s += " + ";
                s += EvalExprNode(b->op2);
            }
            break;

        case ExprNode::postIncrement:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                s = EvalExprNode(env, phase, u->op);
                s += "++";

            }
            break;


        case ExprNode::identifier:
            {
                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
                Multiname *mn = new Multiname();
                for (NamespaceListIterator nli = i->cxt->openNamespaces.begin(), end = i->cxt->openNamespaces.end();
                            (nli != end); nli++)
                    mn->push_back(new QualifiedName(*nli, i->name));
                return OBJECT_TO_JSVAL(new LexicalReference(mn, env, i->cxt->strict));
            }
            break;
        case ExprNode::boolean:
            if (checked_cast<BooleanExprNode *>(p)->value) 
                return JSVAL_TRUE;
            else 
                return JSVAL_FALSE;
        default:
            NOT_REACHED("Not Yet Implemented");
        }
        return JSVAL_VOID;
    }

    void JS2Metadata::ValidateTypeExpression(ExprNode *e)
    {
    }

/************************************************************************************
 *
 *  Environment
 *
 ************************************************************************************/

    // If env is from within a class's body, getEnclosingClass(env) returns the 
    // innermost such class; otherwise, it returns none.
    JS2Class *Environment::getEnclosingClass()
    {
        Frame *pf = firstFrame;
        while (pf && (pf->kind != Frame::Class))
            pf = pf->nextFrame;
        return checked_cast<JS2Class *>(pf);
    }

    // returns the most specific regional frame.
    Frame *Environment::getRegionalFrame()
    {
        Frame *pf = firstFrame;
        Frame *prev = NULL;
        while (pf->kind == Frame::Block) { 
            prev = pf;
            pf = pf->nextFrame;
        }
        if (pf->nextFrame && (pf->kind == Frame::Class))
            pf = prev;
        return pf;
    }

/************************************************************************************
 *
 *  Context
 *
 ************************************************************************************/

    Context::Context(Context *cxt) : strict(cxt->strict), openNamespaces(cxt->openNamespaces)
    {
    }


/************************************************************************************
 *
 *  JS2Metadata
 *
 ************************************************************************************/

    // Define a hoisted var in the current frame (either Global or a Function)
    void JS2Metadata::defineHoistedVar(Environment *env, const StringAtom &id, StmtNode *p)
    {
        QualifiedName qName(publicNamespace, id);
        Frame *regionalFrame = env->getRegionalFrame();
        ASSERT((env->getTopFrame()->kind == Frame::GlobalObject) || (env->getTopFrame()->kind == Frame::Function));
    
        // run through all the existing bindings, both read and write, to see if this
        // variable already exists.
        StaticBindingIterator b, end;
        bool existing = false;
        for (b = regionalFrame->staticReadBindings.lower_bound(id),
                end = regionalFrame->staticReadBindings.upper_bound(id); (b != end); b++) {
            if (b->second->qname == qName) {
                if (b->second->content->kind != StaticMember::HoistedVariable)
                    reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
                else {
                    existing = true;
                    break;
                }
            }
        }
        for (b = regionalFrame->staticWriteBindings.lower_bound(id),
                end = regionalFrame->staticWriteBindings.upper_bound(id); (b != end); b++) {
            if (b->second->qname == qName) {
                if (b->second->content->kind != StaticMember::HoistedVariable)
                    reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
                else {
                    existing = true;
                    break;
                }
            }
        }
        if (!existing) {
            if (regionalFrame->kind == Frame::GlobalObject) {
                GlobalObject *gObj = checked_cast<GlobalObject *>(regionalFrame);
                DynamicPropertyIterator dp = gObj->dynamicProperties.find(id);
                if (dp != gObj->dynamicProperties.end())
                    reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
            }
            // XXX ok to use same binding in read & write maps?
            StaticBinding *sb = new StaticBinding(qName, new HoistedVar());
            const StaticBindingMap::value_type e(id, sb);

            // XXX ok to use same value_type in different multimaps?
            regionalFrame->staticReadBindings.insert(e);
            regionalFrame->staticWriteBindings.insert(e);
        }
        //else A hoisted binding of the same var already exists, so there is no need to create another one
    }

    JS2Metadata::JS2Metadata(World &world) :
        publicNamespace(new Namespace(world.identifiers["public"]))
    {
        initializeMonkey();
    }


    /*
     * Throw an exception of the specified kind, indicating the position 'pos' and
     * attaching the given message. If 'arg' is specified, replace {0} in the message
     * with the argument value. [This is intended to be widened into a more complete
     * argument handling scheme].
     */
    void JS2Metadata::reportError(Exception::Kind kind, char *message, size_t pos, const char *arg)
    {
        const char16 *lineBegin;
        const char16 *lineEnd;
        String x = widenCString(message);
        if (arg) {
            // XXX handle multiple occurences and extend to {1} etc.
            uint32 a = x.find(widenCString("{0}"));
            x.replace(a, 3, widenCString(arg));
        }
        uint32 lineNum = mParser->lexer.reader.posToLineNum(pos);
        size_t linePos = mParser->lexer.reader.getLine(lineNum, lineBegin, lineEnd);
        ASSERT(lineBegin && lineEnd && linePos <= pos);
        throw Exception(kind, x, mParser->lexer.reader.sourceLocation, 
                            lineNum, pos - linePos, pos, lineBegin, lineEnd);
    }

    inline char narrow(char16 ch) { return char(ch); }
    // Accepts a String as the error argument and converts to char *
    void JS2Metadata::reportError(Exception::Kind kind, char *message, size_t pos, const String& name)
    {
        std::string str(name.length(), char());
        std::transform(name.begin(), name.end(), str.begin(), narrow);
        reportError(kind, message, pos, str.c_str());
    }

/************************************************************************************
 *
 *  JS2Object
 *
 ************************************************************************************/

    void *JS2Object::operator new(size_t s)
    {
        return STD::malloc(s);
    }

}; // namespace MetaData
}; // namespace Javascript