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
#include <assert.h>

#include "world.h"
#include "utilities.h"
#include "js2value.h"

#include <map>
#include <algorithm>

#include "reader.h"
#include "parser.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
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
    void JS2Metadata::ValidateStmtList(StmtNode *p) {
        while (p) {
            ValidateStmt(&cxt, &env, p);
            p = p->next;
        }
    }

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
        case StmtNode::If:
            {
                UnaryStmtNode *i = checked_cast<UnaryStmtNode *>(p);
                ValidateExpression(cxt, env, i->expr);
                ValidateStmt(cxt, env, i->stmt);
            }
            break;
        case StmtNode::IfElse:
            {
                BinaryStmtNode *i = checked_cast<BinaryStmtNode *>(p);
                ValidateExpression(cxt, env, i->expr);
                ValidateStmt(cxt, env, i->stmt);
                ValidateStmt(cxt, env, i->stmt2);
            }
            break;
        case StmtNode::Var:
        case StmtNode::Const:
            {
                bool immutable = (p->getKind() == StmtNode::Const);
                Attribute *attr = NULL;
                VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);

                if (vs->attributes) {
                    ValidateAttributeExpression(cxt, env, vs->attributes);
                    attr = EvalAttributeExpression(env, CompilePhase, vs->attributes);
                }
                
                VariableBinding *v = vs->bindings;
                Frame *regionalFrame = env->getRegionalFrame();
                while (v)  {
                    const StringAtom *name = v->name;
                    ValidateTypeExpression(v->type);

                    if (cxt->strict && ((regionalFrame->kind == GlobalObjectKind)
                                        || (regionalFrame->kind == FunctionKind))
                                    && !immutable
                                    && (vs->attributes == NULL)
                                    && (v->type == NULL)) {
                        defineHoistedVar(env, *name, p);
                    }
                    else {
                        CompoundAttribute *a = Attribute::toCompoundAttribute(attr);
                        if (a->dynamic || a->prototype)
                            reportError(Exception::definitionError, "Illegal attribute", p->pos);
                        Attribute::MemberModifier memberMod = a->memberMod;
                        if ((env->getTopFrame()->kind == ClassKind)
                                && (memberMod == Attribute::NoModifier))
                            memberMod = Attribute::Final;
                        switch (memberMod) {
                        case Attribute::NoModifier:
                        case Attribute::Static: {
                                Variable *var = new Variable(NULL, JS2VAL_UNDEFINED, immutable);
                                defineStaticMember(env, *name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, var, p->pos);
                                // XXX - not done !!! XXX
                                // the type and the value are 'future'
                            }
                            break;
                        case Attribute::Abstract:
                        case Attribute::Virtual:
                        case Attribute::Final: 
                            {
                                JS2Class *c = checked_cast<JS2Class *>(env->getTopFrame());
                                InstanceMember *m = NULL;
                                switch (memberMod) {
                                case Attribute::Abstract:
                                    if (v->initializer)
                                        reportError(Exception::syntaxError, "Abstract member may not have initializer", p->pos);
                                    m = new InstanceAccessor(NULL, false);
                                    break;
                                case Attribute::Virtual:
                                    m = new InstanceVariable(immutable, false);
                                    break;
                                case Attribute::Final: 
                                    m = new InstanceVariable(immutable, true);
                                    break;
                                }
                                defineInstanceMember(c, cxt, *name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, m, p->pos);
                            }
                            break;
                        }
                    }

                    v = v->next;
                }
            }
            break;
        case StmtNode::expression:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                ValidateExpression(cxt, env, e->expr);
            }
            break;
        case StmtNode::Namespace:
            {
                NamespaceStmtNode *ns = checked_cast<NamespaceStmtNode *>(p);
                Attribute *attr = NULL;
                if (ns->attributes) {
                    ValidateAttributeExpression(cxt, env, ns->attributes);
                    attr = EvalAttributeExpression(env, CompilePhase, ns->attributes);
                }
                CompoundAttribute *a = Attribute::toCompoundAttribute(attr);
                if (a->dynamic || a->prototype)
                    reportError(Exception::definitionError, "Illegal attribute", p->pos);
                if ( ! ((a->memberMod == Attribute::NoModifier) || ((a->memberMod == Attribute::Static) && (env->getTopFrame()->kind == ClassKind))) )
                    reportError(Exception::definitionError, "Illegal attribute", p->pos);
                Variable *v = new Variable(namespaceClass, OBJECT_TO_JS2VAL(new Namespace(ns->name)), true);
                defineStaticMember(env, ns->name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos);
            }
            break;
        case StmtNode::Use:
            {
                UseStmtNode *u = checked_cast<UseStmtNode *>(p);
                ExprList *eList = u->namespaces;
                while (eList) {
                    js2val av = EvalExpression(env, CompilePhase, eList->expr);
                    if (JS2VAL_IS_NULL(av) || !JS2VAL_IS_OBJECT(av))
                        reportError(Exception::badValueError, "Namespace expected in use directive", p->pos);
                    JS2Object *obj = JS2VAL_TO_OBJECT(av);
                    if ((obj->kind != AttributeObjectKind) || (checked_cast<Attribute *>(obj)->attrKind != Attribute::NamespaceAttr))
                        reportError(Exception::badValueError, "Namespace expected in use directive", p->pos);
                    cxt->openNamespaces.push_back(checked_cast<Namespace *>(obj));                    
                    eList = eList->next;
                }
            }
            break;
        case StmtNode::Class:
            {
                ClassStmtNode *classStmt = checked_cast<ClassStmtNode *>(p);
                JS2Class *superClass = objectClass;
                if (classStmt->superclass) {
                    ValidateExpression(cxt, env, classStmt->superclass);                    
                    js2val av = EvalExpression(env, CompilePhase, classStmt->superclass);
                    if (JS2VAL_IS_NULL(av) || !JS2VAL_IS_OBJECT(av))
                        reportError(Exception::badValueError, "Class expected in inheritance", p->pos);
                    JS2Object *obj = JS2VAL_TO_OBJECT(av);
                    if (obj->kind != ClassKind)
                        reportError(Exception::badValueError, "Class expected in inheritance", p->pos);
                    superClass = checked_cast<JS2Class *>(obj);
                }
                Attribute *attr = NULL;
                if (classStmt->attributes) {
                    ValidateAttributeExpression(cxt, env, classStmt->attributes);
                    attr = EvalAttributeExpression(env, CompilePhase, classStmt->attributes);
                }
                CompoundAttribute *a = Attribute::toCompoundAttribute(attr);
                if (!superClass->complete || superClass->final)
                    reportError(Exception::definitionError, "Illegal inheritance", p->pos);
                JS2Object *proto = NULL;
                bool final;
                switch (a->memberMod) {
                case Attribute::NoModifier: 
                    final = false; 
                    break;
                case Attribute::Static: 
                    if (env->getTopFrame()->kind != ClassKind)
                        reportError(Exception::definitionError, "Illegal use of static modifier", p->pos);
                    final = false;
                    break;
                case Attribute::Final:
                    final = true;
                    break;
                default:
                    reportError(Exception::definitionError, "Illegal modifier for class definition", p->pos);
                    break;
                }
                JS2Class *c = new JS2Class(superClass, proto, new Namespace(engine->private_StringAtom), (a->dynamic || superClass->dynamic), final, classStmt->name);
                classStmt->c = c;
                Variable *v = new Variable(classClass, OBJECT_TO_JS2VAL(c), true);
                defineStaticMember(env, classStmt->name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos);
                if (classStmt->body) {
                    env->addFrame(c);
                    ValidateStmtList(cxt, env, classStmt->body->statements);
                    ASSERT(env->getTopFrame() == c);
                    env->removeTopFrame();
                }
                c->complete = true;
            }
        }   // switch (p->getKind())
    }


    /*
     * Evaluate the linked list of statement nodes beginning at 'p' 
     * (generate bytecode and then execute that bytecode
     */
    js2val JS2Metadata::ExecuteStmtList(Phase phase, StmtNode *p)
    {
        BytecodeContainer *saveBacon = bCon;
        bCon = new BytecodeContainer();
        size_t lastPos = p->pos;
        while (p) {
            EvalStmt(&env, phase, p);
            lastPos = p->pos;
            p = p->next;
        }
        bCon->emitOp(eReturnVoid, lastPos);
        js2val retval = engine->interpret(this, phase, bCon);
        bCon = saveBacon;
        return retval;
    }

    /*
     * Evaluate an individual statement 'p', including it's children
     *  - this generates bytecode for each statement, but doesn't actually
     * execute it.
     */
    void JS2Metadata::EvalStmt(Environment *env, Phase phase, StmtNode *p) 
    {
        switch (p->getKind()) {
        case StmtNode::block:
        case StmtNode::group:
            {
                BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
                StmtNode *bp = b->statements;
                while (bp) {
                    EvalStmt(env, phase, bp);
                    bp = bp->next;
                }
            }
            break;
        case StmtNode::label:
            {
                LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                EvalStmt(env, phase, l->stmt);
            }
            break;
        case StmtNode::If:
            {
                BytecodeContainer::LabelID skipOverStmt = bCon->getLabel();
                UnaryStmtNode *i = checked_cast<UnaryStmtNode *>(p);
                Reference *r = EvalExprNode(env, phase, i->expr);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eToBoolean, p->pos);
                bCon->emitBranch(eBranchFalse, skipOverStmt, p->pos);
                EvalStmt(env, phase, i->stmt);
                bCon->setLabel(skipOverStmt);
            }
            break;
        case StmtNode::IfElse:
            {
                BytecodeContainer::LabelID falseStmt = bCon->getLabel();
                BytecodeContainer::LabelID skipOverFalseStmt = bCon->getLabel();
                BinaryStmtNode *i = checked_cast<BinaryStmtNode *>(p);
                Reference *r = EvalExprNode(env, phase, i->expr);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eToBoolean, p->pos);
                bCon->emitBranch(eBranchFalse, falseStmt, p->pos);
                EvalStmt(env, phase, i->stmt);
                bCon->emitBranch(eBranch, skipOverFalseStmt, p->pos);
                bCon->setLabel(falseStmt);
                EvalStmt(env, phase, i->stmt2);
                bCon->setLabel(skipOverFalseStmt);
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
                Reference *r = EvalExprNode(env, phase, e->expr);
                if (r) r->emitReadBytecode(bCon, p->pos);
            }
            break;
        case StmtNode::Namespace:
            {
            }
            break;
        case StmtNode::Use:
            {
            }
            break;
        case StmtNode::Class:
            {
                ClassStmtNode *classStmt = checked_cast<ClassStmtNode *>(p);
                JS2Class *c = classStmt->c;
                if (classStmt->body) {
                    env->addFrame(c);
                    bCon->emitOp(ePushFrame, p->pos);
                    bCon->addFrame(c);
                    StmtNode *bp = classStmt->body->statements;
                    while (bp) {
                        EvalStmt(env, phase, bp);
                        bp = bp->next;
                    }
                    ASSERT(env->getTopFrame() == c);
                    env->removeTopFrame();
                    bCon->emitOp(ePopFrame, p->pos);
                }
            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        }   // switch (p->getKind())
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
                if (a && (a->attrKind == Attribute::FalseAttr))
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
                js2val av = EvalExpression(env, CompilePhase, p);
                if (JS2VAL_IS_NULL(av) || !JS2VAL_IS_OBJECT(av))
                    reportError(Exception::badValueError, "Namespace expected in attribute", p->pos);
                JS2Object *obj = JS2VAL_TO_OBJECT(av);
                if ((obj->kind != AttributeObjectKind) || (checked_cast<Attribute *>(obj)->attrKind != Attribute::NamespaceAttr))
                    reportError(Exception::badValueError, "Namespace expected in attribute", p->pos);
                return checked_cast<Attribute *>(obj);
            }
            break;

        } // switch (p->getKind())
        return NULL;
    }

    // Combine attributes a & b, reporting errors for incompatibilities
    // a is not false
    Attribute *Attribute::combineAttributes(Attribute *a, Attribute *b)
    {
        if (b && (b->attrKind == FalseAttr)) {
            if (a) delete a;
            return b;
        }
        if (!a || (a->attrKind == TrueAttr)) {
            if (a) delete a;
            return b;
        }
        if (!b || (b->attrKind == TrueAttr)) {
            if (b) delete b;
            return a;
        }
        if (a->attrKind == NamespaceAttr) {
            if (a == b) {
                delete b;
                return a;
            }
            Namespace *na = checked_cast<Namespace *>(a);
            if (b->kind == NamespaceAttr) {
                Namespace *nb = checked_cast<Namespace *>(b);
                CompoundAttribute *c = new CompoundAttribute();
                c->addNamespace(na);
                c->addNamespace(nb);
                delete a;
                delete b;
                return (Attribute *)c;
            }
            else {
                ASSERT(b->attrKind == CompoundAttr);
                CompoundAttribute *cb = checked_cast<CompoundAttribute *>(b);
                cb->addNamespace(na);
                delete a;
                return b;
            }
        }
        else {
            // Both a and b are compound attributes. Ensure that they have no conflicting contents.
            ASSERT((a->attrKind == CompoundAttr) && (b->attrKind == CompoundAttr));
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
        if (namespaces) {
            for (NamespaceListIterator i = namespaces->begin(), end = namespaces->end(); (i != end); i++)
                if (*i == n)
                    return;
        }
        else
            namespaces = new NamespaceList();
        namespaces->push_back(n);
    }

    CompoundAttribute::CompoundAttribute() : Attribute(CompoundAttr),
            namespaces(NULL), xplicit(false), dynamic(false), memberMod(NoModifier), 
            overrideMod(NoOverride), prototype(false), unused(false) 
    { 
    }

    CompoundAttribute *Attribute::toCompoundAttribute(Attribute *a)
    { 
        if (a) 
            return a->toCompoundAttribute(); 
        else
            return new CompoundAttribute();
    }

    CompoundAttribute *Namespace::toCompoundAttribute()    
    { 
        CompoundAttribute *t = new CompoundAttribute(); 
        t->addNamespace(this); 
        return t; 
    }

    CompoundAttribute *TrueAttribute::toCompoundAttribute()    
    { 
        return new CompoundAttribute(); 
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
        case ExprNode::number:
        case ExprNode::boolean:
            break;
        case ExprNode::objectLiteral:
            break;
        case ExprNode::dot:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                ValidateExpression(cxt, env, b->op1);
                ValidateExpression(cxt, env, b->op2);
            }
            break;

        case ExprNode::assignment:
        case ExprNode::add:
        case ExprNode::subtract:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                ValidateExpression(cxt, env, b->op1);
                ValidateExpression(cxt, env, b->op2);
            }
            break;
        case ExprNode::qualify:
        case ExprNode::identifier:
            {
//                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
            }
            break;
        case ExprNode::New: 
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                ValidateExpression(cxt, env, i->op);
                ExprPairList *args = i->pairs;
                while (args) {
                    ValidateExpression(cxt, env, args->value);
                    args = args->next;
                }
            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        } // switch (p->getKind())
    }



    /*
     * Evaluate an expression 'p' and execute the assocaited bytecode
     */
    js2val JS2Metadata::EvalExpression(Environment *env, Phase phase, ExprNode *p)
    {
        BytecodeContainer *saveBacon = bCon;
        bCon = new BytecodeContainer();
        Reference *r = EvalExprNode(env, phase, p);
        if (r) r->emitReadBytecode(bCon, p->pos);
        bCon->emitOp(eReturn, p->pos);
        js2val retval = engine->interpret(this, phase, bCon);
        bCon = saveBacon;
        return retval;
    }

    /*
     * Evaluate the expression rooted at p.
     */
    Reference *JS2Metadata::EvalExprNode(Environment *env, Phase phase, ExprNode *p)
    {
        Reference *returnRef = NULL;
        JS2Op binaryOp;

        switch (p->getKind()) {

        case ExprNode::assignment:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) {
                    Reference *rVal = EvalExprNode(env, phase, b->op2);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    lVal->emitWriteBytecode(bCon, p->pos);
                }
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
            }
            break;
        case ExprNode::add:
            binaryOp = eAdd;
            goto doBinary;
        case ExprNode::subtract:
            binaryOp = eSubtract;
            goto doBinary;
doBinary:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(binaryOp, p->pos);
            }
            break;

        case ExprNode::postIncrement:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, u->op);
                    ASSERT(false);
            }
            break;

        case ExprNode::number:
            {
                bCon->emitOp(eNumber, p->pos);
                bCon->addFloat64(checked_cast<NumberExprNode *>(p)->value);
            }
            break;
        case ExprNode::qualify:
            {
                QualifyExprNode *qe = checked_cast<QualifyExprNode *>(p);
                const StringAtom &name = qe->name;

                js2val av = EvalExpression(env, CompilePhase, qe->qualifier);
                if (JS2VAL_IS_NULL(av) || !JS2VAL_IS_OBJECT(av))
                    reportError(Exception::badValueError, "Namespace expected in qualifier", p->pos);
                JS2Object *obj = JS2VAL_TO_OBJECT(av);
                if ((obj->kind != AttributeObjectKind) || (checked_cast<Attribute *>(obj)->attrKind != Attribute::NamespaceAttr))
                    reportError(Exception::badValueError, "Namespace expected in qualifier", p->pos);
                Namespace *ns = checked_cast<Namespace *>(obj);
                
                returnRef = new LexicalReference(name, ns, cxt.strict);
                // Calling emitBindBytecode causes the multiname to get loaded onto the stack
                ((LexicalReference *)returnRef)->emitBindBytecode(bCon, p->pos);
            }
            break;
        case ExprNode::identifier:
            {
                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
                returnRef = new LexicalReference(i->name, cxt.strict);
                ((LexicalReference *)returnRef)->variableMultiname->addNamespace(cxt);
                ((LexicalReference *)returnRef)->emitBindBytecode(bCon, p->pos);
            }
            break;
        case ExprNode::dot:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *baseVal = EvalExprNode(env, phase, b->op1);
                if (baseVal) baseVal->emitReadBytecode(bCon, p->pos);

                if (b->op2->getKind() == ExprNode::identifier) {
                    returnRef = new DotReference(i->name);
                    ((DotReference *)returnRef)->emitBindBytecode(bCon, p->pos);
                } 
                else {
                    if (b->op2->getKind() == ExprNode::qualify) {
                        Reference *rVal = EvalExprNode(env, phase, b->op2);                        
                        ASSERT(rVal && checked_cast<LexicalReference *>(rVal));
                        returnRef = new DotReference(((LexicalReference *)rVal)->variableMultiname);
                        ((DotReference *)returnRef)->emitBindBytecode(bCon, p->pos);
                    }
                    // else bracketRef...
                }
            }
            break;
        case ExprNode::boolean:
            if (checked_cast<BooleanExprNode *>(p)->value) 
                bCon->emitOp(eTrue, p->pos);
            else 
                bCon->emitOp(eFalse, p->pos);
            break;
        case ExprNode::objectLiteral:
            {
                uint32 argCount = 0;
                PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
                ExprPairList *e = plen->pairs;
                while (e) {
                    ASSERT(e->field && e->value);
                    Reference *rVal = EvalExprNode(env, phase, e->value);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    switch (e->field->getKind()) {
                    case ExprNode::identifier:
                        bCon->addString(checked_cast<IdentifierExprNode *>(e->field)->name, p->pos);
                        break;
                    case ExprNode::string:
                        bCon->addString(checked_cast<StringExprNode *>(e->field)->str, p->pos);
                        break;
                    case ExprNode::number:
                        bCon->addString(numberToString(&(checked_cast<NumberExprNode *>(e->field))->value), p->pos);
                        break;
                    default:
                        NOT_REACHED("bad field name");
                    }
                    argCount++;
                    e = e->next;
                }
                bCon->emitOp(eNewObject, -argCount + 1);
                bCon->addShort(argCount);
            }
            break;
        case ExprNode::New: 
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *rVal = EvalExprNode(env, phase, i->op);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                ExprPairList *args = i->pairs;
                uint32 argCount = 0;
                while (args) {
                    EvalExprNode(env, phase, args->value);
                    argCount++;
                    args = args->next;
                }
                bCon->emitOp(eNew, p->pos);
            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        }
        return returnRef;
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
        while (pf && (pf->kind != ClassKind))
            pf = pf->nextFrame;
        return checked_cast<JS2Class *>(pf);
    }

    // returns the most specific regional frame.
    Frame *Environment::getRegionalFrame()
    {
        Frame *pf = firstFrame;
        Frame *prev = NULL;
        while (pf->kind == BlockKind) { 
            prev = pf;
            pf = pf->nextFrame;
        }
        if ((pf != firstFrame) && (pf->kind == ClassKind))
            pf = prev;
        return pf;
    }

    // XXX makes the argument for vector instead of linked list...
    // Returns the penultimate frame, either Package or Global
    Frame *Environment::getPackageOrGlobalFrame()
    {
        Frame *pf = firstFrame;
        while (pf && (pf->nextFrame) && (pf->nextFrame->nextFrame))
            pf = pf->nextFrame;
        return pf;
    }

    // findThis returns the value of this. If allowPrototypeThis is true, allow this to be defined 
    // by either an instance member of a class or a prototype function. If allowPrototypeThis is 
    // false, allow this to be defined only by an instance member of a class.
    js2val Environment::findThis(bool allowPrototypeThis)
    {
        Frame *pf = firstFrame;
        while (pf) {
            if ((pf->kind == FunctionKind)
                    && !JS2VAL_IS_NULL(checked_cast<FunctionFrame *>(pf)->thisObject))
                if (allowPrototypeThis || !checked_cast<FunctionFrame *>(pf)->prototype)
                    return checked_cast<FunctionFrame *>(pf)->thisObject;
            pf = pf->nextFrame;
        }
        return JS2VAL_VOID;
    }

    // Read the value of a lexical reference - it's an error if that reference
    // doesn't have a binding somewhere
    js2val Environment::lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase)
    {
        LookupKind lookup(true, findThis(false));
        Frame *pf = firstFrame;
        while (pf) {
            js2val rval;
            if (meta->readProperty(pf, multiname, &lookup, phase, &rval))
                return rval;

            pf = pf->nextFrame;
        }
        meta->reportError(Exception::referenceError, "{0} is undefined", meta->engine->errorPos(), multiname->name);
        return JS2VAL_VOID;
    }

    void Environment::lexicalWrite(JS2Metadata *meta, Multiname *multiname, js2val newValue, bool createIfMissing, Phase phase)
    {
        LookupKind lookup(true, findThis(false));
        Frame *pf = firstFrame;
        while (pf) {
            if (meta->writeProperty(pf, multiname, &lookup, false, newValue, phase))
                return;
            pf = pf->nextFrame;
        }
        if (createIfMissing) {
            pf = getPackageOrGlobalFrame();
            if (pf->kind == GlobalObjectKind) {
                if (meta->writeProperty(pf, multiname, &lookup, true, newValue, phase))
                    return;
            }
        }
        meta->reportError(Exception::referenceError, "{0} is undefined", meta->engine->errorPos(), multiname->name);
    }



/************************************************************************************
 *
 *  Context
 *
 ************************************************************************************/

    // clone a context
    Context::Context(Context *cxt) : strict(cxt->strict), openNamespaces(cxt->openNamespaces)
    {
        ASSERT(false);  // ?? used ??
    }


/************************************************************************************
 *
 *  Multiname
 *
 ************************************************************************************/

    // return true if the given namespace is on the namespace list
    bool Multiname::onList(Namespace *nameSpace)
    { 
        for (NamespaceListIterator n = nsList.begin(), end = nsList.end(); (n != end); n++) {
            if (*n == nameSpace)
                return true;
        }
        return false;
    }

    void Multiname::addNamespace(Context &cxt)    
    { 
        addNamespace(&cxt.openNamespaces); 
    }


    // add every namespace from the list to this Multiname
    void Multiname::addNamespace(NamespaceList *ns)
    {
        for (NamespaceListIterator nli = ns->begin(), end = ns->end();
                (nli != end); nli++)
            nsList.push_back(*nli);
    }


/************************************************************************************
 *
 *  JS2Metadata
 *
 ************************************************************************************/

    // - Define namespaces::id (for all namespaces or at least 'public') in the top frame 
    //     unless it's there already. 
    // - If the binding exists (not forbidden) in lower frames in the regional environment, it's an error.
    // - Define a forbidden binding in all the lower frames.
    // 
    void JS2Metadata::defineStaticMember(Environment *env, const StringAtom &id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, StaticMember *m, size_t pos)
    {
        NamespaceList publicNamespaceList;

        Frame *localFrame = env->getTopFrame();
        if ((overrideMod != Attribute::NoOverride) || (xplicit && localFrame->kind != PackageKind))
            reportError(Exception::definitionError, "Illegal definition", pos);
        if ((namespaces == NULL) || namespaces->empty()) {
            publicNamespaceList.push_back(publicNamespace);
            namespaces = &publicNamespaceList;
        }
        Multiname *mn = new Multiname(id);
        mn->addNamespace(namespaces);

        for (StaticBindingIterator b = localFrame->staticReadBindings.lower_bound(id),
                end = localFrame->staticReadBindings.upper_bound(id); (b != end); b++) {
            if (mn->matches(b->second->qname))
                reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
        }
        

        // Check all frames below the current - up to the RegionalFrame
        Frame *regionalFrame = env->getRegionalFrame();
        if (localFrame != regionalFrame) {
            Frame *fr = localFrame->nextFrame;
            while (fr != regionalFrame) {
                for (b = fr->staticReadBindings.lower_bound(id),
                        end = fr->staticReadBindings.upper_bound(id); (b != end); b++) {
                    if (mn->matches(b->second->qname) && (b->second->content->kind == StaticMember::Forbidden))
                        reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
                }
                fr = fr->nextFrame;
            }
        }
        if (regionalFrame->kind == GlobalObjectKind) {
            GlobalObject *gObj = checked_cast<GlobalObject *>(regionalFrame);
            DynamicPropertyIterator dp = gObj->dynamicProperties.find(id);
            if (dp != gObj->dynamicProperties.end())
                reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
        }
        
        for (NamespaceListIterator nli = mn->nsList.begin(), nlend = mn->nsList.end(); (nli != nlend); nli++) {
            QualifiedName qName(*nli, id);
            StaticBinding *sb = new StaticBinding(qName, m);
            const StaticBindingMap::value_type e(id, sb);
            if (access & ReadAccess)
                regionalFrame->staticReadBindings.insert(e);
            if (access & WriteAccess)
                regionalFrame->staticWriteBindings.insert(e);
        }
        
        if (localFrame != regionalFrame) {
            Frame *fr = localFrame->nextFrame;
            while (fr != regionalFrame) {
                for (NamespaceListIterator nli = mn->nsList.begin(), nlend = mn->nsList.end(); (nli != nlend); nli++) {
                    QualifiedName qName(*nli, id);
                    StaticBinding *sb = new StaticBinding(qName, forbiddenMember);
                    const StaticBindingMap::value_type e(id, sb);
                    if (access & ReadAccess)
                        fr->staticReadBindings.insert(e);
                    if (access & WriteAccess)
                        fr->staticWriteBindings.insert(e);
                }
                fr = fr->nextFrame;
            }
        }

    }

    // Look through 'c' and all it's super classes for a identifier 
    // matching the qualified name and access.
    InstanceMember *JS2Metadata::findInstanceMember(JS2Class *c, QualifiedName *qname, Access access)
    {
        if (qname == NULL)
            return NULL;
        JS2Class *s = c;
        while (s) {
            if (access & ReadAccess) {
                for (InstanceBindingIterator b = s->instanceReadBindings.lower_bound(qname->id),
                        end = s->instanceReadBindings.upper_bound(qname->id); (b != end); b++) {
                    if (*qname == b->second->qname)
                        return b->second->content;
                }
            }        
            if (access & WriteAccess) {
                for (InstanceBindingIterator b = s->instanceWriteBindings.lower_bound(qname->id),
                        end = s->instanceWriteBindings.upper_bound(qname->id); (b != end); b++) {
                    if (*qname == b->second->qname)
                        return b->second->content;
                }
            }
            s = s->super;
        }
        return NULL;
    }

    // Examine class 'c' and find all instance members that would be overridden
    // by 'id' in any of the given namespaces.
    OverrideStatus *JS2Metadata::searchForOverrides(JS2Class *c, const StringAtom &id, NamespaceList *namespaces, Access access, size_t pos)
    {
        OverrideStatus *os = new OverrideStatus(NULL, id);
        for (NamespaceListIterator ns = namespaces->begin(), end = namespaces->end(); (ns != end); ns++) {
            QualifiedName qname(*ns, id);
            InstanceMember *m = findInstanceMember(c, &qname, access);
            if (m) {
                os->multiname.addNamespace(*ns);
                if (os->overriddenMember == NULL)
                    os->overriddenMember = m;
                else
                    if (os->overriddenMember != m)  // different instance members by same id
                        reportError(Exception::definitionError, "Illegal override", pos);
            }
        }
        return os;
    }

    // Find the possible override conflicts that arise from the given id and namespaces
    // Fall back on the currently open namespace list if no others are specified.
    OverrideStatus *JS2Metadata::resolveOverrides(JS2Class *c, Context *cxt, const StringAtom &id, NamespaceList *namespaces, Access access, bool expectMethod, size_t pos)
    {
        OverrideStatus *os = NULL;
        if ((namespaces == NULL) || namespaces->empty()) {
            os = searchForOverrides(c, id, &cxt->openNamespaces, access, pos);
            if (os->overriddenMember == NULL) {
                ASSERT(os->multiname.nsList.empty());
                os->multiname.addNamespace(publicNamespace);
            }
        }
        else {
            OverrideStatus *os2 = searchForOverrides(c, id, namespaces, access, pos);
            if (os2->overriddenMember == NULL) {
                OverrideStatus *os3 = searchForOverrides(c, id, &cxt->openNamespaces, access, pos);
                if (os3->overriddenMember == NULL) {
                    os->multiname.addNamespace(namespaces);
                }
                else {
                    os->potentialConflict = true;   // Didn't find the member with a specified namespace, but did with
                                                    // the use'd ones. That'll be an error unless the override is 
                                                    // disallowed (in defineInstanceMember below)
                    os->multiname.addNamespace(namespaces);
                }
                delete os3;
                delete os2;
            }
            else {
                os = os2;
                os->multiname.addNamespace(namespaces);
            }
        }
        // For all the discovered possible overrides, make sure the member doesn't already exist in the class
        for (NamespaceListIterator nli = os->multiname.nsList.begin(), nlend = os->multiname.nsList.end(); (nli != nlend); nli++) {
            QualifiedName qname(*nli, id);
            if (access & ReadAccess) {
                for (InstanceBindingIterator b = c->instanceReadBindings.lower_bound(id),
                        end = c->instanceReadBindings.upper_bound(id); (b != end); b++) {
                    if (qname == b->second->qname)
                        reportError(Exception::definitionError, "Illegal override", pos);
                }
            }        
            if (access & WriteAccess) {
                for (InstanceBindingIterator b = c->instanceWriteBindings.lower_bound(id),
                        end = c->instanceWriteBindings.upper_bound(id); (b != end); b++) {
                    if (qname == b->second->qname)
                        reportError(Exception::definitionError, "Illegal override", pos);
                }
            }
        }
        // Make sure we're getting what we expected
        if (expectMethod) {
            if (os->overriddenMember && !os->potentialConflict && (os->overriddenMember->kind != InstanceMember::InstanceMethodKind))
                reportError(Exception::definitionError, "Illegal override, expected method", pos);
        }
        else {
            if (os->overriddenMember && !os->potentialConflict && (os->overriddenMember->kind == InstanceMember::InstanceMethodKind))
                reportError(Exception::definitionError, "Illegal override, didn't expect method", pos);
        }

        return os;
    }

    OverrideStatusPair JS2Metadata::defineInstanceMember(JS2Class *c, Context *cxt, const StringAtom &id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, InstanceMember *m, size_t pos)
    {
        OverrideStatus *readStatus;
        OverrideStatus *writeStatus;
        if (xplicit)
            reportError(Exception::definitionError, "Illegal use of explicit", pos);

        if (access & ReadAccess)
            readStatus = resolveOverrides(c, cxt, id, namespaces, ReadAccess, (m->kind == InstanceMember::InstanceMethodKind), pos);
        else
            readStatus = new OverrideStatus(NULL, id);

        if (access & WriteAccess)
            writeStatus = resolveOverrides(c, cxt, id, namespaces, WriteAccess, (m->kind == InstanceMember::InstanceMethodKind), pos);
        else
            writeStatus = new OverrideStatus(NULL, id);

        if ((!readStatus->potentialConflict && (readStatus->overriddenMember != NULL))
                || (!writeStatus->potentialConflict && (writeStatus->overriddenMember != NULL))) {
            if ((overrideMod != Attribute::DoOverride) && (overrideMod != Attribute::OverrideUndefined))
                reportError(Exception::definitionError, "Illegal override", pos);
        }
        else {
            if (readStatus->potentialConflict || writeStatus->potentialConflict) {
                if ((overrideMod != Attribute::DontOverride) && (overrideMod != Attribute::OverrideUndefined))
                    reportError(Exception::definitionError, "Illegal override", pos);
            }
        }

        NamespaceListIterator nli, nlend;
        for (nli = readStatus->multiname.nsList.begin(), nlend = readStatus->multiname.nsList.end(); (nli != nlend); nli++) {
            QualifiedName qName(*nli, id);
            InstanceBinding *ib = new InstanceBinding(qName, m);
            const InstanceBindingMap::value_type e(id, ib);
            c->instanceReadBindings.insert(e);
        }
        
        for (nli = writeStatus->multiname.nsList.begin(), nlend = writeStatus->multiname.nsList.end(); (nli != nlend); nli++) {
            QualifiedName qName(*nli, id);
            InstanceBinding *ib = new InstanceBinding(qName, m);
            const InstanceBindingMap::value_type e(id, ib);
            c->instanceWriteBindings.insert(e);
        }
        
        OverrideStatusPair osp(readStatus, writeStatus);
        return osp;
    }

    // Define a hoisted var in the current frame (either Global or a Function)
    void JS2Metadata::defineHoistedVar(Environment *env, const StringAtom &id, StmtNode *p)
    {
        QualifiedName qName(publicNamespace, id);
        Frame *regionalFrame = env->getRegionalFrame();
        ASSERT((env->getTopFrame()->kind == GlobalObjectKind) || (env->getTopFrame()->kind == FunctionKind));
    
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
            if (regionalFrame->kind == GlobalObjectKind) {
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
        world(world),
        engine(new JS2Engine(world)),
        publicNamespace(new Namespace(engine->public_StringAtom)),
        bCon(new BytecodeContainer()),
        glob(world),
        env(new MetaData::SystemFrame(), &glob)
    {
        cxt.openNamespaces.clear();
        cxt.openNamespaces.push_back(publicNamespace);

        objectClass = new JS2Class(NULL, new JS2Object(PrototypeInstanceKind), new Namespace(engine->private_StringAtom), true, false, engine->object_StringAtom);
        objectClass->complete = true;
    }

    // objectType(o) returns an OBJECT o's most specific type.
    JS2Class *JS2Metadata::objectType(js2val obj)
    {
        if (JS2VAL_IS_VOID(obj))
            return undefinedClass;
        if (JS2VAL_IS_NULL(obj))
            return nullClass;
        if (JS2VAL_IS_BOOLEAN(obj))
            return booleanClass;
        if (JS2VAL_IS_NUMBER(obj))
            return numberClass;
        if (JS2VAL_IS_STRING(obj)) {
            if (JS2VAL_TO_STRING(obj)->length() == 1)
                return characterClass;
            else 
                return stringClass;
        }
        ASSERT(JS2VAL_IS_OBJECT(obj));
        JS2Object *obj = JS2VAL_TO_OBJECT(obj);
        switch (obj->kind) {
        case AttributeObjectKind:
            return attributeClass;
        case MultinameKind:
            return namespaceClass;
        case ClassKind:
            return classClass;
        case PrototypeInstanceKind: 
            return prototypeClass;

        case FixedInstanceKind: 
            return checked_cast<FixedInstance *>(obj)->type;
        case DynamicInstanceKind:
            return checked_cast<DynamicInstance *>(obj)->type;

        case GlobalObjectKind: 
        case PackageKind:
            return packageClass;

        case SystemKind:
        case FunctionKind: 
        case BlockKind: 
        default:
            ASSERT(false);
            return NULL;
        }
    }

    // Read the property from the container given by the public id in multiname - if that exists
    // 
    bool JS2Metadata::readDynamicProperty(JS2Object *container, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval)
    {
        ASSERT(container && ((container->kind == DynamicInstanceKind) 
                                || (container->kind == GlobalObjectKind)
                                || (container->kind == PrototypeInstanceKind)));
        if (!multiname->onList(publicNamespace))
            return false;
        const StringAtom &name = multiname->name;
        if (phase == CompilePhase) 
            reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
        DynamicPropertyMap *dMap = NULL;
        bool isPrototypeInstance = false;
        if (container->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(container))->dynamicProperties;
        else
        if (container->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(container))->dynamicProperties;
        else {
            isPrototypeInstance = true;
            dMap = &(checked_cast<PrototypeInstance *>(container))->dynamicProperties;
        }
        for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
            if (i->first == name) {
                *rval = i->second;
                return true;
            }
        }
        if (isPrototypeInstance) {
            PrototypeInstance *pInst = (checked_cast<PrototypeInstance *>(container))->parent;
            while (pInst) {
                for (DynamicPropertyIterator i = pInst->dynamicProperties.begin(), end = pInst->dynamicProperties.end(); (i != end); i++) {
                    if (i->first == name) {
                        *rval = i->second;
                        return true;
                    }
                }
                pInst = pInst->parent;
            }
        }
        if (lookupKind->isPropertyLookup()) {
            *rval = JS2VAL_UNDEFINED;
            return true;
        }
        return false;   // 'None'
    }

    bool JS2Metadata::writeDynamicProperty(Frame *container, Multiname *multiname, bool createIfMissing, js2val newValue, Phase phase)
    {
        ASSERT(container && ((container->kind == DynamicInstanceKind) 
                                || (container->kind == GlobalObjectKind)
                                || (container->kind == PrototypeInstanceKind)));
        if (!multiname->onList(publicNamespace))
            return false;
        const StringAtom &name = multiname->name;
        DynamicPropertyMap *dMap = NULL;
        if (container->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(container))->dynamicProperties;
        else
        if (container->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(container))->dynamicProperties;
        else 
            dMap = &(checked_cast<PrototypeInstance *>(container))->dynamicProperties;
        for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
            if (i->first == name) {
                i->second = newValue;
                return true;
            }
        }
        if (!createIfMissing)
            return false;
        if (container->kind == DynamicInstanceKind) {
            DynamicInstance *dynInst = checked_cast<DynamicInstance *>(container);
            InstanceBinding *ib = resolveInstanceMemberName(dynInst->type, multiname, ReadAccess, phase);
            if (ib == NULL) {
                const DynamicPropertyMap::value_type e(name, newValue);
                dynInst->dynamicProperties.insert(e);
                return true;
            }
        }
        else {
            if (container->kind == GlobalObjectKind) {
                GlobalObject *glob = checked_cast<GlobalObject *>(container);
                StaticMember *m = findFlatMember(glob, multiname, ReadAccess, phase);
                if (m == NULL) {
                    const DynamicPropertyMap::value_type e(name, newValue);
                    glob->dynamicProperties.insert(e);
                    return true;
                }
            }
        }
        return false;   // 'None'
    }

    bool JS2Metadata::readStaticMember(StaticMember *m, Phase phase, js2val *rval)
    {
        if (m == NULL)
            return false;   // 'None'
        switch (m->kind) {
        case StaticMember::Forbidden:
            reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
            break;
        case StaticMember::Variable:
            *rval = (checked_cast<Variable *>(m))->value;
            return true;
        case StaticMember::HoistedVariable:
            *rval = (checked_cast<HoistedVar *>(m))->value;
            return true;
        case StaticMember::ConstructorMethod:
            break;
        case StaticMember::Accessor:
            break;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    bool JS2Metadata::writeStaticMember(StaticMember *m, js2val newValue, Phase phase)
    {
        if (m == NULL)
            return false;   // 'None'
        switch (m->kind) {
        case StaticMember::Forbidden:
        case StaticMember::ConstructorMethod:
            reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
            break;
        case StaticMember::Variable:
            (checked_cast<Variable *>(m))->value = newValue;
            return true;
        case StaticMember::HoistedVariable:
            (checked_cast<HoistedVar *>(m))->value = newValue;
            return true;
        case StaticMember::Accessor:
            break;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    // Read the value of a property in the container. Return true/false if that container has
    // the property or not. If it does, return it's value
    bool JS2Metadata::readProperty(js2val containerVal, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval)
    {
        bool isDynamicInstance = false;
        if (JS2VAL_IS_PRIMITIVE(containerVal)) {
readClassProperty:
            JS2Class *c = objectType(containerVal);
            InstanceBinding *ib = resolveInstanceMemberName(c, multiname, ReadAccess, Phase phase);
            if ((ib == NULL) && isDynamicInstance) 
                return readDynamicProperty(JS2VAL_TO_OBJECT(containerVal), multiname, lookupKind, phase, rval);
            else 
                // XXX passing a primitive here ???
                return readInstanceMember(containerVal, c, (ib)? &ib->qname : NULL, phase, rval);
        }
        JS2Object *obj = JS2VAL_TO_OBJECT(containerVal);
        switch (obj->kind) {
        case AttributeObjectKind:
        case MultinameKind:
        case FixedInstanceKind: 
            goto readClassProperty;
        case DynamicInstanceKind:
            isDynamicInstance = true;
            goto readClassProperty;

        case SystemKind:
        case GlobalObjectKind: 
        case PackageKind:
        case FunctionKind: 
        case BlockKind: 
            {
            }
            break;
        case ClassKind:
            {
            }
            break;

        case PrototypeInstanceKind: 
            return readDynamicProperty(obj, multiname, lookupKind, phase, rval);
        default:
            ASSERT(false);
            return false;
        }
    }
/*
m: INSTANCEMEMBEROPT ¨ findInstanceMember(c, qname, read);
case m of
{none} do return none;
INSTANCEVARIABLE do
if phase = compile and not m.immutable then throw compileExpressionError
end if;
v: OBJECTU ¨ findSlot(this, m).value;
if v = uninitialised then throw uninitialisedError end if;
return v;
INSTANCEMETHOD do return METHODCLOSURE·this: this, method: mÒ;
INSTANCEGETTER do return m.call(this, m.env, phase);
INSTANCESETTER do
m cannot be an INSTANCESETTER because these are only represented as write-only members.
end case
end proc;
*/

proc findSlot(o: OBJECT, id: INSTANCEVARIABLE): SLOT
o must be an INSTANCE;
matchingSlots: SLOT{} ¨ {s | "s Œ resolveAlias(o).slots such that s.id = id};
return the one element of matchingSlots
end proc;

    JS2MetaData::findSlot(js2val thisObjVal, InstanceVariable *id)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisObjVal) 
                    && ((JS2VAL_TO_OBJECT(thisObjVal)->kind == DynamicInstanceKind)
                        || (JS2VAL_TO_OBJECT(thisObjVal)->kind == FixedInstanceKind)));
        JS2Object *thisObj = JS2VAL_TO_OBJECT(thisObjVal);
        Slots *s;
        if (thisObj->kind == DynamicInstanceKind)
            s = checked_cast<DynamicInstance *>(thisObj)->slots;
        else
            s = checked_cast<FixedInstance *>(thisObj)->slots;

    }


    bool JS2Metadata::readInstanceMember(js2val containerVal, JS2Class *c, QualifiedName *qname, Phase phase, js2val *rval)
    {
        InstanceMember *m = findInstanceMember(c, qname, ReadAccess);
        if (m == NULL) return false;
        switch (m->kind) {
        case InstanceVariableKind:
            if ((phase == CompilePhase) && !checked_cast<InstanceVariable *>(m)->immutable)
                reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
            findSlot(containerVal, m);
            

        case InstanceMethodKind:
        case InstanceAccessorKind:
            break;
        }
    }

    // Read the value of a property in the frame. Return true/false if that frame has
    // the property or not. If it does, return it's value
    bool JS2Metadata::readProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval)
    {
        StaticMember *m = findFlatMember(container, multiname, ReadAccess, phase);
        if (!m && (container->kind == GlobalObjectKind))
            return readDynamicProperty(container, multiname, lookupKind, phase, rval);
        else
            return readStaticMember(m, phase, rval);
    }

    // Write the value of a property in the frame. Return true/false if that frame has
    // the property or not.
    bool JS2Metadata::writeProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue, Phase phase)
    {
        StaticMember *m = findFlatMember(container, multiname, WriteAccess, phase);
        if (!m && (container->kind == GlobalObjectKind))
            return writeDynamicProperty(container, multiname, createIfMissing, newValue, phase);
        else
            return writeStaticMember(m, newValue, phase);
    }

    // Find a binding in the frame that matches the multiname and access
    // It's an error if more than one such binding exists.
    StaticMember *JS2Metadata::findFlatMember(Frame *container, Multiname *multiname, Access access, Phase phase)
    {
        StaticMember *found = NULL;
        StaticBindingIterator b, end;
        if (access & ReadAccess) {
            b = container->staticReadBindings.lower_bound(multiname->name);
            end = container->staticReadBindings.upper_bound(multiname->name);
        }
        else {
            b = container->staticWriteBindings.lower_bound(multiname->name);
            end = container->staticWriteBindings.upper_bound(multiname->name);
        }
        while (true) {
            if (b == end) {
                if (access == ReadWriteAccess) {
                    access = WriteAccess;
                    b = container->staticWriteBindings.lower_bound(multiname->name);
                    end = container->staticWriteBindings.upper_bound(multiname->name);
                }
                else
                    break;
            }
            if (multiname->matches(b->second->qname)) {
                if (found && (b->second->content != found))
                    reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                else
                    found = b->second->content;
            }
            b++;
        }
        return found;
    }

    /*
    * Start from the root class (Object) and proceed through more specific classes that are ancestors of c.
    * Find the binding that matches the given access and multiname, it's an error if more than one such exists.
    *
    */
    InstanceBinding *JS2Metadata::resolveInstanceMemberName(JS2Class *c, Multiname *multiname, Access access, Phase phase)
    {
        InstanceBinding *result = NULL;
        if (c->super) {
            result = resolveInstanceMemberName(c->super, multiname, access, phase);
            if (result) return result;
        }
        InstanceBindingIterator b, end;
        if (access & ReadAccess) {
            b = c->instanceReadBindings.lower_bound(multiname->name);
            end = c->instanceReadBindings.upper_bound(multiname->name);
        }
        else {
            b = c->instanceWriteBindings.lower_bound(multiname->name);
            end = c->instanceWriteBindings.upper_bound(multiname->name);
        }
        while (true) {
            if (b == end) {
                if (access == ReadWriteAccess) {
                    access = WriteAccess;
                    b = c->instanceWriteBindings.lower_bound(multiname->name);
                    end = c->instanceWriteBindings.upper_bound(multiname->name);
                }
                else
                    break;
            }
            if (multiname->matches(b->second->qname)) {
                if (result && (b->second->content != result->content))
                    reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                else
                    result = b->second;
            }
            b++;
        }
        return result;
    }

    /*
     * Throw an exception of the specified kind, indicating the position 'pos' and
     * attaching the given message. If 'arg' is specified, replace {0} in the message
     * with the argument value. [This is intended to be widened into a more complete
     * argument handling scheme].
     */
    void JS2Metadata::reportError(Exception::Kind kind, const char *message, size_t pos, const char *arg)
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
    void JS2Metadata::reportError(Exception::Kind kind, const char *message, size_t pos, const String& name)
    {
        std::string str(name.length(), char());
        std::transform(name.begin(), name.end(), str.begin(), narrow);
        reportError(kind, message, pos, str.c_str());
    }

/************************************************************************************
 *
 *  Pond
 *
 ************************************************************************************/

    Pond::Pond(size_t sz, Pond *next) : sanity(POND_SANITY), pondSize(sz + POND_SIZE), pondBase(new uint8[pondSize]), pondTop(pondBase), freeHeader(NULL), nextPond(next) 
    { 
    }
    
    // Allocate from this or the next Pond (make a new one if necessary)
    void *Pond::allocFromPond(int32 sz)
    {
        // Try scannning the free list...
        PondScum *p = freeHeader;
        PondScum *pre = NULL;
        while (p) {
            ASSERT(p->size > 0);
            if (p->size >= sz) {
                if (pre)
                    pre->owner = p->owner;
                else
                    freeHeader = (PondScum *)(p->owner);
                p->owner = this;
                return (p + 1);
            }
            pre = p;
            p = (PondScum *)(p->owner);
        }

        // See if there's room left...
        if (sz > (pondSize - (pondTop - pondBase))) {
            if (nextPond == NULL)
                nextPond = new Pond(sz, nextPond);
            return nextPond->allocFromPond(sz);
        }
        p = (PondScum *)pondTop;
        p->owner = this;
        p->size = sz;
        pondTop += sz;
#ifdef DEBUG
        memset((p + 1), 0xB7, p->size - sizeof(PondScum));
#endif
        return (p + 1);
    }

    // Stick the chunk at the start of the free list
    void Pond::returnToPond(PondScum *p)
    {
        p->owner = (Pond *)freeHeader;
        p->size &= 0x7FFFFFFF;      // might have lingering mark from previous gc
        uint8 *t = (uint8 *)(p + 1);
#ifdef DEBUG
        memset(t, 0xB7, p->size - sizeof(PondScum));
#endif
        freeHeader = p;
    }

    // Clear the mark bit from all PondScums
    void Pond::resetMarks()
    {
        uint8 *t = pondBase;
        while (t != pondTop) {
            PondScum *p = (PondScum *)t;
            p->resetMark();
            t += p->size;
        }
        if (nextPond)
            nextPond->resetMarks();
    }

    // Anything left unmarked is now moved to the free list
    void Pond::moveUnmarkedToFreeList()
    {
        uint8 *t = pondBase;
        while (t != pondTop) {
            PondScum *p = (PondScum *)t;
            if (!p->isMarked())
                returnToPond(p);
            t += p->size;
        }
        if (nextPond)
            nextPond->moveUnmarkedToFreeList();
    }


 /************************************************************************************
 *
 *  JS2Class
 *
 ************************************************************************************/


    JS2Class::JS2Class(JS2Class *super, JS2Object *proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name) 
        : Frame(ClassKind), 
            instanceInitOrder(NULL), 
            complete(false), 
            super(super), 
            prototype(proto), 
            privateNamespace(privateNamespace), 
            dynamic(dynamic),
            primitive(false),
            final(final),
            call(NULL),
            construct(JS2Engine::defaultConstructor),
            name(name)
    { 
    }

    // examine the instancebinding map to determine the number of slots required
    uint32 JS2Class::countSlots()
    {
        uint32 count = 0;
        InstanceBindingIterator b, end;
        for (b = instanceReadBindings.begin(), end = instanceReadBindings.end(); (b != end); b++) {
                
        }
        for (b = instanceWriteBindings.begin(), end = instanceWriteBindings.end(); (b != end); b++) {
        
        }
    }



 /************************************************************************************
 *
 *  JS2Object
 *
 ************************************************************************************/

    Pond JS2Object::pond(POND_SIZE, NULL);
    std::vector<PondScum **> JS2Object::rootList;

    void JS2Object::addRoot(void *t)
    {
        PondScum **p = (PondScum **)t;
        ASSERT(p);
        rootList.push_back(p);
    }

    void JS2Object::gc()
    {
        pond.resetMarks();
        for (std::vector<PondScum **>::iterator i = rootList.begin(), end = rootList.end(); (i != end); i++) {
            PondScum *p = **i;
            if (p) {
                ASSERT(p->owner && (p->size >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
                p->mark();
            }
        }
        pond.moveUnmarkedToFreeList();
    }

    void *JS2Object::alloc(size_t s)
    {
        s += sizeof(PondScum);
        // make sure that the thing is 8-byte aligned
        if (s & 0x7) s += 8 - (s & 0x7);
        ASSERT(s <= 0x7FFFFFFF);
        return pond.allocFromPond((int32)s);
    }

    void JS2Object::unalloc(void *t)
    {
        PondScum *p = (PondScum *)t - 1;
        ASSERT(p->owner && (p->size >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
        p->owner->returnToPond(p);
    }

}; // namespace MetaData
}; // namespace Javascript