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
#include "js2engine.h"
#include "regexp.h"
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
            ValidateStmt(&cxt, env, Singular, p);
            p = p->next;
        }
    }

    void JS2Metadata::ValidateStmtList(Context *cxt, Environment *env, Plurality pl, StmtNode *p) {
        while (p) {
            ValidateStmt(cxt, env, pl, p);
            p = p->next;
        }
    }

    FunctionInstance *JS2Metadata::createFunctionInstance(Environment *env, bool prototype, bool unchecked, NativeCode *code, uint32 length, DynamicVariable **lengthProperty)
    {
        ParameterFrame *compileFrame = new (this) ParameterFrame(JS2VAL_VOID, prototype);
        DEFINE_ROOTKEEPER(this, rk1, compileFrame);        
        FunctionInstance *result = new (this) FunctionInstance(this, functionClass->prototype, functionClass);
        DEFINE_ROOTKEEPER(this, rk2, result);
        if (code == NULL)
            result->fWrap = new FunctionWrapper(this, unchecked, compileFrame, env);
        else
            result->fWrap = new FunctionWrapper(this, unchecked, compileFrame, code, env);
        result->fWrap->length = length;
        DynamicVariable *dv = createDynamicProperty(result, engine->length_StringAtom, INT_TO_JS2VAL(length), ReadAccess, true, false);
        if (lengthProperty)
            *lengthProperty = dv;
        return result;
    }


    FunctionInstance *JS2Metadata::validateStaticFunction(Context *cxt, Environment *env, FunctionDefinition *fnDef, bool prototype, bool unchecked, bool isConstructor, size_t pos)
    {
        DynamicVariable *lengthVar = NULL;
        FunctionInstance *result = createFunctionInstance(env, prototype, unchecked, NULL, 0, &lengthVar);
        DEFINE_ROOTKEEPER(this, rk1, result);
        fnDef->fn = result;

        Frame *curTopFrame = env->getTopFrame();
        CompilationData *oldData = startCompilationUnit(result->fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
        try {
            env->addFrame(result->fWrap->compileFrame);
            VariableBinding *pb = fnDef->parameters;
            uint32 pCount = 0;
            if (pb) {
                while (pb) {
                    pCount++;
                    pb = pb->next;
                }
                pb = fnDef->parameters;
                while (pb) {
                    if (pb->type)
                        ValidateTypeExpression(cxt, env, pb->type);
					if (unchecked) {
						pb->member = NULL;
						defineHoistedVar(env, *pb->name, JS2VAL_UNDEFINED, true, pos);
					}
					else {
						FrameVariable *v = new FrameVariable(result->fWrap->compileFrame->allocateSlot(), FrameVariable::Parameter);
						pb->member = v;
						defineLocalMember(env, *pb->name, NULL, Attribute::NoOverride, false, ReadWriteAccess, v, pb->pos, true);
					}
                    pb = pb->next;
                }
            }
            if (fnDef->resultType)
                ValidateTypeExpression(cxt, env, fnDef->resultType);
            result->fWrap->length = pCount;
            lengthVar->value = INT_TO_JS2VAL(pCount);
            if (cxt->E3compatibility)
                createDynamicProperty(result, world.identifiers["arguments"], JS2VAL_NULL, ReadAccess, true, false);
            result->fWrap->compileFrame->isConstructor = isConstructor;
            ValidateStmt(cxt, env, Plural, fnDef->body);
            env->removeTopFrame();
        }
        catch (Exception x) {
            restoreCompilationUnit(oldData);
            env->setTopFrame(curTopFrame);
            throw x;
        }
        restoreCompilationUnit(oldData);
        return result;
    }

    void JS2Metadata::validateStatic(Context *cxt, Environment *env, FunctionDefinition *fnDef, CompoundAttribute *a, bool unchecked, bool hoisted, size_t pos)
    {
        FunctionInstance *fnInst = NULL;
        DEFINE_ROOTKEEPER(this, rk1, fnInst);
        switch (fnDef->prefix) {
        case FunctionName::normal:
            fnInst = validateStaticFunction(cxt, env, fnDef, a->prototype, unchecked, false, pos);
            if (hoisted)
                defineHoistedVar(env, *fnDef->name, OBJECT_TO_JS2VAL(fnInst), false, pos);
            else {
                Variable *v = new Variable(functionClass, OBJECT_TO_JS2VAL(fnInst), true);
                defineLocalMember(env, *fnDef->name, &a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, pos, true);
            }
            break;
        case FunctionName::Get:
            {
                if (a->prototype)
                    reportError(Exception::attributeError, "A getter cannot have the prototype attribute", pos);
                ASSERT(!(unchecked || hoisted));
                // XXX shouldn't be using validateStaticFunction
                fnInst = validateStaticFunction(cxt, env, fnDef, false, false, false, pos);
                Getter *g = new Getter(fnInst->fWrap->resultType, fnInst);
                defineLocalMember(env, *fnDef->name, &a->namespaces, a->overrideMod, a->xplicit, ReadAccess, g, pos, true);
            }
            break;
        case FunctionName::Set:
            {
                if (a->prototype)
                    reportError(Exception::attributeError, "A setter cannot have the prototype attribute", pos);
                ASSERT(!(unchecked || hoisted));
                // XXX shouldn't be using validateStaticFunction
                fnInst = validateStaticFunction(cxt, env, fnDef, false, false, false, pos);
                Setter *s = new Setter(fnInst->fWrap->resultType, fnInst);
                defineLocalMember(env, *fnDef->name, &a->namespaces, a->overrideMod, a->xplicit, WriteAccess, s, pos, true);
            }
            break;
        }
		StringFormatter sFmt;
		PrettyPrinter pp(sFmt, 80);
		fnDef->print(pp, NULL, true);
        pp.end();
		fnInst->sourceText = engine->allocStringPtr(&sFmt.getString());
    }

    void JS2Metadata::validateConstructor(Context *cxt, Environment *env, FunctionDefinition *fnDef, JS2Class *c, CompoundAttribute *a, size_t pos)
    {
        if (a->prototype)
            reportError(Exception::attributeError, "A class constructor cannot have the prototype attribute", pos);
        if (fnDef->prefix != FunctionName::normal)
            reportError(Exception::syntaxError, "A class constructor cannot be a getter or a setter", pos);
        // XXX shouldn't be using validateStaticFunction
        c->init = validateStaticFunction(cxt, env, fnDef, false, false, true, pos);
    }

    void JS2Metadata::validateInstance(Context *cxt, Environment *env, FunctionDefinition *fnDef, JS2Class *c, CompoundAttribute *a, bool final, size_t pos)
    {
        if (a->prototype)
            reportError(Exception::attributeError, "An instance method cannot have the prototype attribute", pos);
        // XXX shouldn't be using validateStaticFunction
        FunctionInstance *fnInst = NULL;
        DEFINE_ROOTKEEPER(this, rk1, fnInst);
        fnInst = validateStaticFunction(cxt, env, fnDef, false, false, false, pos);
        Multiname *mn = new (this) Multiname(*fnDef->name, a->namespaces);
        InstanceMember *m;
        switch (fnDef->prefix) {
        case FunctionName::normal:
            m = new InstanceMethod(mn, fnInst, final, true);
            break;
        case FunctionName::Set:
            m = new InstanceSetter(mn, fnInst, objectClass, final, true);
            break;
        case FunctionName::Get:
            m = new InstanceGetter(mn, fnInst, objectClass, final, true);
            break;
        }                            
        defineInstanceMember(c, cxt, *fnDef->name, a->namespaces, a->overrideMod, a->xplicit, m, pos);
    }

    /*
     * Validate an individual statement 'p', including it's children
     */
    void JS2Metadata::ValidateStmt(Context *cxt, Environment *env, Plurality pl, StmtNode *p) 
    {
        CompoundAttribute *a = NULL;
        DEFINE_ROOTKEEPER(this, rk, a);
        Frame *curTopFrame = env->getTopFrame();

        try {
            switch (p->getKind()) {
            case StmtNode::block:
            case StmtNode::group:
                {
                    BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
                    b->compileFrame = new (this) BlockFrame();
                    b->compileFrame->isFunctionFrame = (env->getTopFrame()->kind == ParameterFrameKind);
                    bCon->saveFrame(b->compileFrame);   // stash this frame so it doesn't get gc'd before eval pass.
                    env->addFrame(b->compileFrame);
                    targetList.push_back(p);
                    ValidateStmtList(cxt, env, pl, b->statements);
                    env->removeTopFrame();
                    targetList.pop_back();
                }
                break;
            case StmtNode::label:
                {
                    LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                    l->labelID = bCon->getLabel();
                    // Make sure there is no existing target with the same name
                    for (TargetListIterator si = targetList.begin(), end = targetList.end(); (si != end); si++) {
                        switch ((*si)->getKind()) {
                        case StmtNode::label:
                            if (checked_cast<LabelStmtNode *>(*si)->name == l->name)
                                reportError(Exception::syntaxError, "Duplicate statement label", p->pos);
                            break;
                        }
                    }
                    targetList.push_back(p);
                    ValidateStmt(cxt, env, pl, l->stmt);
                    targetList.pop_back();
                }
                break;
            case StmtNode::If:
                {
                    UnaryStmtNode *i = checked_cast<UnaryStmtNode *>(p);
                    ValidateExpression(cxt, env, i->expr);
                    ValidateStmt(cxt, env, pl, i->stmt);
                }
                break;
            case StmtNode::IfElse:
                {
                    BinaryStmtNode *i = checked_cast<BinaryStmtNode *>(p);
                    ValidateExpression(cxt, env, i->expr);
                    ValidateStmt(cxt, env, pl, i->stmt);
                    ValidateStmt(cxt, env, pl, i->stmt2);
                }
                break;
            case StmtNode::ForIn:
                // XXX need to validate that first expression is a single binding ?
            case StmtNode::For:
                {
                    ForStmtNode *f = checked_cast<ForStmtNode *>(p);
                    f->breakLabelID = bCon->getLabel();
                    f->continueLabelID = bCon->getLabel();
                    if (f->initializer)
                        ValidateStmt(cxt, env, pl, f->initializer);
                    if (f->expr2)
                        ValidateExpression(cxt, env, f->expr2);
                    if (f->expr3)
                        ValidateExpression(cxt, env, f->expr3);
                    f->breakLabelID = bCon->getLabel();
                    f->continueLabelID = bCon->getLabel();
                    targetList.push_back(p);
                    ValidateStmt(cxt, env, pl, f->stmt);
                    targetList.pop_back();
                }
                break;
            case StmtNode::Switch:
                {
                    SwitchStmtNode *sw = checked_cast<SwitchStmtNode *>(p);
                    sw->breakLabelID = bCon->getLabel();
                    ValidateExpression(cxt, env, sw->expr);
                    targetList.push_back(p);
                    StmtNode *s = sw->statements;
                    while (s) {
                        if (s->getKind() == StmtNode::Case) {
                            ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                            c->labelID = bCon->getLabel();
                            if (c->expr)
                                ValidateExpression(cxt, env, c->expr);
                        }
                        else
                            ValidateStmt(cxt, env, pl, s);
                        s = s->next;
                    }
                    targetList.pop_back();
                }
                break;
            case StmtNode::While:
            case StmtNode::DoWhile:
                {
                    UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                    w->breakLabelID = bCon->getLabel();
                    w->continueLabelID = bCon->getLabel();
                    targetList.push_back(p);
                    ValidateExpression(cxt, env, w->expr);
                    ValidateStmt(cxt, env, pl, w->stmt);
                    targetList.pop_back();
                }
                break;
            case StmtNode::Break:
                {
                    GoStmtNode *g = checked_cast<GoStmtNode *>(p);
                    g->tgtID = -1;
                    if (g->name) {
                        // need to find the closest 'breakable' statement covered by the named label
                        LabelID tgt = -1;
                        bool foundit = false;
                        for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); 
                                    ((g->tgtID == -1) && (si != end) && !foundit); si++)
                        {
                            switch ((*si)->getKind()) {
                            case StmtNode::label:
                                {
                                    LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                    if (l->name == *g->name) {
                                        g->tgtID = l->labelID;
                                        foundit = true;
                                    }
                                }
                                break;
                            }
                        }
                    }
                    else {
                        // un-labelled, just find the closest breakable statement
                        for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); 
                                        ((g->tgtID == -1) && (si != end)); si++) {
                            switch ((*si)->getKind()) {
                            case StmtNode::While:
                            case StmtNode::DoWhile:
                                {
                                    UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(*si);
                                    g->tgtID = w->breakLabelID;
                                }
                                break;
                            case StmtNode::For:
                            case StmtNode::ForIn:
                                {
                                    ForStmtNode *f = checked_cast<ForStmtNode *>(*si);
                                    g->tgtID = f->breakLabelID;
                                }
                                break;
                            case StmtNode::Switch:
                                {
                                    SwitchStmtNode *s = checked_cast<SwitchStmtNode *>(*si);
                                    g->tgtID = s->breakLabelID;
                                }
                                break;
                            }
                        }
                    }
                    if (g->tgtID == -1) 
                        reportError(Exception::syntaxError, "No break target available", p->pos);
                }
                break;
            case StmtNode::Continue:
                {
                    GoStmtNode *g = checked_cast<GoStmtNode *>(p);
                    g->tgtID = -1;
                    if (g->name) {
                        // need to find the closest 'continuable' statement covered by the named label
                        LabelID tgt = -1;
                        bool foundit = false;
                        for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); 
                                    ((g->tgtID == -1) && (si != end) && !foundit); si++)
                        {
                            switch ((*si)->getKind()) {
                            case StmtNode::label:
                                {
                                    LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                    if (l->name == *g->name) {
                                        g->tgtID = tgt;
                                        foundit = true;
                                    }
                                }
                                break;
                            case StmtNode::While:
                            case StmtNode::DoWhile:
                                {
                                    UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(*si);
                                    tgt = w->continueLabelID;
                                }
                                break;
                            case StmtNode::For:
                            case StmtNode::ForIn:
                                {
                                    ForStmtNode *f = checked_cast<ForStmtNode *>(*si);
                                    tgt = f->continueLabelID;
                                }
                                break;
                            }
                        }
                    }
                    else {
                        // un-labelled, just find the closest breakable statement
                        for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); 
                                        ((g->tgtID == -1) && (si != end)); si++) {
                            // only some non-label statements will do
                            StmtNode *s = *si;
                            switch (s->getKind()) {
                            case StmtNode::While:
                            case StmtNode::DoWhile:
                                {
                                    UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(*si);
                                    g->tgtID = w->continueLabelID;
                                }
                                break;
                            case StmtNode::For:
                            case StmtNode::ForIn:
                                {
                                    ForStmtNode *f = checked_cast<ForStmtNode *>(*si);
                                    g->tgtID = f->continueLabelID;
                                }
                                break;
                            }
                        }
                    }
                    if (g->tgtID == -1) 
                        reportError(Exception::syntaxError, "No continue target available", p->pos);
                }
                break;
            case StmtNode::Throw:
                {
                    ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                    ValidateExpression(cxt, env, e->expr);
                }
                break;
            case StmtNode::Try:
                {
                    TryStmtNode *t = checked_cast<TryStmtNode *>(p);
                    ValidateStmt(cxt, env, pl, t->stmt);
                    if (t->finally)
                        ValidateStmt(cxt, env, pl, t->finally);
                    CatchClause *c = t->catches;
                    while (c) {                    
                        ValidateStmt(cxt, env, pl, c->stmt);
                        if (c->type)
                            ValidateExpression(cxt, env, c->type);
                        c = c->next;
                    }
                }
                break;
            case StmtNode::Return:
                {
                    ParameterFrame *pFrame = env->getEnclosingParameterFrame(NULL);
                    // If there isn't a parameter frame, or the parameter frame is
                    // only a runtime frame (for eval), then it's an orphan return
                    if (!pFrame || pFrame->pluralFrame)
                        reportError(Exception::syntaxError, "Return statement not in function", p->pos);
                    ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                    if (e->expr) {
                        ValidateExpression(cxt, env, e->expr);
                    }
                }
                break;
            case StmtNode::Function:
                {
                    Attribute *attr = NULL;
                    FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
                    if (f->attributes) {
                        ValidateAttributeExpression(cxt, env, f->attributes);
                        attr = EvalAttributeExpression(env, CompilePhase, f->attributes);
                    }
                    a = Attribute::toCompoundAttribute(this, attr);
                    if (a->dynamic)
                        reportError(Exception::attributeError, "A function cannot have the dynamic attribute", p->pos);
                    Frame *topFrame = env->getTopFrame();
                    if (topFrame->kind == ClassKind) {
                        switch (a->memberMod) {
                        case Attribute::Static:
                            validateStatic(cxt, env, &f->function, a, false, false, p->pos);
                            break;
                        case Attribute::NoModifier:
                            if (*f->function.name == (checked_cast<JS2Class *>(topFrame))->name)
                                validateConstructor(cxt, env, &f->function, checked_cast<JS2Class *>(topFrame), a, p->pos);
                            else
                                validateInstance(cxt, env, &f->function, checked_cast<JS2Class *>(topFrame), a, false, p->pos);
                            break;
                        case Attribute::Virtual:
                            validateInstance(cxt, env, &f->function, checked_cast<JS2Class *>(topFrame), a, false, p->pos);
                            break;
                        case Attribute::Final:
                            validateInstance(cxt, env, &f->function, checked_cast<JS2Class *>(topFrame), a, true, p->pos);
                            break;
                        }
                    }
                    else {
                        if (a->memberMod != Attribute::NoModifier)
                            reportError(Exception::attributeError, "Non-class-member functions cannot have a static, virtual or final attribute", p->pos);
                        // discover Plain[function] by looking for typed parameters or result
                        VariableBinding *vb = f->function.parameters;
                        bool untyped = (f->function.resultType == NULL);
                        if (untyped) {
                            while (vb) {
                                if (vb->type) {
                                    untyped = false;
                                    break;
                                }
                                vb = vb->next;
                            }
                        }
                        bool unchecked = !cxt->strict && (f->function.prefix == FunctionName::normal) && untyped;
                        bool hoisted = unchecked 
                                        && (f->attributes == NULL)
                                        && ((topFrame->kind == PackageKind)
                                                        || (topFrame->kind == BlockFrameKind)
                                                        || (topFrame->kind == ParameterFrameKind));
                        validateStatic(cxt, env, &f->function, a, unchecked, hoisted, p->pos);
                    }
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
                
                    VariableBinding *vb = vs->bindings;
                    Frame *regionalFrame = *(env->getRegionalFrame());
                    while (vb)  {
						ASSERT(vb->name);
                        const StringAtom &name = *vb->name;
                        if (vb->type)
                            ValidateTypeExpression(cxt, env, vb->type);
                        vb->member = NULL;
                        if (vb->initializer)
                            ValidateExpression(cxt, env, vb->initializer);

                        if (!cxt->strict && ((regionalFrame->kind == PackageKind)
                                            || (regionalFrame->kind == ParameterFrameKind))
                                        && !immutable
                                        && (vs->attributes == NULL)
                                        && (vb->type == NULL)) {
                            defineHoistedVar(env, name, JS2VAL_UNDEFINED, true, p->pos);
                        }
                        else {
                            a = Attribute::toCompoundAttribute(this, attr);
                            if (a->dynamic || a->prototype)
                                reportError(Exception::definitionError, "Illegal attribute", p->pos);
                            Attribute::MemberModifier memberMod = a->memberMod;
                            if ((env->getTopFrame()->kind == ClassKind)
                                    && (memberMod == Attribute::NoModifier))
                                memberMod = Attribute::Final;
                            switch (memberMod) {
                            case Attribute::NoModifier:
                            case Attribute::Static: 
                                {
                                    // Set type to FUTURE_TYPE - it will be resolved during 'Setup'. The value is either FUTURE_VALUE
                                    // for 'const' - in which case the expression is compile time evaluated (or attempted) or set
                                    // to INACCESSIBLE until run time initialization occurs.
                                    Variable *v = new Variable(FUTURE_TYPE, immutable ? JS2VAL_FUTUREVALUE : JS2VAL_INACCESSIBLE, immutable);
                                    vb->member = v;
                                    v->vb = vb;
                                    vb->mn = defineLocalMember(env, name, &a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
                                    bCon->saveMultiname(vb->mn);
                                }
                                break;
                            case Attribute::Virtual:
                            case Attribute::Final: 
                                {
                                    Multiname *mn = new (this) Multiname(name, a->namespaces);
                                    JS2Class *c = checked_cast<JS2Class *>(env->getTopFrame());
                                    InstanceMember *m = new InstanceVariable(mn, FUTURE_TYPE, immutable, (memberMod == Attribute::Final), true, c->slotCount++);
                                    vb->member = m;
                                    vb->overridden = defineInstanceMember(c, cxt, name, a->namespaces, a->overrideMod, a->xplicit, m, p->pos);
                                }
                                break;
                            default:
                                reportError(Exception::definitionError, "Illegal attribute", p->pos);
                                break;
                            }
                        }
                        vb = vb->next;
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
                    a = Attribute::toCompoundAttribute(this, attr);
                    if (a->dynamic || a->prototype)
                        reportError(Exception::definitionError, "Illegal attribute", p->pos);
                    if ( ! ((a->memberMod == Attribute::NoModifier) || ((a->memberMod == Attribute::Static) && (env->getTopFrame()->kind == ClassKind))) )
                        reportError(Exception::definitionError, "Illegal attribute", p->pos);
                    Variable *v = new Variable(namespaceClass, OBJECT_TO_JS2VAL(new (this) Namespace(ns->name)), true);
                    defineLocalMember(env, ns->name, &a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
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
                    a = Attribute::toCompoundAttribute(this, attr);
                    if (!superClass->complete || superClass->final)
                        reportError(Exception::definitionError, "Illegal inheritance", p->pos);
                    js2val protoVal = JS2VAL_NULL;
                    bool final = false;
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
                    JS2Class *c = new (this) JS2Class(superClass, protoVal, new (this) Namespace(engine->private_StringAtom), (a->dynamic || superClass->dynamic), final, classStmt->name);
                    classStmt->c = c;
                    Variable *v = new Variable(classClass, OBJECT_TO_JS2VAL(c), true);
                    defineLocalMember(env, classStmt->name, &a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
                    if (classStmt->body) {
                        env->addFrame(c);
                        ValidateStmtList(cxt, env, pl, classStmt->body->statements);
                        ASSERT(env->getTopFrame() == c);
                        env->removeTopFrame();
                    }
                    if (c->init == NULL)
                        c->init = superClass->init;
                    c->complete = true;
                }
                break;
            case StmtNode::With:
                {
                    UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                    ValidateExpression(cxt, env, w->expr);
                    if (w->stmt->getKind() != StmtNode::block) {
                        w->compileFrame = new (this) BlockFrame();
                        env->addFrame(w->compileFrame);
                        ValidateStmt(cxt, env, pl, w->stmt);
                        env->removeTopFrame();
                    }
                    else
                        ValidateStmt(cxt, env, pl, w->stmt);
                }
                break;
            case StmtNode::empty:
                break;
            case StmtNode::Package:
                {
                    PackageStmtNode *ps = checked_cast<PackageStmtNode *>(p);
                    String packageName = getPackageName(ps->packageIdList);
                    Package *package = new (this) Package(packageName, new (this) Namespace(world.identifiers["internal"]));

                    Variable *v = new Variable(packageClass, OBJECT_TO_JS2VAL(package), true);
                    defineLocalMember(env, world.identifiers[packageName], NULL, Attribute::NoOverride, false, ReadAccess, v, 0, true);
                    
                    package->status = Package::InTransit;
                    packages.push_back(package);
                    env->addFrame(package);
                    ValidateStmt(cxt, env, pl, ps->body);
                    env->removeTopFrame();
                    package->status = Package::InHand;
                }
                break;
            case StmtNode::Import:
                {
                    ImportStmtNode *i = checked_cast<ImportStmtNode *>(p);
                    String packageName;
                    if (i->packageIdList)
						packageName = getPackageName(i->packageIdList);
                    else
                        packageName = *i->packageString;

                    if (!checkForPackage(packageName, i->pos))
                        loadPackage(packageName, packageName + ".js");

                    Multiname mn(world.identifiers[packageName], publicNamespace);
                    js2val packageValue;
                    env->lexicalRead(this, &mn, CompilePhase, &packageValue, NULL);
                    if (JS2VAL_IS_VOID(packageValue) || JS2VAL_IS_NULL(packageValue) || !JS2VAL_IS_OBJECT(packageValue)
                            || (JS2VAL_TO_OBJECT(packageValue)->kind != PackageKind))
                        reportError(Exception::badValueError, "Package expected in Import directive", i->pos);

                    Package *package = checked_cast<Package *>(JS2VAL_TO_OBJECT(packageValue));            
                    if (i->varName) {
                        Variable *v = new Variable(packageClass, packageValue, true);
                        defineLocalMember(env, *i->varName, NULL, Attribute::NoOverride, false, ReadAccess, v, 0, true);
                    }
#if 0

                    // defineVariable(m_cx, *i->varName, NULL, Package_Type, JSValue::newPackage(package));
            
                    // scan all local bindings in 'package' and handle the alias-ing issue...
                    for (PropertyIterator it = package->mProperties.begin(), end = package->mProperties.end();
                                (it != end); it++)
                    {
                        ASSERT(PROPERTY_KIND(it) == ValuePointer);
                        bool makeAlias = true;
                        if (i->includeExclude) {
                            makeAlias = i->exclude;
                            IdentifierList *idList = i->includeExclude;
                            while (idList) {
                                if (idList->name.compare(PROPERTY_NAME(it)) == 0) {
                                    makeAlias = !makeAlias;
                                    break;
                                }
                                idList = idList->next;
                            }
                        }
                        if (makeAlias)
                            defineAlias(m_cx, PROPERTY_NAME(it), PROPERTY_NAMESPACELIST(it), PROPERTY_ATTR(it), PROPERTY_TYPE(it), PROPERTY_VALUEPOINTER(it));
                    }
#endif
                }
                break;
            default:
                NOT_REACHED("Not Yet Implemented");
            }   // switch (p->getKind())
        }
        catch (Exception x) {
            env->setTopFrame(curTopFrame);
            throw x;
        }
    }

    /*
        Build a name for the package from the identifier list
    */
    String JS2Metadata::getPackageName(IdentifierList *packageIdList)
    {
        String packagePath;
        IdentifierList *idList = packageIdList;
        while (idList) {
            packagePath += idList->name;
            idList = idList->next;
            if (idList)
                packagePath += '/'; // XXX how to get path separator for OS?
        }
        return packagePath;
    }

    /*
        See if the specified package is already loaded - return true
        Throw an exception if the package is being loaded already
    */
    bool JS2Metadata::checkForPackage(const String &packageName, size_t pos)
    {
        // XXX linear search 
        for (PackageList::iterator pi = packages.begin(), end = packages.end(); (pi != end); pi++) {
            if ((*pi)->name.compare(packageName) == 0) {
                if ((*pi)->status == Package::InTransit)
                    reportError(Exception::referenceError, "Package circularity", pos);
                else
                    return true;
            }
        }
        return false;
    }

    /*
        Load the specified package from the file
    */
    void JS2Metadata::loadPackage(const String & /*packageName*/, const String &filename)
    {
        // XXX need some rules for search path
        // XXX need to extract just the target package from the file
        readEvalFile(filename);
    }

    JS2Class *JS2Metadata::getVariableType(Variable *v, Phase phase, size_t pos)
    {
        JS2Class *type = v->type;
        if (type == NULL) { // Inaccessible, Note that this can only happen when phase = compile 
                            // because the compilation phase ensures that all types are valid, 
                            // so invalid types will not occur during the run phase.          
            ASSERT(phase == CompilePhase);
            reportError(Exception::compileExpressionError, "No type assigned", pos);
        }
        else {
            if (v->type == FUTURE_TYPE) {
                // Note that phase = compile because all futures are resolved by the end of the compilation phase.
                ASSERT(phase == CompilePhase);
                if (v->vb->type) {
                    v->type = NULL;
                    v->type = EvalTypeExpression(env, CompilePhase, v->vb->type);
                }
                else
                    v->type = objectClass;
            }
        }
        return v->type;
    }

    /*
     * Process an individual statement 'p', including it's children
     *  - this generates bytecode for each statement, but doesn't actually
     * execute it.
     */
    void JS2Metadata::SetupStmt(Environment *env, Phase phase, StmtNode *p) 
    {
        JS2Class *exprType;
        switch (p->getKind()) {
        case StmtNode::block:
        case StmtNode::group:
            {
                targetList.push_back(p);
                bool pushed = false;
                BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
                if (b->compileFrame->isFunctionFrame || b->compileFrame->localBindings.size()) {
                    bCon->emitOp(ePushFrame, p->pos);
                    bCon->addFrame(b->compileFrame);
                    pushed = true;
                }
                env->addFrame(b->compileFrame);
                StmtNode *bp = b->statements;
                while (bp) {
                    SetupStmt(env, phase, bp);
                    bp = bp->next;
                }
                if (pushed)
                    bCon->emitOp(ePopFrame, p->pos);
                env->removeTopFrame();
                targetList.pop_back();
            }
            break;
        case StmtNode::label:
            {
                LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                targetList.push_back(p);
                SetupStmt(env, phase, l->stmt);
                targetList.pop_back();
                // labelled statements target are break targets
                bCon->setLabel(l->labelID);
            }
            break;
        case StmtNode::If:
            {
                BytecodeContainer::LabelID skipOverStmt = bCon->getLabel();
                UnaryStmtNode *i = checked_cast<UnaryStmtNode *>(p);
                Reference *r = SetupExprNode(env, phase, i->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchFalse, skipOverStmt, p->pos);
                SetupStmt(env, phase, i->stmt);
                bCon->setLabel(skipOverStmt);
            }
            break;
        case StmtNode::IfElse:
            {
                BytecodeContainer::LabelID falseStmt = bCon->getLabel();
                BytecodeContainer::LabelID skipOverFalseStmt = bCon->getLabel();
                BinaryStmtNode *i = checked_cast<BinaryStmtNode *>(p);
                Reference *r = SetupExprNode(env, phase, i->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchFalse, falseStmt, p->pos);
                SetupStmt(env, phase, i->stmt);
                bCon->emitBranch(eBranch, skipOverFalseStmt, p->pos);
                bCon->setLabel(falseStmt);
                SetupStmt(env, phase, i->stmt2);
                bCon->setLabel(skipOverFalseStmt);
            }
            break;
        case StmtNode::Break:
            // XXX for break - if there's a finally that applies to this block, it should
            // be invoked at this point - need to track the appropriate label and emit
            // eCallFinally here.
        case StmtNode::Continue:
            {
                GoStmtNode *g = checked_cast<GoStmtNode *>(p);
                bCon->emitBranch(eBreak, g->tgtID, p->pos);
                uint32 blockCount = 0;
                bool foundit = false;
                for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); 
                            ((si != end) && !foundit); si++)
                {
                    switch ((*si)->getKind()) {
                        case StmtNode::label:
                            {
                                LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                if (g->tgtID == l->labelID)
                                    foundit = true;
                            }
                            break;
                        case StmtNode::While:
                        case StmtNode::DoWhile:
                            {
                                UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(*si);
                                if ((g->tgtID == w->breakLabelID) || (g->tgtID == w->continueLabelID))
                                    foundit = true;
                            }
                            break;
                        case StmtNode::For:
                        case StmtNode::ForIn:
                            {
                                ForStmtNode *f = checked_cast<ForStmtNode *>(*si);
                                if ((g->tgtID == f->breakLabelID) || (g->tgtID == f->continueLabelID))
                                    foundit = true;
                            }
                            break;
                        case StmtNode::Switch:
                            {
                                SwitchStmtNode *s = checked_cast<SwitchStmtNode *>(*si);
                                if (g->tgtID == s->breakLabelID)
                                    foundit = true;
                            }
                            break;
                        case StmtNode::block:
                            {
                                BlockStmtNode *b = checked_cast<BlockStmtNode *>(*si);
                                if (b->compileFrame->isFunctionFrame || b->compileFrame->localBindings.size())
                                    blockCount++;
                            }
                            break;
                    }
                }
                ASSERT(foundit); 
                bCon->addShort(blockCount);
            }
            break;
        case StmtNode::ForIn:
/*
            iterator = get_first_property_of(object) [eFirst]
            //
            // pushes iterator object on stack, returns true/false
            //
            if (false) --> break;
            top:
                v <-- iterator.value  // v is the thing specified by for ('v' in object) ...
                <statement body>
            continue:
                //
                // expect iterator object on top of stack
                // increment it. Returns true/false
                //
                iterator = get_next_property_of(object, iterator) [eNext]
                if (true) --> top;
            break:
                // want stack cleared of iterator at this point
*/
            {
                ForStmtNode *f = checked_cast<ForStmtNode *>(p);
                BytecodeContainer::LabelID loopTop = bCon->getLabel();

                Reference *r = SetupExprNode(env, phase, f->expr2, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eFirst, p->pos);
                bCon->emitBranch(eBranchFalse, f->breakLabelID, p->pos);

                bCon->setLabel(loopTop);                
                
                bCon->emitOp(eForValue, p->pos);

                Reference *v = NULL;
                if (f->initializer->getKind() == StmtNode::Var) {
                    VariableStmtNode *vs = checked_cast<VariableStmtNode *>(f->initializer);
                    VariableBinding *vb = vs->bindings;
                    v = new (*referenceArena) LexicalReference(new (this) Multiname(*vb->name), cxt.strict, bCon);
                    referenceArena->registerDestructor(v);
                }
                else {
                    if (f->initializer->getKind() == StmtNode::expression) {
                        ExprStmtNode *e = checked_cast<ExprStmtNode *>(f->initializer);
                        v = SetupExprNode(env, phase, e->expr, &exprType);
                        if (v == NULL)
                            reportError(Exception::semanticError, "for..in needs an lValue", p->pos);
                    }
                    else
                        NOT_REACHED("what else??");
                }            
                switch (v->hasStackEffect()) {
                case 1:
                    bCon->emitOp(eSwap, p->pos);
                    break;
                case 2:
                    bCon->emitOp(eSwap2, p->pos);
                    break;
                }
                v->emitWriteBytecode(bCon, p->pos);
                bCon->emitOp(ePop, p->pos);     // clear iterator value from stack
                targetList.push_back(p);
                SetupStmt(env, phase, f->stmt);
                targetList.pop_back();
                bCon->setLabel(f->continueLabelID);
                bCon->emitOp(eNext, p->pos);
                bCon->emitBranch(eBranchTrue, loopTop, p->pos);
                bCon->setLabel(f->breakLabelID);
                bCon->emitOp(ePop, p->pos);
            }
            break;
        case StmtNode::For:
            {
                ForStmtNode *f = checked_cast<ForStmtNode *>(p);
                BytecodeContainer::LabelID loopTop = bCon->getLabel();
                BytecodeContainer::LabelID testLocation = bCon->getLabel();

                if (f->initializer)
                    SetupStmt(env, phase, f->initializer);
                if (f->expr2)
                    bCon->emitBranch(eBranch, testLocation, p->pos);
                bCon->setLabel(loopTop);
                targetList.push_back(p);
                SetupStmt(env, phase, f->stmt);
                targetList.pop_back();
                bCon->setLabel(f->continueLabelID);
                if (f->expr3) {
                    Reference *r = SetupExprNode(env, phase, f->expr3, &exprType);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    bCon->emitOp(ePop, p->pos);
                }
                bCon->setLabel(testLocation);
                if (f->expr2) {
                    Reference *r = SetupExprNode(env, phase, f->expr2, &exprType);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    bCon->emitBranch(eBranchTrue, loopTop, p->pos);
                }
                else
                    bCon->emitBranch(eBranch, loopTop, p->pos);
                bCon->setLabel(f->breakLabelID);
            }
            break;
        case StmtNode::Switch:
/*
            <swexpr>        
            eSlotWrite    <switchTemp>

        // test sequence in source order except 
        // the default is moved to end.

            eSlotRead    <switchTemp>
            <case1expr>
            Equal
            BranchTrue --> case1StmtLabel
            eLexicalRead    <switchTemp>
            <case2expr>
            Equal
            BranchTrue --> case2StmtLabel
            Branch --> default, if there is one, or break label

    case1StmtLabel:
            <stmt>
    case2StmtLabel:
            <stmt>
    defaultLabel:
            <stmt>
    case3StmtLabel:
            <stmt>
            ..etc..     // all in source order
    
    breakLabel:
*/
            {
                SwitchStmtNode *sw = checked_cast<SwitchStmtNode *>(p);
                FrameListIterator fi = env->getRegionalFrame();
                NonWithFrame *regionalFrame = checked_cast<NonWithFrame *>(*fi);
                if (regionalFrame->kind == ParameterFrameKind)
                    regionalFrame = checked_cast<NonWithFrame *>(*--fi);
                FrameVariable *frV = makeFrameVariable(regionalFrame);
                ASSERT(frV->kind != FrameVariable::Parameter);
                Reference *switchTemp;
                if (frV->kind == FrameVariable::Package)
                    switchTemp = new (*referenceArena) PackageSlotReference(frV->frameSlot);
                else
                    switchTemp = new (*referenceArena) FrameSlotReference(frV->frameSlot);
                BytecodeContainer::LabelID defaultLabel = NotALabel;

                Reference *r = SetupExprNode(env, phase, sw->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                switchTemp->emitWriteBytecode(bCon, p->pos);
                bCon->emitOp(ePopv, p->pos);


                // First time through, generate the conditional waterfall 
                StmtNode *s = sw->statements;
                while (s) {
                    if (s->getKind() == StmtNode::Case) {
                        ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                        if (c->expr) {
                            switchTemp->emitReadBytecode(bCon, p->pos);
                            Reference *r = SetupExprNode(env, phase, c->expr, &exprType);
                            if (r) r->emitReadBytecode(bCon, c->pos);
                            bCon->emitOp(eIdentical, c->pos);
                            bCon->emitBranch(eBranchTrue, c->labelID, c->pos);
                        }
                        else
                            defaultLabel = c->labelID;
                    }
                    s = s->next;
                }
                if (defaultLabel == NotALabel)
                    bCon->emitBranch(eBranch, sw->breakLabelID, p->pos);
                else
                    bCon->emitBranch(eBranch, defaultLabel, p->pos);
                // Now emit the contents
                targetList.push_back(p);
                s = sw->statements;
                while (s) {
                    if (s->getKind() == StmtNode::Case) {
                        ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                        bCon->setLabel(c->labelID);
                    }
                    else
                        SetupStmt(env, phase, s);
                    s = s->next;
                }
                targetList.pop_back();

                bCon->setLabel(sw->breakLabelID);
				delete frV;
            }
            break;
        case StmtNode::While:
            {
                UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                BytecodeContainer::LabelID loopTop = bCon->getLabel();
                bCon->emitBranch(eBranch, w->continueLabelID, p->pos);
                bCon->setLabel(loopTop);
                targetList.push_back(p);
                SetupStmt(env, phase, w->stmt);
                targetList.pop_back();
                bCon->setLabel(w->continueLabelID);
                Reference *r = SetupExprNode(env, phase, w->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchTrue, loopTop, p->pos);
                bCon->setLabel(w->breakLabelID);
            }
            break;
        case StmtNode::DoWhile:
            {
                UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                BytecodeContainer::LabelID loopTop = bCon->getLabel();
                bCon->setLabel(loopTop);
                targetList.push_back(p);
                SetupStmt(env, phase, w->stmt);
                targetList.pop_back();
                bCon->setLabel(w->continueLabelID);
                Reference *r = SetupExprNode(env, phase, w->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchTrue, loopTop, p->pos);
                bCon->setLabel(w->breakLabelID);
            }
            break;
        case StmtNode::Throw:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                Reference *r = SetupExprNode(env, phase, e->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eThrow, p->pos);
            }
            break;
        case StmtNode::Try:
// Your logic is insane and happenstance, like that of a troll
/*
            try {   //  [catchLabel,finallyInvoker] handler labels are pushed on handler stack [eTry]
                    <tryblock>
                }   //  catch handler label is popped off handler stack [eHandler]
                jsr finally
                jump-->finished                 

            finally:        // finally handler label popped off here.
                {           // A throw from in here goes to the next handler on the
                            // handler stack
                }
                rts

            finallyInvoker:         // invoked when an exception is caught in the try block
                push exception      // it arranges to call the finally block and then re-throw
                jsr finally         // the exception - reaching the catch block
                throw exception
            catchLabel:

                    the incoming exception is on the top of the stack at this point

                catch (exception) { // catch handler label popped off   [eHandler]
                        // any throw from in here must jump to the next handler
                        // (i.e. not the this same catch handler!)

                    Of the many catch clauses specified, only the one whose exception variable type
                    matches the type of the incoming exception is executed...

                    dup
                    push type of exception-variable
                    is
                    jumpfalse-->next catch
                    
                    setlocalvar exception-variable
                    pop
                    <catch body>


                }
                // 'normal' fall thru from catch
                jsr finally
                jump finished

            finished:
*/
            {
                TryStmtNode *t = checked_cast<TryStmtNode *>(p);
                BytecodeContainer::LabelID catchClauseLabel;
                BytecodeContainer::LabelID finallyInvokerLabel;
                BytecodeContainer::LabelID t_finallyLabel;
                bCon->emitOp(eTry, p->pos);
                if (t->finally) {
                    finallyInvokerLabel = bCon->getLabel();
                    bCon->addFixup(finallyInvokerLabel);            
                    t_finallyLabel = bCon->getLabel(); 
                }
                else {
                    finallyInvokerLabel = NotALabel;
                    bCon->addOffset(NotALabel);
                    t_finallyLabel = NotALabel;
                }
                if (t->catches) {
                    catchClauseLabel = bCon->getLabel();
                    bCon->addFixup(catchClauseLabel);            
                }
                else {
                    catchClauseLabel = NotALabel;
                    bCon->addOffset(NotALabel);
                }
                BytecodeContainer::LabelID finishedLabel = bCon->getLabel();
                SetupStmt(env, phase, t->stmt);

                if (t->finally) {
                    bCon->emitBranch(eCallFinally, t_finallyLabel, p->pos);
                    bCon->emitBranch(eBranch, finishedLabel, p->pos);

                    bCon->setLabel(t_finallyLabel);
                    bCon->emitOp(eHandler, p->pos);
                    SetupStmt(env, phase, t->finally);
                    bCon->emitOp(eReturnFinally, p->pos);

                    bCon->setLabel(finallyInvokerLabel);
                    // the exception object is on the top of the stack already
                    bCon->emitBranch(eCallFinally, t_finallyLabel, p->pos);
                    ASSERT(bCon->mStackTop == 0);
                    bCon->mStackTop = 1;
                    bCon->emitOp(eThrow, p->pos);
                }
                else {
                    bCon->emitBranch(eBranch, finishedLabel, p->pos);
                }

                if (t->catches) {
                    bCon->setLabel(catchClauseLabel);
                    bCon->emitOp(eHandler, p->pos);
                    CatchClause *c = t->catches;
                    // the exception object will be the only thing on the stack
//                    ASSERT(bCon->mStackTop == 0);
                    bCon->mStackTop = 1;
                    if (bCon->mStackMax < 1) bCon->mStackMax = 1;
                    BytecodeContainer::LabelID nextCatch = NotALabel;
                    while (c) {                    
                        if (c->next && c->type) {
                            nextCatch = bCon->getLabel();
                            bCon->emitOp(eDup, p->pos);
                            Reference *r = SetupExprNode(env, phase, c->type, &exprType);
                            if (r) r->emitReadBytecode(bCon, p->pos);
                            bCon->emitOp(eIs, p->pos);
                            bCon->emitBranch(eBranchFalse, nextCatch, p->pos);
                        }
                        // write the exception object (on stack top) into the named
                        // local variable
                        Reference *r = new (*referenceArena) LexicalReference(new (this) Multiname(c->name), false, bCon);
                        referenceArena->registerDestructor(r);
                        r->emitWriteBytecode(bCon, p->pos);
                        bCon->emitOp(ePop, p->pos);
                        SetupStmt(env, phase, c->stmt);
                        if (t->finally) {
                            bCon->emitBranch(eCallFinally, t_finallyLabel, p->pos);
                        }
                        c = c->next;
                        if (c) {
                            bCon->emitBranch(eBranch, finishedLabel, p->pos);
                            bCon->mStackTop = 1;
                            if (nextCatch != NotALabel)
                                bCon->setLabel(nextCatch);
                        }
                    }
                }
                bCon->setLabel(finishedLabel);
            }
            break;
        case StmtNode::Return:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                if (e->expr) {
                    Reference *r = SetupExprNode(env, phase, e->expr, &exprType);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    bCon->emitOp(eReturn, p->pos);
                }
                else
                    bCon->emitOp(eReturnVoid, p->pos);
            }
            break;
        case StmtNode::Function:
            {
                FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
                CompilationData *oldData = NULL;
                FunctionInstance *fnInst = f->function.fn;
                try {
                    oldData = startCompilationUnit(fnInst->fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
                    env->addFrame(fnInst->fWrap->compileFrame);
                    bCon->fName = *f->function.name;
                    VariableBinding *pb = f->function.parameters;
                    while (pb) {
						if (pb->member) {
							FrameVariable *v = checked_cast<FrameVariable *>(pb->member);
							if (pb->type)
								v->type = EvalTypeExpression(env, CompilePhase, pb->type);
							else
								v->type = objectClass;
						}
                        pb = pb->next;
                    }
                    if (f->function.resultType)
                        fnInst->fWrap->resultType = EvalTypeExpression(env, CompilePhase, f->function.resultType);
                    else
                        fnInst->fWrap->resultType = objectClass;

                    SetupStmt(env, phase, f->function.body);
                    // XXX need to make sure that all paths lead to an exit of some kind
                    bCon->emitOp(eReturnVoid, p->pos);
                    env->removeTopFrame();
                    restoreCompilationUnit(oldData);
                }
                catch (Exception &x) {
                    if (oldData)
                        restoreCompilationUnit(oldData);
                    throw x;
                }
                bCon->emitOp(eClosure, p->pos);
                bCon->addObject(fnInst);
            }
            break;
        case StmtNode::Var:
        case StmtNode::Const:
            {
                // Note that the code here is the Setup code plus the emit of the Eval bytecode
                VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);                
                VariableBinding *vb = vs->bindings;
                while (vb)  {
                    if (vb->member) {   // static or instance variable
                        if (vb->member->memberKind == Member::VariableMember) {
                            Variable *v = checked_cast<Variable *>(vb->member);
                            JS2Class *type = getVariableType(v, CompilePhase, p->pos);
                            if (JS2VAL_IS_FUTURE(v->value)) {   // it's a const, execute the initializer
                                v->value = JS2VAL_INACCESSIBLE;
                                if (vb->initializer) {
                                    try {
                                        js2val newValue = EvalExpression(env, CompilePhase, vb->initializer);
                                        v->value = type->ImplicitCoerce(this, newValue);
                                    }
                                    catch (Exception x) {
                                        // If a compileExpressionError occurred, then the initialiser is 
                                        // not a compile-time constant expression. In this case, ignore the
                                        // error and leave the value of the variable INACCESSIBLE until it
                                        // is defined at run time.
                                        if (x.kind != Exception::compileExpressionError)
                                            throw x;
                                        Reference *r = SetupExprNode(env, phase, vb->initializer, &exprType);
                                        if (r) r->emitReadBytecode(bCon, p->pos);
                                        LexicalReference *lVal = new (*referenceArena) LexicalReference(new (this) Multiname(vb->mn), cxt.strict, bCon);
                                        referenceArena->registerDestructor(lVal);
                                        lVal->emitWriteBytecode(bCon, p->pos);      
                                        bCon->emitOp(ePop, p->pos);
                                    }
                                }
                                else
                                    // Would only have come here if the variable was immutable - i.e. a 'const' definition
                                    // XXX why isn't this handled at validation-time?
                                    reportError(Exception::compileExpressionError, "Missing compile time expression", p->pos);
                            }
                            else {
                                // Not immutable
                                ASSERT(JS2VAL_IS_INACCESSIBLE(v->value));
                                if (vb->initializer) {
                                    Reference *r = SetupExprNode(env, phase, vb->initializer, &exprType);
                                    if (r) r->emitReadBytecode(bCon, p->pos);
                                    bCon->emitOp(eCoerce, p->pos);
                                    bCon->addType(v->type);
                                    LexicalReference *lVal = new (*referenceArena) LexicalReference(new (this) Multiname(vb->mn), cxt.strict, bCon);
                                    referenceArena->registerDestructor(lVal);
                                    lVal->emitInitBytecode(bCon, p->pos);      
                                }
                                else {
                                    v->type->emitDefaultValue(bCon, p->pos);
                                    LexicalReference *lVal = new (*referenceArena) LexicalReference(new (this) Multiname(vb->mn), cxt.strict, bCon);
                                    referenceArena->registerDestructor(lVal);
                                    lVal->emitInitBytecode(bCon, p->pos);      
                                }
                            }
                        }
                        else {
                            InstanceVariable *v = checked_cast<InstanceVariable *>(vb->member);
                            JS2Class *t;
                            if (vb->type)
                                t = EvalTypeExpression(env, CompilePhase, vb->type);
                            else {
                                if (vb->overridden) {
                                    switch (vb->overridden->memberKind) {
                                    case Member::InstanceVariableMember:
                                        t = checked_cast<InstanceVariable *>(vb->overridden)->type;
                                        break;
                                    case Member::InstanceGetterMember:
                                        t = checked_cast<InstanceGetter *>(vb->overridden)->type;
                                        break;
                                    case Member::InstanceSetterMember:
                                        t = checked_cast<InstanceSetter *>(vb->overridden)->type;
                                        break;
                                    case Member::InstanceMethodMember:
                                        //t = checked_cast<InstanceMethod *>(vb->overridden)->type;
                                        t = objectClass;
                                        break;
                                    }
                                }
                                else
                                    t = objectClass;
                            }
                            v->type = t;
                            if (vb->initializer) {
                                js2val newValue = EvalExpression(env, CompilePhase, vb->initializer);
                                v->defaultValue = t->ImplicitCoerce(this, newValue);
                            }
                            else
                                v->defaultValue = t->defaultValue;
                        }
                    }
                    else { // HoistedVariable
                        if (vb->initializer) {
                            Reference *r = SetupExprNode(env, phase, vb->initializer, &exprType);
                            if (r) r->emitReadBytecode(bCon, p->pos);
                            LexicalReference *lVal = new (*referenceArena) LexicalReference(new (this) Multiname(*vb->name), cxt.strict, bCon);
                            referenceArena->registerDestructor(lVal);
                            lVal->variableMultiname->addNamespace(publicNamespace);
                            lVal->emitInitBytecode(bCon, p->pos);                                                        
                        }
                    }
                    vb = vb->next;
                }
            }
            break;
        case StmtNode::expression:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                Reference *r = SetupExprNode(env, phase, e->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                // superStmt expressions don't produce any result value
                if (e->expr->getKind() != ExprNode::superStmt)
                    bCon->emitOp(ePopv, p->pos);
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
                        SetupStmt(env, phase, bp);
                        bp = bp->next;
                    }
                    ASSERT(env->getTopFrame() == c);
                    env->removeTopFrame();
                    bCon->emitOp(ePopFrame, p->pos);
                }
            }
            break;
        case StmtNode::With:
            {
                UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                Reference *r = SetupExprNode(env, phase, w->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eWithin, p->pos);
                if (w->stmt->getKind() != StmtNode::block) {
                    env->addFrame(w->compileFrame);
                    bCon->emitOp(ePushFrame, p->pos);
                    bCon->addFrame(w->compileFrame);
                    SetupStmt(env, phase, w->stmt);                        
                    bCon->emitOp(ePopFrame, p->pos);
                    env->removeTopFrame();
                }
                else
                    SetupStmt(env, phase, w->stmt);                        
                bCon->emitOp(eWithout, p->pos);
            }
            break;
        case StmtNode::empty:
            break;
        case StmtNode::Import:
            break;
        case StmtNode::Package:
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
                return new (this) TrueAttribute();
            else
                return new (this) FalseAttribute();
        case ExprNode::juxtapose:
            {
                BinaryExprNode *j = checked_cast<BinaryExprNode *>(p);
                Attribute *a = EvalAttributeExpression(env, phase, j->op1);
                if (a && (a->attrKind == Attribute::FalseAttr))
                    return a;
                Attribute *b = EvalAttributeExpression(env, phase, j->op2);
                try {
                    return Attribute::combineAttributes(this, a, b);
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
                case Token::Final:
                    ca = new (this) CompoundAttribute();
                    ca->memberMod = Attribute::Final;
                    return ca;
                case Token::Private:
                    {
                        JS2Class *c = env->getEnclosingClass();
                        return c->privateNamespace;
                    }
                case Token::Static:
                    ca = new (this) CompoundAttribute();
                    ca->memberMod = Attribute::Static;
                    return ca;
                case Token::identifier:
                    if (name == world.identifiers["override"]) {
                        ca = new (this) CompoundAttribute();
                        ca->overrideMod = Attribute::DoOverride;
                        return ca;
                    }
                    else
                    if (name == world.identifiers["enumerable"]) {
                        ca = new (this) CompoundAttribute();
                        ca->enumerable = true;
                        return ca;
                    }
                    else
                    if (name == world.identifiers["virtual"]) {
                        ca = new (this) CompoundAttribute();
                        ca->memberMod = Attribute::Virtual;
                        return ca;
                    }
                    else
                    if (name == world.identifiers["dynamic"]) {
                        ca = new (this) CompoundAttribute();
                        ca->dynamic = true;
                        return ca;
                    }
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
    Attribute *Attribute::combineAttributes(JS2Metadata *meta, Attribute *a, Attribute *b)
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
            if (b->attrKind == NamespaceAttr) {
                Namespace *nb = checked_cast<Namespace *>(b);
                CompoundAttribute *c = new (meta) CompoundAttribute();
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
            for (NamespaceListIterator i = cb->namespaces.begin(), end = cb->namespaces.end(); (i != end); i++)
                ca->addNamespace(*i);
            ca->xplicit |= cb->xplicit;
            ca->dynamic |= cb->dynamic;
            if (ca->memberMod == NoModifier)
                ca->memberMod = cb->memberMod;
            if (ca->overrideMod == NoOverride)
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
        for (NamespaceListIterator i = namespaces.begin(), end = namespaces.end(); (i != end); i++)
            if (*i == n)
                return;
        namespaces.push_back(n);
    }

    CompoundAttribute::CompoundAttribute() : Attribute(CompoundAttr),
            namespaces(NULL), xplicit(false), enumerable(false), dynamic(false), memberMod(NoModifier), 
            overrideMod(NoOverride), prototype(false), unused(false) 
    { 
    }

    // Convert an attribute to a compoundAttribute. If the attribute
    // is NULL, return a default compoundAttribute
    CompoundAttribute *Attribute::toCompoundAttribute(JS2Metadata *meta, Attribute *a)
    { 
        if (a) 
            return a->toCompoundAttribute(); 
        else
            return new (meta) CompoundAttribute();
    }

    // Convert a simple namespace to a compoundAttribute with that namespace
    CompoundAttribute *Namespace::toCompoundAttribute(JS2Metadata *meta)    
    { 
        CompoundAttribute *t = new (meta) CompoundAttribute(); 
        t->addNamespace(this); 
        return t; 
    }

    // Convert a 'true' attribute to a default compoundAttribute
    CompoundAttribute *TrueAttribute::toCompoundAttribute(JS2Metadata *meta)    
    { 
        return new (meta) CompoundAttribute(); 
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void CompoundAttribute::markChildren()
    {
        for (NamespaceListIterator i = namespaces.begin(), end = namespaces.end(); (i != end); i++) {
            GCMARKOBJECT(*i)
        }
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
        case ExprNode::Null:
        case ExprNode::number:
        case ExprNode::regExp:
        case ExprNode::numUnit:
        case ExprNode::string:
        case ExprNode::boolean:
            break;
        case ExprNode::This:
            {
				js2val thisVal;
                ParameterFrame *pFrame = env->getEnclosingParameterFrame(&thisVal);
                if ((pFrame == NULL) || (thisVal == JS2VAL_VOID))
					if (!cxt->E3compatibility)
						reportError(Exception::syntaxError, "No 'this' available", p->pos);
            }
            break;
        case ExprNode::superExpr:
            {
                SuperExprNode *s = checked_cast<SuperExprNode *>(p);
                JS2Class *c = env->getEnclosingClass();
                if (s->op) {
                    if (c == NULL)                    
                        reportError(Exception::syntaxError, "No 'super' available", p->pos);
                    ValidateExpression(cxt, env, s->op);
                }
                else {
                    ParameterFrame *pFrame = env->getEnclosingParameterFrame(NULL);
                    if ((c == NULL) || (pFrame == NULL) || !(pFrame->isConstructor || pFrame->isInstance))
                        reportError(Exception::syntaxError, "No 'super' available", p->pos);
                    if (c->super == NULL)
                        reportError(Exception::definitionError, "No 'super' for this class", p->pos);
                }
            }
            break;
        case ExprNode::objectLiteral:
			{
                PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
                ExprPairList *e = plen->pairs;
                while (e) {
                    ASSERT(e->field && e->value);
                    ValidateExpression(cxt, env, e->value);
                    e = e->next;
                }
			}
            break;
        case ExprNode::arrayLiteral:
            {
                PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
                ExprPairList *e = plen->pairs;
                while (e) {
                    if (e->value)
                        ValidateExpression(cxt, env, e->value);
                    e = e->next;
                }
            }
			break;
        case ExprNode::index:
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                ValidateExpression(cxt, env, i->op);
                ExprPairList *ep = i->pairs;
                uint16 positionalCount = 0;
                // XXX errors below should only occur at runtime - insert code to throw exception
                // or let the bytecodes handle (and throw on) multiple & named arguments?
                while (ep) {
                    if (ep->field)
                        reportError(Exception::argumentMismatchError, "Indexing doesn't support named arguments", p->pos);
                    else {
                        if (positionalCount)
                            reportError(Exception::argumentMismatchError, "Indexing doesn't support more than 1 argument", p->pos);
                        positionalCount++;
                        ValidateExpression(cxt, env, ep->value);
                    }
                    ep = ep->next;
                }
                if (!positionalCount)
                    reportError(Exception::argumentMismatchError, "Indexing requires at least 1 argument", p->pos);
            }
            break;
        case ExprNode::dot:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                ValidateExpression(cxt, env, b->op1);
                ValidateExpression(cxt, env, b->op2);
            }
            break;

        case ExprNode::lessThan:
        case ExprNode::lessThanOrEqual:
        case ExprNode::greaterThan:
        case ExprNode::greaterThanOrEqual:
        case ExprNode::equal:
        case ExprNode::notEqual:
        case ExprNode::assignment:
        case ExprNode::add:
        case ExprNode::subtract:
        case ExprNode::multiply:
        case ExprNode::divide:
        case ExprNode::modulo:
        case ExprNode::addEquals:
        case ExprNode::subtractEquals:
        case ExprNode::multiplyEquals:
        case ExprNode::divideEquals:
        case ExprNode::moduloEquals:
        case ExprNode::logicalAnd:
        case ExprNode::logicalXor:
        case ExprNode::logicalOr:
        case ExprNode::leftShift:
        case ExprNode::rightShift:
        case ExprNode::logicalRightShift:
        case ExprNode::bitwiseAnd:
        case ExprNode::bitwiseXor:
        case ExprNode::bitwiseOr:
        case ExprNode::leftShiftEquals:
        case ExprNode::rightShiftEquals:
        case ExprNode::logicalRightShiftEquals:
        case ExprNode::bitwiseAndEquals:
        case ExprNode::bitwiseXorEquals:
        case ExprNode::bitwiseOrEquals:
        case ExprNode::logicalAndEquals:
        case ExprNode::logicalXorEquals:
        case ExprNode::logicalOrEquals:
        case ExprNode::comma:
        case ExprNode::Instanceof:
        case ExprNode::identical:
        case ExprNode::notIdentical:
        case ExprNode::In:

            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                ValidateExpression(cxt, env, b->op1);
                ValidateExpression(cxt, env, b->op2);
            }
            break;

        case ExprNode::Delete:
        case ExprNode::minus:
        case ExprNode::plus:
        case ExprNode::complement:
        case ExprNode::postIncrement:
        case ExprNode::postDecrement:
        case ExprNode::preIncrement:
        case ExprNode::preDecrement:
        case ExprNode::parentheses:
        case ExprNode::Typeof:
        case ExprNode::logicalNot:
        case ExprNode::Void: 
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                ValidateExpression(cxt, env, u->op);
            }
            break;

        case ExprNode::conditional:
            {
                TernaryExprNode *c = checked_cast<TernaryExprNode *>(p);
                ValidateExpression(cxt, env, c->op1);
                ValidateExpression(cxt, env, c->op2);
                ValidateExpression(cxt, env, c->op3);
            }
            break;

        case ExprNode::qualify:
        case ExprNode::identifier:
            {
//                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
            }
            break;
        case ExprNode::call:
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
        case ExprNode::functionLiteral:
            {
                FunctionExprNode *f = checked_cast<FunctionExprNode *>(p);
                validateStaticFunction(cxt, env, &f->function, true, true, false, p->pos);
            }
            break;
        case ExprNode::superStmt:
            {
                ParameterFrame *pFrame = env->getEnclosingParameterFrame(NULL);
                if ((pFrame == NULL) || !pFrame->isConstructor) 
                    reportError(Exception::syntaxError, "A super statement is meaningful only inside a constructor", p->pos);

                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                ExprPairList *args = i->pairs;
                while (args) {
                    ValidateExpression(cxt, env, args->value);
                    args = args->next;
                }
                pFrame->callsSuperConstructor = true;
            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        } // switch (p->getKind())
    }



    /*
     * Process the expression (i.e. generate bytecode, but don't execute) rooted at p.
     */
    Reference *JS2Metadata::SetupExprNode(Environment *env, Phase phase, ExprNode *p, JS2Class **exprType)
    {
        Reference *returnRef = NULL;
        *exprType = NULL;
        JS2Op op;

        switch (p->getKind()) {

        case ExprNode::parentheses:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                returnRef = SetupExprNode(env, phase, u->op, exprType);
            }
            break;
        case ExprNode::assignment:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                JS2Class *l_exprType = *exprType;
                if (lVal) {
                    Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    lVal->emitWriteBytecode(bCon, p->pos);
                    *exprType = l_exprType;
                }
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
            }
            break;
        case ExprNode::leftShiftEquals:
            op = eLeftShift;
            goto doAssignBinary;
        case ExprNode::rightShiftEquals:
            op = eRightShift;
            goto doAssignBinary;
        case ExprNode::logicalRightShiftEquals:
            op = eLogicalRightShift;
            goto doAssignBinary;
        case ExprNode::bitwiseAndEquals:
            op = eBitwiseAnd;
            goto doAssignBinary;
        case ExprNode::bitwiseXorEquals:
            op = eBitwiseXor;
            goto doAssignBinary;
        case ExprNode::logicalXorEquals:
            op = eLogicalXor;
            goto doAssignBinary;
        case ExprNode::bitwiseOrEquals:
            op = eBitwiseOr;
            goto doAssignBinary;
        case ExprNode::addEquals:
            op = eAdd;
            goto doAssignBinary;
        case ExprNode::subtractEquals:
            op = eSubtract;
            goto doAssignBinary;
        case ExprNode::multiplyEquals:
            op = eMultiply;
            goto doAssignBinary;
        case ExprNode::divideEquals:
            op = eDivide;
            goto doAssignBinary;
        case ExprNode::moduloEquals:
            op = eModulo;
            goto doAssignBinary;
doAssignBinary:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                JS2Class *l_exprType = *exprType;
                if (lVal) {
                    lVal->emitReadForWriteBackBytecode(bCon, p->pos);
                    Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    *exprType = l_exprType;
                }
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
                bCon->emitOp(op, p->pos);
                lVal->emitWriteBackBytecode(bCon, p->pos);
            }
            break;
        case ExprNode::lessThan:
            op = eLess;
            goto boolBinary;
        case ExprNode::lessThanOrEqual:
            op = eLessEqual;
            goto boolBinary;
        case ExprNode::greaterThan:
            op = eGreater;
            goto boolBinary;
        case ExprNode::greaterThanOrEqual:
            op = eGreaterEqual;
            goto boolBinary;
        case ExprNode::equal:
            op = eEqual;
            goto boolBinary;
        case ExprNode::notEqual:
            op = eNotEqual;
            goto boolBinary;
        case ExprNode::identical:
            op = eIdentical;
            goto boolBinary;
        case ExprNode::notIdentical:
            op = eNotIdentical;
            goto boolBinary;
boolBinary:
            *exprType = booleanClass;
            goto doBinary;

        case ExprNode::leftShift:
            op = eLeftShift;
            goto doBinary;
        case ExprNode::rightShift:
            op = eRightShift;
            goto doBinary;
        case ExprNode::logicalRightShift:
            op = eLogicalRightShift;
            goto doBinary;
        case ExprNode::bitwiseAnd:
            op = eBitwiseAnd;
            goto doBinary;
        case ExprNode::bitwiseXor:
            op = eBitwiseXor;
            goto doBinary;
        case ExprNode::bitwiseOr:
            op = eBitwiseOr;
            goto doBinary;

        case ExprNode::add:
            op = eAdd;
            goto doBinary;
        case ExprNode::subtract:
            op = eSubtract;
            goto doBinary;
        case ExprNode::multiply:
            op = eMultiply;
            goto doBinary;
        case ExprNode::divide:
            op = eDivide;
            goto doBinary;
        case ExprNode::modulo:
            op = eModulo;
            goto doBinary;
doBinary:
            {
                JS2Class *l_exprType, *r_exprType;
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, b->op1, &l_exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                Reference *rVal = SetupExprNode(env, phase, b->op2, &r_exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(op, p->pos);
            }
            break;
        case ExprNode::Void: 
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                SetupExprNode(env, phase, u->op, exprType);
                bCon->emitOp(eVoid, p->pos);
            }
            break;

        case ExprNode::logicalNot:
            op = eLogicalNot;
            goto doUnary;
        case ExprNode::minus:
            op = eMinus;
            goto doUnary;
        case ExprNode::plus:
            op = ePlus;
            goto doUnary;
        case ExprNode::complement:
            op = eComplement;
            goto doUnary;
doUnary:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *rVal = SetupExprNode(env, phase, u->op, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(op, p->pos);
            }
            break;

        case ExprNode::logicalAndEquals:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                if (lVal) 
                    lVal->emitReadForWriteBackBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchFalse, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->setLabel(skipOverSecondHalf);
                lVal->emitWriteBackBytecode(bCon, p->pos);
            }
            break;

        case ExprNode::logicalOrEquals:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                if (lVal) 
                    lVal->emitReadForWriteBackBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchTrue, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->setLabel(skipOverSecondHalf);
                lVal->emitWriteBackBytecode(bCon, p->pos);
            }
            break;

        case ExprNode::logicalAnd:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchFalse, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->setLabel(skipOverSecondHalf);
            }
            break;

        case ExprNode::logicalXor:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eLogicalXor, p->pos);
            }
            break;

        case ExprNode::logicalOr:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = SetupExprNode(env, phase, b->op1, exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchTrue, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->setLabel(skipOverSecondHalf);
            }
            break;

        case ExprNode::This:
            {
                bCon->emitOp(eThis, p->pos);
            }
            break;
        case ExprNode::superExpr:
            {
                SuperExprNode *s = checked_cast<SuperExprNode *>(p);
                if (s->op) {
                    Reference *lVal = SetupExprNode(env, phase, s->op, exprType);
                    if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                    bCon->emitOp(eSuperExpr, p->pos);
                }
                else
                    bCon->emitOp(eSuper, p->pos);
            }
            break;
        case ExprNode::Null:
            {
                bCon->emitOp(eNull, p->pos);
            }
            break;
        case ExprNode::numUnit:
            {
                NumUnitExprNode *n = checked_cast<NumUnitExprNode *>(p);
                if (n->str.compare(String(widenCString("UL"))) == 0)
                    bCon->addUInt64((uint64)(n->num), p->pos);
                else
                    if (n->str.compare(String(widenCString("L"))) == 0)
                        bCon->addInt64((uint64)(n->num), p->pos);
                    else
                        reportError(Exception::badValueError, "Unrecognized unit", p->pos);
            }
            break;
        case ExprNode::number:
            {
                int32 i;
                float64 x = checked_cast<NumberExprNode *>(p)->value;
                if (JSDOUBLE_IS_INT(x, i) && INT_FITS_IN_JS2VAL(i))
                    bCon->addInteger(i, p->pos);
                else
                    bCon->addFloat64(x, p->pos);
            }
            break;
        case ExprNode::regExp:
            {
                RegExpExprNode *v = checked_cast<RegExpExprNode *>(p);
                js2val args[2];
                const String *reStr = engine->allocStringPtr(&v->re);
                DEFINE_ROOTKEEPER(this, rk1, reStr);
                const String *flagStr = engine->allocStringPtr(&v->flags);
                DEFINE_ROOTKEEPER(this, rk2, flagStr);
                args[0] = STRING_TO_JS2VAL(reStr);
                args[1] = STRING_TO_JS2VAL(flagStr);
                // XXX error handling during this parse? The RegExp_Constructor is
                // going to call errorPos() on the current bCon.
                js2val reValue = RegExp_Constructor(this, JS2VAL_NULL, args, 2);
                RegExpInstance *reInst = checked_cast<RegExpInstance *>(JS2VAL_TO_OBJECT(reValue));
                bCon->addRegExp(reInst, p->pos);
            }
            break;
        case ExprNode::string:
            {  
                bCon->addString(checked_cast<StringExprNode *>(p)->str, p->pos);
            }
            break;
        case ExprNode::conditional:
            {
                BytecodeContainer::LabelID falseConditionExpression = bCon->getLabel();
                BytecodeContainer::LabelID labelAtBottom = bCon->getLabel();

                TernaryExprNode *c = checked_cast<TernaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, c->op1, exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchFalse, falseConditionExpression, p->pos);

                lVal = SetupExprNode(env, phase, c->op2, exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranch, labelAtBottom, p->pos);

                bCon->setLabel(falseConditionExpression);
                //adjustStack(-1);        // the true case will leave a stack entry pending
                                          // but we can discard it since only one path will be taken.
                lVal = SetupExprNode(env, phase, c->op3, exprType);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);

                bCon->setLabel(labelAtBottom);
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
                
                returnRef = new (*referenceArena) LexicalReference(new (this) Multiname(name, ns), cxt.strict, bCon);
                referenceArena->registerDestructor(returnRef);
            }
            break;
        case ExprNode::identifier:
            {
                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
                if ((i->name == widenCString("eval")) || (i->name == widenCString("arguments"))) {
                    // find the parameterFrame for this function and make sure
                    // that the arguments property will get built
                    FrameListIterator fi = env->getBegin(), end = env->getEnd();
                    while (fi != end) {
                        Frame *fr = *fi;
                        if ((fr->kind != WithFrameKind) && (fr->kind != BlockFrameKind)) {
                            NonWithFrame *nwf = checked_cast<NonWithFrame *>(fr);
                            if (nwf->kind == ParameterFrameKind) {
                                ParameterFrame *pf = checked_cast<ParameterFrame *>(nwf);
                                pf->buildArguments = true;
                                break;
                            }
                            else    // ran into a class or package, we're not in a function
                                break;
                        }
                        fi++;
                    }
                }
                returnRef = new (*referenceArena) LexicalReference(new (this) Multiname(i->name), cxt.strict, bCon);
                referenceArena->registerDestructor(returnRef);
                ((LexicalReference *)returnRef)->variableMultiname->addNamespace(cxt);
                // Try to find this identifier at compile time, we have to stop if we reach
                // a frame that supports dynamic properties - the identifier could be
                // created at runtime without us finding it here.
                // We're looking to find both the type of the reference (to store into exprType)
                // and to see if we can change the reference to a FrameSlot or Slot (for member
                // functions)
                Multiname *multiname = ((LexicalReference *)returnRef)->variableMultiname;
                FrameListIterator fi = env->getBegin(), end = env->getEnd();
                bool keepLooking = true;
                while (fi != end && keepLooking) {
                    Frame *fr = *fi;
                    if (fr->kind == WithFrameKind)
                        // XXX unless it's provably not a dynamic object that been with'd??
                        break;
                    NonWithFrame *pf = checked_cast<NonWithFrame *>(fr);
                    switch (pf->kind) {
                    default:
                        keepLooking = false;
                        break;
                    case ParameterFrameKind:
                        {
							bool isEnumerable;
                            LocalMember *m = findLocalMember(pf, multiname, ReadAccess, isEnumerable);
                            if (m) {
                                switch (checked_cast<LocalMember *>(m)->memberKind) {
                                case LocalMember::VariableMember:
                                    *exprType = checked_cast<Variable *>(m)->type;
                                    break;
                                case LocalMember::FrameVariableMember:
                                    ASSERT(checked_cast<FrameVariable *>(m)->kind == FrameVariable::Parameter);
                                    returnRef = new (*referenceArena) ParameterSlotReference(checked_cast<FrameVariable *>(m)->frameSlot);
                                    break;
                                }                                
                            }
                            keepLooking = false;    // don't look beneath the current function, as the slot base pointers aren't relevant
                        }
                        break;
                    case BlockFrameKind:
                        {
							bool isEnumerable;
                            LocalMember *m = findLocalMember(pf, multiname, ReadAccess, isEnumerable);
                            if (m) {
                                switch (checked_cast<LocalMember *>(m)->memberKind) {
                                case LocalMember::VariableMember:
                                    *exprType = checked_cast<Variable *>(m)->type;
                                    break;
                                case LocalMember::FrameVariableMember:
                                    ASSERT(checked_cast<FrameVariable *>(m)->kind == FrameVariable::Local);
                                    returnRef = new (*referenceArena) FrameSlotReference(checked_cast<FrameVariable *>(m)->frameSlot);
                                    break;
                                }                                        
                                keepLooking = false;
                            }
                        }
                        break;
                    case ClassKind: 
                        {
                            // look for this identifier in the static members
                            // (do we have a this?)
                            // If the class allows dynamic members, have to stop the search here
                            keepLooking = false;
                        }
                        break;
                    case PackageKind:
                        {
                            JS2Class *limit = objectType(pf);
                            InstanceMember *mBase = findBaseInstanceMember(limit, multiname, ReadAccess);
                            if (mBase) {
                                InstanceMember *m = getDerivedInstanceMember(*exprType, mBase, ReadAccess);
                                switch (m->memberKind) {
                                case Member::InstanceVariableMember:
                                    {
                                        InstanceVariable *mv = checked_cast<InstanceVariable *>(m);
                                        *exprType = mv->type;
                                    }
                                    break;
                                }
                                keepLooking = false;
                            }
                            else {
                                js2val base = OBJECT_TO_JS2VAL(pf);
                                Member *m = findCommonMember(&base, multiname, ReadAccess, false);
                                if (m) {
                                    switch (m->memberKind) {
                                    case Member::ForbiddenMember:
                                    case Member::DynamicVariableMember:
                                    case Member::FrameVariableMember:
                                    case Member::VariableMember:
                                    case Member::ConstructorMethodMember:
                                    case Member::SetterMember:
                                    case Member::GetterMember:
                                        switch (checked_cast<LocalMember *>(m)->memberKind) {
                                        case LocalMember::VariableMember:
                                            *exprType = checked_cast<Variable *>(m)->type;
                                            break;
                                        case LocalMember::FrameVariableMember:
                                            ASSERT(checked_cast<FrameVariable *>(m)->kind == FrameVariable::Package);
                                            returnRef = new (*referenceArena) PackageSlotReference(checked_cast<FrameVariable *>(m)->frameSlot);
                                            break;
                                        }                                        
                                        break;
                                    case Member::InstanceVariableMember:
                                    case Member::InstanceMethodMember:
                                    case Member::InstanceGetterMember:
                                    case Member::InstanceSetterMember:
                                        // XXX checked_cast<InstanceMember *>(m)
                                        break;
                                    }
                                    keepLooking = false;
                                }
                            }
                            // XXX if package allows dynamic members, stop looking
                            keepLooking = false;
                        }
                        break;
                    }
                    fi++;
                }
            }
            break;
        case ExprNode::Delete:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, u->op, exprType);
                if (lVal)
                    lVal->emitDeleteBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "Delete needs an lValue", p->pos);
            }
            break;
        case ExprNode::postIncrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, u->op, exprType);
                if (lVal)
                    lVal->emitPostIncBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PostIncrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::postDecrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, u->op, exprType);
                if (lVal)
                    lVal->emitPostDecBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PostDecrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::preIncrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, u->op, exprType);
                if (lVal)
                    lVal->emitPreIncBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PreIncrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::preDecrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = SetupExprNode(env, phase, u->op, exprType);
                if (lVal)
                    lVal->emitPreDecBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PreDecrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::index:
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *baseVal = SetupExprNode(env, phase, i->op, exprType);
                if (baseVal) baseVal->emitReadBytecode(bCon, p->pos);
                ExprPairList *ep = i->pairs;
                while (ep) {    // Validate has made sure there is only one, unnamed argument
                    Reference *argVal = SetupExprNode(env, phase, ep->value, exprType);
                    if (argVal) argVal->emitReadBytecode(bCon, p->pos);
                    ep = ep->next;
                }
                returnRef = new (*referenceArena) BracketReference();
                referenceArena->registerDestructor(returnRef);
            }
            break;
        case ExprNode::dot:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *baseVal = SetupExprNode(env, phase, b->op1, exprType);
                if (baseVal) baseVal->emitReadBytecode(bCon, p->pos);

                if (b->op2->getKind() == ExprNode::identifier) {
                    IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(b->op2);

                    if (*exprType) {
                        Multiname multiname(i->name);
                        InstanceMember *mBase = findBaseInstanceMember(*exprType, &multiname, ReadAccess);
                        if (mBase) {
                            InstanceMember *m = getDerivedInstanceMember(*exprType, mBase, ReadAccess);
                            if (m->memberKind == Member::InstanceVariableMember) {
                                InstanceVariable *mv = checked_cast<InstanceVariable *>(m);
                                *exprType = mv->type;
                                returnRef = new (*referenceArena) SlotReference(mv->slotIndex);
                            }
                        }
                    }

                    if (returnRef == NULL) {
                        returnRef = new (*referenceArena) DotReference(new (this) Multiname(i->name), bCon);
                        referenceArena->registerDestructor(returnRef);
                        checked_cast<DotReference *>(returnRef)->propertyMultiname->addNamespace(cxt);
                    }
                } 
                else {
                    if (b->op2->getKind() == ExprNode::qualify) {
                        Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                        ASSERT(rVal && checked_cast<LexicalReference *>(rVal));
                        returnRef = new (*referenceArena) DotReference(((LexicalReference *)rVal)->variableMultiname, bCon);
                        referenceArena->registerDestructor(returnRef);
                        checked_cast<DotReference *>(returnRef)->propertyMultiname->addNamespace(cxt);
                    }
                    // XXX else bracketRef...
                    else
                        NOT_REACHED("do we support these, or not?");
                }
            }
            break;
        case ExprNode::boolean:
            if (checked_cast<BooleanExprNode *>(p)->value) 
                bCon->emitOp(eTrue, p->pos);
            else 
                bCon->emitOp(eFalse, p->pos);
            break;
        case ExprNode::arrayLiteral:
            {
                int32 argCount = 0;
                PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
                ExprPairList *e = plen->pairs;
                while (e) {
                    if (e->value) {
                        Reference *rVal = SetupExprNode(env, phase, e->value, exprType);
                        if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                        argCount++;
                    }
                    e = e->next;
                }
                bCon->emitOp(eNewArray, p->pos, -argCount + 1);    // pop argCount args and push a new array
                bCon->addShort((uint16)argCount);
            }
            break;
        case ExprNode::objectLiteral:
            {
                int32 argCount = 0;
                PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
                ExprPairList *e = plen->pairs;
                while (e) {
                    ASSERT(e->field && e->value);
                    Reference *rVal = SetupExprNode(env, phase, e->value, exprType);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    switch (e->field->getKind()) {
                    case ExprNode::identifier:
                        bCon->addString(&checked_cast<IdentifierExprNode *>(e->field)->name, p->pos);
                        break;
                    case ExprNode::string:
                        bCon->addString(checked_cast<StringExprNode *>(e->field)->str, p->pos);
                        break;
                    case ExprNode::number:
                        bCon->addString(engine->numberToString(&(checked_cast<NumberExprNode *>(e->field))->value), p->pos);
                        break;
                    default:
                        NOT_REACHED("bad field name");
                    }
                    argCount++;
                    e = e->next;
                }
                bCon->emitOp(eNewObject, p->pos, -argCount + 1);    // pop argCount args and push a new object
                bCon->addShort((uint16)argCount);
            }
            break;
        case ExprNode::Typeof:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *rVal = SetupExprNode(env, phase, u->op, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eTypeof, p->pos);
            }
            break;
        case ExprNode::call:
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *rVal = SetupExprNode(env, phase, i->op, exprType);
                if (rVal) 
                    rVal->emitReadForInvokeBytecode(bCon, p->pos);
                else /* a call doesn't have to have an lValue to execute on, 
                      * but we use the value as it's own 'this' in that case. 
                      */
                    bCon->emitOp(eDup, p->pos);
                ExprPairList *args = i->pairs;
                uint16 argCount = 0;
                while (args) {
                    Reference *r = SetupExprNode(env, phase, args->value, exprType);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    argCount++;
                    args = args->next;
                }
                bCon->emitOp(eCall, p->pos, -(argCount + 2) + 1);    // pop argCount args, the base & function, and push a result
                bCon->addShort(argCount);
            }
            break;
        case ExprNode::New: 
            {
                // XXX why not?--> if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *rVal = SetupExprNode(env, phase, i->op, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                ExprPairList *args = i->pairs;
                uint16 argCount = 0;
                while (args) {
                    Reference *r = SetupExprNode(env, phase, args->value, exprType);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    argCount++;
                    args = args->next;
                }
                bCon->emitOp(eNew, p->pos, -(argCount + 1) + 1);    // pop argCount args, the type or function, and push a result
                bCon->addShort(argCount);
            }
            break;
        case ExprNode::comma:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *r = SetupExprNode(env, phase, b->op1, exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(ePopv, p->pos);
                returnRef = SetupExprNode(env, phase, b->op2, exprType);
            }
            break;
        case ExprNode::Instanceof:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *rVal = SetupExprNode(env, phase, b->op1, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eInstanceof, p->pos);
            }
            break;
        case ExprNode::In:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *rVal = SetupExprNode(env, phase, b->op1, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                rVal = SetupExprNode(env, phase, b->op2, exprType);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eIn, p->pos);
            }
            break;
        case ExprNode::functionLiteral:
            {
                FunctionExprNode *f = checked_cast<FunctionExprNode *>(p);
                FunctionInstance *fnInst = f->function.fn;
                CompilationData *oldData = startCompilationUnit(fnInst->fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
                env->addFrame(fnInst->fWrap->compileFrame);
                SetupStmt(env, phase, f->function.body);
                // XXX need to make sure that all paths lead to an exit of some kind
                bCon->emitOp(eReturnVoid, p->pos);
                env->removeTopFrame();
                restoreCompilationUnit(oldData);
                bCon->emitOp(eFunction, p->pos);
                bCon->addObject(fnInst);
            }
            break;
        case ExprNode::superStmt:
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                ExprPairList *args = i->pairs;
                uint16 argCount = 0;
                while (args) {
                    Reference *r = SetupExprNode(env, phase, args->value, exprType);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    argCount++;
                    args = args->next;
                }
                bCon->emitOp(eSuperCall, p->pos, -argCount);    // pop argCount args, no result
                bCon->addShort(argCount);
            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        }
        return returnRef;
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
        FrameListIterator fi = getBegin(), end = getEnd();
        while (fi != end) {
            if ((*fi)->kind == ClassKind)
                return checked_cast<JS2Class *>(*fi);
            fi++;
        }
        return NULL;
    }

    // If env is from with a function's body, return the innermost ParameterFrame for
    // the innermost such function, otherwise return NULL
    ParameterFrame *Environment::getEnclosingParameterFrame(js2val *thisP)
    {
        FrameListIterator fi = getBegin(), end = getEnd();
        while (fi != end) {
            switch ((*fi)->kind) {
            case ClassKind:
            case PackageKind:
            case SystemKind:
                return NULL;
            case ParameterFrameKind:
				{
					ParameterFrame *pFrame = checked_cast<ParameterFrame *>(*fi);
					if (thisP) *thisP = pFrame->thisObject;
					return pFrame;
				}
            case BlockFrameKind:
            case WithFrameKind:
                break;
            }
            fi++;
        }
        return NULL;
    }

    // getRegionalEnvironment(env) returns all frames in env up to and including the first 
    // regional frame. A regional frame is either any frame other than a with frame or local 
    // block frame, a local block frame directly enclosed in a class, or a local block frame 
    // directly enclosed in a with frame directly enclosed in a class.
    // In this implementation, the return value is the iterator at the end of the regional environment.
    FrameListIterator Environment::getRegionalEnvironment()
    {
        FrameListIterator start = getBegin();
        FrameListIterator  fi = start;
        while (((*fi)->kind == BlockFrameKind) || ((*fi)->kind == WithFrameKind)) {
            fi++;
            ASSERT(fi != getEnd());
        }
        if ((*fi)->kind == ClassKind) {
            while ((fi != start) && ((*fi)->kind != BlockFrameKind))
                fi--;
        }
        return fi;
   }


    // Returns the most specific regional frame.
    // Returns the iterator value for that frame so that the frames neighbors can be accessed
    FrameListIterator Environment::getRegionalFrame()
    {
        FrameListIterator fi = getRegionalEnvironment();
        return fi;
    }

    // Returns the penultimate frame, always a Package
    Package *Environment::getPackageFrame()
    {
        Frame *result = *(getEnd() - 2);
        ASSERT(result->kind == PackageKind);
        return checked_cast<Package *>(result);
    }

    js2val Environment::readImplicitThis(JS2Metadata *meta)
    {
		js2val thisVal;
        ParameterFrame *pFrame = getEnclosingParameterFrame(&thisVal);
        if (pFrame == NULL)
            meta->reportError(Exception::referenceError, "Can't access instance members outside an instance method without supplying an instance object", meta->engine->errorPos());
        if ((!JS2VAL_IS_OBJECT(thisVal) || JS2VAL_IS_NULL(thisVal)) || !pFrame->isInstance || !pFrame->isConstructor)
            meta->reportError(Exception::referenceError, "Can't access instance members inside a non-instance method without supplying an instance object", meta->engine->errorPos());
        if (!pFrame->superConstructorCalled)
            meta->reportError(Exception::uninitializedError, "Can't access instance members from within a constructor before the superconstructor has been called", meta->engine->errorPos());
        return thisVal;
    }


    // Read the value of a lexical reference - it's an error if that reference
    // doesn't have a binding somewhere.
    // Attempt the read in each frame in the current environment, stopping at the
    // first succesful effort. If the property can't be found in any frame, it's 
    // an error.
    void Environment::lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase, js2val *rval, js2val *base)
    {
        FrameListIterator fi = getBegin(), end = getEnd();
        bool result = false;
        while (fi != end) {
            Frame *f = (*fi);
            switch (f->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(f));
                    js2val frame = OBJECT_TO_JS2VAL(f);
                    result = limit->Read(meta, &frame, multiname, this, phase, rval);
                }
                break;
            case SystemKind:
            case ParameterFrameKind:
            case BlockFrameKind:
                {
					bool isEnumerable;
                    LocalMember *m = meta->findLocalMember(f, multiname, ReadAccess, isEnumerable);
                    if (m)
                        result = meta->readLocalMember(m, phase, rval, f);
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(f);
                    // XXX uninitialized 'with' object?
                    js2val withVal = OBJECT_TO_JS2VAL(wf->obj);
                    JS2Class *limit = meta->objectType(withVal);
                    result = limit->Read(meta, &withVal, multiname, this, phase, rval);
                    if (result && base)
                        *base = withVal;
                }
                break;
            }
            if (result)
                return;
            fi++;
        }
        meta->reportError(Exception::referenceError, "{0} is undefined", meta->engine->errorPos(), multiname->name);
    }

    // Attempt the write in the top frame in the current environment - if the property
    // exists, then fine. Otherwise create the property there.
    void Environment::lexicalWrite(JS2Metadata *meta, Multiname *multiname, js2val newValue, bool createIfMissing)
    {
        FrameListIterator fi = getBegin(), end = getEnd();
        bool result = false;
        while (fi != end) {
            Frame *f = (*fi);
            switch (f->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(f));
                    result = limit->Write(meta, OBJECT_TO_JS2VAL(f), multiname, this, false, newValue, false);
                }
                break;
            case SystemKind:
            case ParameterFrameKind:
            case BlockFrameKind:
                {
					bool isEnumerable;
                    LocalMember *m = meta->findLocalMember(f, multiname, WriteAccess, isEnumerable);
                    if (m) {
                        meta->writeLocalMember(m, newValue, false, f);
                        result = true;
                    }
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(f);
                    // XXX uninitialized 'with' object?
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(wf->obj));
                    result = limit->Write(meta, OBJECT_TO_JS2VAL(wf->obj), multiname, this, false, newValue, false);
                }
                break;
            }
            if (result)
                return;
            fi++;
        }
        if (createIfMissing) {
            Package *pkg = getPackageFrame();
            JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(pkg));
            result = limit->Write(meta, OBJECT_TO_JS2VAL(pkg), multiname, this, true, newValue, false);
            if (result)
                return;
        }
        if (!meta->cxt.E3compatibility)
            meta->reportError(Exception::referenceError, "{0} is undefined", meta->engine->errorPos(), multiname->name);
    }


    // Initialize a variable - it might not be in the immediate frame, because of hoisting
    // but it had darn well better be in the environment somewhere.
    void Environment::lexicalInit(JS2Metadata *meta, Multiname *multiname, js2val newValue)
    {
        FrameListIterator fi = getBegin(), end = getEnd();
        bool result = false;
        while (fi != end) {
            Frame *f = (*fi);
            switch (f->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(f));
                    result = limit->Write(meta, OBJECT_TO_JS2VAL(f), multiname, this, false, newValue, true);
                }
                break;
            case SystemKind:
            case ParameterFrameKind:
            case BlockFrameKind:
                {
					bool isEnumerable;
                    LocalMember *m = meta->findLocalMember(f, multiname, WriteAccess, isEnumerable);
                    if (m) {
                        meta->writeLocalMember(m, newValue, true, f);
                        result = true;
                    }
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(f);
                    // XXX uninitialized 'with' object?
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(wf->obj));
                    result = limit->Write(meta, OBJECT_TO_JS2VAL(wf->obj), multiname, this, false, newValue, true);
                }
                break;
            }
            if (result)
                return;
            fi++;
        }
        // XXX can reach here? Shouldn't it be defined in the frame/etc already???
        ASSERT(false);
        Package *pkg = getPackageFrame();
        JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(pkg));
        result = limit->Write(meta, OBJECT_TO_JS2VAL(pkg), multiname, this, true, newValue, true);
        if (result)
            return;
    }

    // Delete the named property in the current environment, return true if the property
    // can't be found, or the result of the deleteProperty call if it was found.
    bool Environment::lexicalDelete(JS2Metadata *meta, Multiname *multiname, Phase phase)
    {
        FrameListIterator fi = getBegin(), end = getEnd();
        bool result = false;
        while (fi != end) {
            Frame *f = (*fi);
            switch (f->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(f));
                    if (limit->Delete(meta, OBJECT_TO_JS2VAL(f), multiname, this, &result))
                        return result;
                }
                break;
            case SystemKind:
            case ParameterFrameKind:
            case BlockFrameKind:
                {
					bool isEnumerable;
                    LocalMember *m = meta->findLocalMember(f, multiname, WriteAccess, isEnumerable);
                    if (m)
                        return false;
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(f);
                    // XXX uninitialized 'with' object?
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(wf->obj));
                    if (limit->Delete(meta, OBJECT_TO_JS2VAL(wf->obj), multiname, this, &result))
                        return result;
                }
                break;
            }
            fi++;
        }
        return true;
    }

    // Clone the pluralFrame bindings into the singularFrame, instantiating new members for each binding
    void Environment::instantiateFrame(NonWithFrame *pluralFrame, NonWithFrame *singularFrame, bool buildSlots)
    {
        singularFrame->localBindings.clear();

        for (LocalBindingIterator bi = pluralFrame->localBindings.begin(), bend = pluralFrame->localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            lbe->clear();
        }
        for (LocalBindingIterator bi2 = pluralFrame->localBindings.begin(), bend2 = pluralFrame->localBindings.end(); (bi2 != bend2); bi2++) {
            LocalBindingEntry *lbe = *bi2;
            singularFrame->localBindings.insert(lbe->name, lbe->clone());
        }
        if (buildSlots && pluralFrame->frameSlots) {
            size_t count = pluralFrame->frameSlots->size();
            singularFrame->frameSlots = new std::vector<js2val>(count);
            for (size_t i = 0; i < count; i++)
                (*singularFrame->frameSlots)[i] = (*pluralFrame->frameSlots)[i];
        }
    }

    // need to mark all the frames in the environment - otherwise a marked frame that
    // came initially from the bytecodeContainer may prevent the markChildren call
    // from finding frames further down the list.
    void Environment::markChildren()
    { 
        FrameListIterator fi = getBegin(), end = getEnd();
        while (fi != end) {
            GCMARKOBJECT(*fi);
            fi++;
        }
    }


/************************************************************************************
 *
 *  Context
 *
 ************************************************************************************/

    // clone a context
    Context::Context(Context *cxt) : strict(cxt->strict), E3compatibility(cxt->E3compatibility), openNamespaces(cxt->openNamespaces)
    {
        ASSERT(false);  // ?? used ??
    }


/************************************************************************************
 *
 *  Multiname
 *
 ************************************************************************************/

    // return true if the given namespace is on the namespace list
    bool Multiname::listContains(Namespace *nameSpace)
    { 
        if (nsList->empty())
            return true;
        for (NamespaceListIterator n = nsList->begin(), end = nsList->end(); (n != end); n++) {
            if (*n == nameSpace)
                return true;
        }
        return false;
    }

    // add all the open namespaces from the given context
    void Multiname::addNamespace(Context &cxt)    
    { 
        addNamespace(&cxt.openNamespaces); 
    }


    // add every namespace from the list to this Multiname
    void Multiname::addNamespace(NamespaceList &ns)
    {
        for (NamespaceListIterator nli = ns.begin(), end = ns.end();
                (nli != end); nli++)
            nsList->push_back(*nli);
    }

    QualifiedName *Multiname::selectPrimaryName(JS2Metadata *meta)
    {
        if (nsList->size() == 1)
            return new QualifiedName(nsList->back(), name);
        else {
            if (listContains(meta->publicNamespace))
                return new QualifiedName(meta->publicNamespace, name);
            else {
                meta->reportError(Exception::propertyAccessError, "No good primary name {0}", meta->engine->errorPos(), name);
            }
        }
		return NULL;
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void Multiname::markChildren()
    {
        for (NamespaceListIterator n = nsList->begin(), end = nsList->end(); (n != end); n++) {
            GCMARKOBJECT(*n)
        }
//        if (name) JS2Object::mark(name);
    }

    bool Multiname::subsetOf(Multiname &mn)
    {
        if (name != mn.name)
            return false;
        for (NamespaceListIterator n = nsList->begin(), end = nsList->end(); (n != end); n++) {
            if (!mn.listContains(*n))
                return false;
        }
        return true;
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
    Multiname *JS2Metadata::defineLocalMember(Environment *env, const StringAtom &id, NamespaceList *namespaces, 
                                                Attribute::OverrideModifier overrideMod, bool xplicit, Access access,
                                                LocalMember *m, size_t pos, bool enumerable)
    {
        NonWithFrame *innerFrame = checked_cast<NonWithFrame *>(*(env->getBegin()));
        if ((overrideMod != Attribute::NoOverride) || (xplicit && innerFrame->kind != PackageKind))
            reportError(Exception::definitionError, "Illegal definition", pos);
        
        Multiname *multiname = new (this) Multiname(world.identifiers[id]);
        if (!namespaces || namespaces->empty()) 
            multiname->addNamespace(publicNamespace);
        else
            multiname->addNamespace(namespaces);

        // Search the local frame for an overlapping definition
        LocalBindingEntry **lbeP = innerFrame->localBindings[id];
        if (lbeP) {
            for (LocalBindingEntry::NS_Iterator i = (*lbeP)->begin(), end = (*lbeP)->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding &ns = *i;
                if ((ns.second->accesses & access) && multiname->listContains(ns.first))
                    reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
            }
        }

        // Check all frames below the current - up to the RegionalFrame - for a non-forbidden definition
        FrameListIterator fi = env->getBegin();
        Frame *regionalFrame = *(env->getRegionalFrame());
        if (innerFrame != regionalFrame) {
            // The frame iterator is pointing at the top of the environment's
            // frame list, start at the one below that and continue to the frame
            // returned by 'getRegionalFrame()'.
            Frame *fr = *++fi;
            while (true) {
                if (fr->kind != WithFrameKind) {
                    NonWithFrame *nwfr = checked_cast<NonWithFrame *>(fr);
                    LocalBindingEntry **rbeP = nwfr->localBindings[id];
                    if (rbeP) {
                        for (LocalBindingEntry::NS_Iterator i = (*rbeP)->begin(), end = (*rbeP)->end(); (i != end); i++) {
                            LocalBindingEntry::NamespaceBinding &ns = *i;
                            if ((ns.second->accesses & access) 
                                    && (ns.second->content->memberKind != LocalMember::ForbiddenMember)
                                    && multiname->listContains(ns.first))
                                reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
                        }
                    }
                }
                if (fr == regionalFrame)
                    break;
                fr = *++fi;
                ASSERT(fr);
            }
        }

        // Now insert the id, via all it's namespaces into the local frame
        LocalBindingEntry *lbe;
        if (lbeP == NULL) {
            lbe = new LocalBindingEntry(id);
            innerFrame->localBindings.insert(id, lbe);
        }
        else
            lbe = *lbeP;
        for (NamespaceListIterator nli = multiname->nsList->begin(), nlend = multiname->nsList->end(); (nli != nlend); nli++) {
            LocalBinding *new_b = new LocalBinding(access, m, enumerable);
            lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(*nli, new_b));
        }
        // Mark the bindings of multiname as Forbidden in all non-innermost frames in the current
        // region if they haven't been marked as such already.
        if (innerFrame != regionalFrame) {
            fi = env->getBegin();
            Frame *fr = *++fi;
            while (true) {
                if (fr->kind != WithFrameKind) {
                    NonWithFrame *nwfr = checked_cast<NonWithFrame *>(fr);
                    for (NamespaceListIterator nli = multiname->nsList->begin(), nlend = multiname->nsList->end(); (nli != nlend); nli++) {
                        bool foundEntry = false;
                        LocalBindingEntry **rbeP = nwfr->localBindings[id];
                        if (rbeP) {
                            for (LocalBindingEntry::NS_Iterator i = (*rbeP)->begin(), end = (*rbeP)->end(); (i != end); i++) {
                                LocalBindingEntry::NamespaceBinding &ns = *i;
                                if ((ns.second->accesses & access) && (ns.first == *nli)) {
                                    ASSERT(ns.second->content->memberKind == LocalMember::ForbiddenMember);
                                    foundEntry = true;
                                    break;
                                }
                            }
                        }
                        if (!foundEntry) {
                            LocalBindingEntry *rbe = new LocalBindingEntry(id);
                            nwfr->localBindings.insert(id, rbe);
                            LocalBinding *new_b = new LocalBinding(access, forbiddenMember, false);
                            rbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(*nli, new_b));
                        }
                    }
                }
                if (fr == regionalFrame)
                    break;
                fr = *++fi;
            }
        }
        return multiname;
    }

    // Look through 'c' and all it's super classes for an identifier 
    // matching the qualified name and access.
    InstanceMember *JS2Metadata::findInstanceMember(JS2Class *c, QualifiedName *qname, Access access)
    {
        if (qname == NULL)
            return NULL;
        JS2Class *s = c;
        while (s) {
            InstanceBindingEntry **ibeP = c->instanceBindings[qname->name];
            if (ibeP) {
                for (InstanceBindingEntry::NS_Iterator i = (*ibeP)->begin(), end = (*ibeP)->end(); (i != end); i++) {
                    InstanceBindingEntry::NamespaceBinding &ns = *i;
                    if ((ns.second->accesses & access) && (ns.first == qname->nameSpace)) {
                        return ns.second->content;
                    }
                }
            }
            s = s->super;
        }
        return NULL;
    }

    InstanceMember *JS2Metadata::searchForOverrides(JS2Class *c, Multiname *multiname, Access access, size_t pos)
    {
        InstanceMember *mBase = NULL;
        JS2Class *s = c->super;
        if (s) {
            for (NamespaceListIterator nli = multiname->nsList->begin(), nlend = multiname->nsList->end(); (nli != nlend); nli++) {
                Multiname *mn = new (this) Multiname(multiname->name, *nli);
                DEFINE_ROOTKEEPER(this, rk, mn);
                InstanceMember *m = findBaseInstanceMember(s, mn, access);
                if (mBase == NULL)
                    mBase = m;
                else
                    if (m && (m != mBase))
                        reportError(Exception::definitionError, "Illegal override", pos);
            }            
        }
        return mBase;
    }

    InstanceMember *JS2Metadata::defineInstanceMember(JS2Class *c, Context *cxt, const StringAtom &id, NamespaceList &namespaces, 
                                                                    Attribute::OverrideModifier overrideMod, bool xplicit,
                                                                    InstanceMember *m, size_t pos)
    {
        if (xplicit)
            reportError(Exception::definitionError, "Illegal use of explicit", pos);
        Access access = m->instanceMemberAccess();
        Multiname requestedMultiname(id, namespaces);
        Multiname openMultiname(id, cxt);
        Multiname *definedMultiname = NULL;
        Multiname *searchedMultiname = NULL;
        if (requestedMultiname.nsList->empty()) {
            definedMultiname = new (this) Multiname(id, publicNamespace);
            searchedMultiname = &openMultiname;
        }
        else {
            definedMultiname = &requestedMultiname;
            searchedMultiname = &requestedMultiname;
        }
        InstanceMember *mBase = searchForOverrides(c, searchedMultiname, access, pos);
        InstanceMember *mOverridden = NULL;
        if (mBase) {
            mOverridden = getDerivedInstanceMember(c, mBase, access);
            definedMultiname = mOverridden->multiname;
            if (!requestedMultiname.subsetOf(*definedMultiname))
                reportError(Exception::definitionError, "Illegal definition", pos);
            bool goodKind;
            switch (m->memberKind) {
            case Member::InstanceVariableMember:
                goodKind = (mOverridden->memberKind == Member::InstanceVariableMember);
                break;
            case Member::InstanceGetterMember:
                goodKind = ((mOverridden->memberKind == Member::InstanceVariableMember)
                                || (mOverridden->memberKind == Member::InstanceGetterMember));
                break;
            case Member::InstanceSetterMember:
                goodKind = ((mOverridden->memberKind == Member::InstanceVariableMember)
                                || (mOverridden->memberKind == Member::InstanceSetterMember));
                break;
            case Member::InstanceMethodMember:
                goodKind = (mOverridden->memberKind == Member::InstanceMethodMember);
                break;
            }
            if (mOverridden->final || !goodKind)
                reportError(Exception::definitionError, "Illegal override", pos);
        }
        InstanceBindingEntry **ibeP = c->instanceBindings[id];
        if (ibeP) {
            for (InstanceBindingEntry::NS_Iterator i = (*ibeP)->begin(), end = (*ibeP)->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding &ns = *i;
                if ((access & ns.second->content->instanceMemberAccess()) && (definedMultiname->listContains(ns.first)))
                    reportError(Exception::definitionError, "Illegal override", pos);
            }
        }
        switch (overrideMod) {
        case Attribute::NoOverride:
            if (mBase || searchForOverrides(c, &openMultiname, access, pos))
                reportError(Exception::definitionError, "Illegal override", pos);
            break;
        case Attribute::DontOverride:
            if (mBase)
                reportError(Exception::definitionError, "Illegal override", pos);
            break;
        case Attribute::DoOverride: 
            if (!mBase)
                reportError(Exception::definitionError, "Illegal override", pos);
            break;
        }
        m->multiname = new (this) Multiname(definedMultiname);
        InstanceBindingEntry *ibe;
        if (ibeP == NULL) {
            ibe = new InstanceBindingEntry(id);
            c->instanceBindings.insert(id, ibe);
        }
        else
            ibe = *ibeP;
        for (NamespaceListIterator nli = definedMultiname->nsList->begin(), nlend = definedMultiname->nsList->end(); (nli != nlend); nli++) {
            // XXX here and in defineLocal... why a new binding for each namespace?
            // (other than it would mess up the destructor sequence :-)
            InstanceBinding *ib = new InstanceBinding(access, m);
            ibe->bindingList.push_back(InstanceBindingEntry::NamespaceBinding(*nli, ib));
        }
        return mOverridden;
    }

    FrameVariable *JS2Metadata::makeFrameVariable(NonWithFrame *regionalFrame)
    {
        FrameVariable *result = NULL;
        switch (regionalFrame->kind) {
        case PackageKind:
            result = new FrameVariable(regionalFrame->allocateSlot(), FrameVariable::Package);
            break;
        case ParameterFrameKind:
            result = new FrameVariable(regionalFrame->allocateSlot(), FrameVariable::Parameter);
            break;
        case BlockFrameKind:
            result = new FrameVariable(regionalFrame->allocateSlot(), FrameVariable::Local);
            break;
        default:
            NOT_REACHED("Bad frame kind");
        }
        return result;
    }

    // Define a hoisted var in the current frame (either Package or a Function)
    // defineHoistedVar(env, id, initialValue) defines a hoisted variable with the name id in the environment env. 
    // Hoisted variables are hoisted to the package or enclosing function scope. Multiple hoisted variables may be 
    // defined in the same scope, but they may not coexist with non-hoisted variables with the same name. A hoisted 
    // variable can be defined using either a var or a function statement. If it is defined using var, then initialValue
    // is always undefined (if the var statement has an initialiser, then the variable's value will be written later 
    // when the var statement is executed). If it is defined using function, then initialValue must be a function 
    // instance or open instance.  A var hoisted variable may be hoisted into the ParameterFrame if there is already 
    // a parameter with the same name; a function hoisted variable is never hoisted into the ParameterFrame and 
    // will shadow a parameter with the same name for compatibility with ECMAScript Edition 3. 
    // If there are multiple function definitions, the initial value is the last function definition. 
    
    LocalMember *JS2Metadata::defineHoistedVar(Environment *env, const StringAtom &id, js2val initVal, bool isVar, size_t pos)
    {
        LocalMember *result = NULL;
        FrameListIterator regionalFrameEnd = env->getRegionalEnvironment();
        NonWithFrame *regionalFrame = checked_cast<NonWithFrame *>(*regionalFrameEnd);
        ASSERT((regionalFrame->kind == PackageKind) || (regionalFrame->kind == ParameterFrameKind));          
rescan:
        // run through all the existing bindings, to see if this variable already exists.
        LocalBinding *bindingResult = NULL;
        bool foundMultiple = false;
        LocalBindingEntry **lbeP = regionalFrame->localBindings[id];
        if (lbeP) {
            for (LocalBindingEntry::NS_Iterator i = (*lbeP)->begin(), end = (*lbeP)->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding &ns = *i;
                if (ns.first == publicNamespace) {
                    if (bindingResult) {
                        foundMultiple = true;
                        break;  // it's not important to find more than one duplicate
                    }
                    else
                        bindingResult = ns.second;
                }
            }
        }
        if (((bindingResult == NULL) || !isVar) 
                && (regionalFrame->kind == ParameterFrameKind)
                && (regionalFrameEnd != env->getBegin())) {
            // re-scan in the frame above the parameter frame
            regionalFrame = checked_cast<NonWithFrame *>(*(regionalFrameEnd - 1));
            goto rescan;
        }
        
        if (bindingResult == NULL) {
            LocalBindingEntry *lbe;
            if (lbeP == NULL) {
                lbe = new LocalBindingEntry(id);
                regionalFrame->localBindings.insert(id, lbe);
            }
            else
                lbe = *lbeP;
            result = makeFrameVariable(regionalFrame);
            (*regionalFrame->frameSlots)[checked_cast<FrameVariable *>(result)->frameSlot] = initVal;
            LocalBinding *sb = new LocalBinding(ReadWriteAccess, result, true);
            lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(publicNamespace, sb));
        }
        else {
            if (foundMultiple)
                reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
            else {
                if ((bindingResult->accesses != ReadWriteAccess)
                        || ((bindingResult->content->memberKind != LocalMember::DynamicVariableMember)
                              && (bindingResult->content->memberKind != LocalMember::FrameVariableMember)))
                    reportError(Exception::definitionError, "Illegal redefinition of {0}", pos, id);
                else {
					// in the case of a duplicate parameter, we construct the additional slot 
					// so that there's a correspondence with the incoming argument array and
					// change the original parameter to the later slot.
					if (regionalFrame->kind == ParameterFrameKind) {
						result = makeFrameVariable(regionalFrame);
						bindingResult->content = result;
					}
					else {
						result = bindingResult->content;
						writeLocalMember(result, initVal, true, regionalFrame);
					}
                }
            }
            // At this point a hoisted binding of the same var already exists, so there is no need to create another one
        }
        return result;
    }

    static js2val GlobalObject_isNaN(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 /* argc */)
    {
        float64 d = meta->toFloat64(argv[0]);
        return BOOLEAN_TO_JS2VAL(JSDOUBLE_IS_NaN(d));
    }

    static js2val GlobalObject_isFinite(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 /* argc */)
    {
        float64 d = meta->toFloat64(argv[0]);
        return BOOLEAN_TO_JS2VAL(JSDOUBLE_IS_FINITE(d));
    }

    static js2val GlobalObject_toString(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 /* argc */)
    {
        return STRING_TO_JS2VAL(meta->engine->allocString("[object global]"));
    }

    static js2val GlobalObject_eval(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        if (argc) {
            if (!JS2VAL_IS_STRING(argv[0]))
                return argv[0];
            // need to reset the environment to the one in operation when eval was called so
            // that eval code can affect the apppropriate scopes.
            meta->engine->jsr(meta->engine->phase, NULL, meta->engine->sp - meta->engine->execStack, JS2VAL_VOID, meta->engine->activationStackTop[-1].env);
            js2val result = meta->readEvalString(*meta->toString(argv[0]), widenCString("Eval Source"));
            meta->engine->rts();
            return result;
        }
        else
            return JS2VAL_UNDEFINED;
    }

    /* See ECMA-262 15.1.2.5 */
    static js2val GlobalObject_unescape(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        const String *str = meta->toString(argv[0]);
        const char16 *chars = str->data();
        uint32 length = str->length();

        /* Don't bother allocating less space for the new string. */
        char16 *newchars = new char16[length + 1];
    
        uint32 ni = 0;
        uint32 i = 0;
        char16 ch;
        while (i < length) {
            ch = chars[i++];
            if (ch == '%') {
                if (i + 1 < length &&
                    JS7_ISHEX(chars[i]) && JS7_ISHEX(chars[i + 1]))
                {
                    ch = JS7_UNHEX(chars[i]) * 16 + JS7_UNHEX(chars[i + 1]);
                    i += 2;
                } else if (i + 4 < length && chars[i] == 'u' &&
                           JS7_ISHEX(chars[i + 1]) && JS7_ISHEX(chars[i + 2]) &&
                           JS7_ISHEX(chars[i + 3]) && JS7_ISHEX(chars[i + 4]))
                {
                    ch = (((((JS7_UNHEX(chars[i + 1]) << 4)
                            + JS7_UNHEX(chars[i + 2])) << 4)
                          + JS7_UNHEX(chars[i + 3])) << 4)
                        + JS7_UNHEX(chars[i + 4]);
                    i += 5;
                }
            }
            newchars[ni++] = ch;
        }
        newchars[ni] = 0;
        String s(newchars, ni);
        return meta->engine->allocString(s);
    }

    // Taken from jsstr.c...
/*
 * Stuff to emulate the old libmocha escape, which took a second argument
 * giving the type of escape to perform.  Retained for compatibility, and
 * copied here to avoid reliance on net.h, mkparse.c/NET_EscapeBytes.
 */

#define URL_XALPHAS     ((uint8) 1)
#define URL_XPALPHAS    ((uint8) 2)
#define URL_PATH        ((uint8) 4)

static const uint8 urlCharType[256] =
/*      Bit 0           xalpha          -- the alphas
 *      Bit 1           xpalpha         -- as xalpha but
 *                             converts spaces to plus and plus to %20
 *      Bit 2 ...       path            -- as xalphas but doesn't escape '/'
 */
    /*   0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 0x */
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* 1x */
         0,0,0,0,0,0,0,0,0,0,7,4,0,7,7,4,       /* 2x   !"#$%&'()*+,-./  */
         7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,       /* 3x  0123456789:;<=>?  */
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 4x  @ABCDEFGHIJKLMNO  */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,       /* 5X  PQRSTUVWXYZ[\]^_  */
         0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       /* 6x  `abcdefghijklmno  */
         7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,       /* 7X  pqrstuvwxyz{\}~  DEL */
         0, };

/* This matches the ECMA escape set when mask is 7 (default.) */

#define IS_OK(C, mask) (urlCharType[((uint8) (C))] & (mask))

    /* See ECMA-262 15.1.2.4. */
    static js2val GlobalObject_escape(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        uint32 newlength;
        char16 ch;
        const char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                               '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

        int32 mask = URL_XALPHAS | URL_XPALPHAS | URL_PATH;
        if (argc > 1) {
            float64 d = meta->toFloat64(argv[1]);
            if (!JSDOUBLE_IS_FINITE(d) ||
                (mask = (int32)d) != d ||
                mask & ~(URL_XALPHAS | URL_XPALPHAS | URL_PATH))
            {
                meta->reportError(Exception::badValueError,  "Need integral non-zero mask for escape", meta->engine->errorPos());
            }
        }

        const String *str = meta->toString(argv[0]);
        const char16 *chars = str->data();
        uint32 length = newlength = str->length();

        /* Take a first pass and see how big the result string will need to be. */
        uint32 i;
        for (i = 0; i < length; i++) {
            if ((ch = chars[i]) < 128 && IS_OK(ch, mask))
                continue;
            if (ch < 256) {
                if (mask == URL_XPALPHAS && ch == ' ')
                    continue;   /* The character will be encoded as '+' */
                newlength += 2; /* The character will be encoded as %XX */
            } else {
                newlength += 5; /* The character will be encoded as %uXXXX */
            }
        }

        char16 *newchars = new char16[newlength + 1];
        uint32 ni;
        for (i = 0, ni = 0; i < length; i++) {
            if ((ch = chars[i]) < 128 && IS_OK(ch, mask)) {
                newchars[ni++] = ch;
            } else if (ch < 256) {
                if (mask == URL_XPALPHAS && ch == ' ') {
                    newchars[ni++] = '+'; /* convert spaces to pluses */
                } else {
                    newchars[ni++] = '%';
                    newchars[ni++] = digits[ch >> 4];
                    newchars[ni++] = digits[ch & 0xF];
                }
            } else {
                newchars[ni++] = '%';
                newchars[ni++] = 'u';
                newchars[ni++] = digits[ch >> 12];
                newchars[ni++] = digits[(ch & 0xF00) >> 8];
                newchars[ni++] = digits[(ch & 0xF0) >> 4];
                newchars[ni++] = digits[ch & 0xF];
            }
        }
        ASSERT(ni == newlength);
        newchars[newlength] = 0;
        String s(newchars, newlength);
        return meta->engine->allocString(s);
    }

    static js2val GlobalObject_parseInt(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        if (argc > 0) {
            const String *str = meta->toString(argv[0]);
            const char16 *chars = str->data();
            uint32 length = str->length();
            const char16 *numEnd;
            uint base = 0;

            // we need to handle the prefix & radix ourselves as 'stringToInteger' does it differently
            const char16 *strEnd = chars + length;
            const char16 *strStart = skipWhiteSpace(chars, strEnd);
            if (strStart == strEnd)
                return meta->engine->nanValue;

            bool neg = false;
            if (*strStart == '-') {
                neg = true;
                strStart++;
            }
            else {
                if (*strStart == '+')
                    strStart++;
            }

            if (argc > 1) {
                float64 d = meta->toFloat64(argv[1]);
                base = JS2Engine::float64toInt32(d);
                if ((base != 0) && ((base < 2) || (base > 36)))
                    return meta->engine->nanValue;
            }
            if (*strStart == '0') {
                if ((strStart[1] & ~0x20) == 'X') {
                    base = 16;
                    strStart += 2;
                }
                else {
                    // backwards octal support
                    if ((argc == 1) && ((strStart + 1) != strEnd) && meta->cxt.E3compatibility) {
                        base = 8;
                        strStart++;
                    }
                }
            }
            if (strStart == strEnd)
                return meta->engine->nanValue;
                
            float64 d = stringToInteger(strStart, strEnd, numEnd, base);

            return meta->engine->allocNumber((neg) ? -d : d);
        }
		else
			return meta->engine->nanValue;
        
    }

    static js2val GlobalObject_parseFloat(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        if (argc > 0) {
            const String *str = meta->toString(argv[0]);
            const char16 *chars = str->data();
            uint32 length = str->length();
            const char16 *numEnd;
            const char16 *strEnd = chars + length;
            const char16 *strStart = skipWhiteSpace(chars, strEnd);
            if (strStart == strEnd)
                return meta->engine->nanValue;

            float64 d = stringToDouble(strStart, strEnd, numEnd);
            if (numEnd == strStart)
                return meta->engine->nanValue;
            return meta->engine->allocNumber(d);
        }
        else
			return meta->engine->nanValue;
    }


    static js2val GlobalObject_version(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
		if (argc > 0 && JS2VAL_IS_INT(argv[0])) {
			int32 v = JS2VAL_TO_INT(argv[0]);
			int32 oldV = meta->version;
			meta->version = v;
			return INT_TO_JS2VAL(oldV);
		}
		else
			return INT_TO_JS2VAL(meta->version);
    }

    void JS2Metadata::addGlobalObjectFunction(char *name, NativeCode *code, uint32 length)
    {
        FunctionInstance *fInst = createFunctionInstance(env, true, true, code, length, NULL);
        DEFINE_ROOTKEEPER(this, rk1, fInst);
        createDynamicProperty(glob, world.identifiers[name], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, true);
    }

    static js2val Object_Constructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {
        if ((argc == 0) || JS2VAL_IS_NULL(argv[0]) || JS2VAL_IS_UNDEFINED(argv[0]))
            return OBJECT_TO_JS2VAL(new (meta) SimpleInstance(meta, meta->objectClass->prototype, meta->objectClass));
        else {
            if (JS2VAL_IS_OBJECT(argv[0]))
                return argv[0];     // XXX special handling for host objects?
            else
                return meta->toObject(argv[0]);
        }
    }

    static js2val Object_toString(JS2Metadata *meta, const js2val thisValue, js2val /* argv */ [], uint32 /* argc */)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
        if ((obj->kind == PackageKind) && meta->cxt.E3compatibility) {
            // special case this for now, ECMA3 test sanity...
            return GlobalObject_toString(meta, thisValue, NULL, 0);
        }
        else {
			if (obj->kind == ClassKind && meta->cxt.E3compatibility) {
				String s = widenCString("[object Function]");
				return STRING_TO_JS2VAL(meta->engine->allocString(s));
			}
			else {
				JS2Class *type = (checked_cast<SimpleInstance *>(obj))->type;
				String s = "[object " + type->name + "]";
				return STRING_TO_JS2VAL(meta->engine->allocString(s));
			}
        }
    }
    
    static js2val Object_underbarProtoGet(JS2Metadata *meta, const js2val thisValue, js2val /* argv */ [], uint32 /* argc */)
    {
        JS2Object *obj;
        if (!JS2VAL_IS_OBJECT(thisValue))
            obj = JS2VAL_TO_OBJECT(meta->toObject(thisValue));
        else 
            obj = JS2VAL_TO_OBJECT(thisValue);
        if (obj->kind == SimpleInstanceKind)
            return (checked_cast<SimpleInstance *>(obj))->super;
        else
            return JS2VAL_UNDEFINED;
    }

    static js2val Object_underbarProtoSet(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        JS2Object *obj;
        if (!JS2VAL_IS_OBJECT(thisValue))
            obj = JS2VAL_TO_OBJECT(meta->toObject(thisValue));
        else 
            obj = JS2VAL_TO_OBJECT(thisValue);
        if ((argc > 0) && (obj->kind == SimpleInstanceKind)) {
            (checked_cast<SimpleInstance *>(obj))->super = argv[0];
            return argv[0];
        }
        return JS2VAL_UNDEFINED;
    }

    static js2val Object_hasOwnProperty(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
	{
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
		Multiname mn(meta->world.identifiers[*meta->toString(argv[0])]);
		bool isEnumerable;
		if (!meta->findLocalMember(obj, &mn, ReadAccess, isEnumerable))
			return JS2VAL_FALSE;
		return JS2VAL_TRUE;
	}

    static js2val Object_isPropertyOf(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
	{
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
		js2val v = argv[0];
		while (JS2VAL_IS_OBJECT(v)) {
			JS2Object *v_obj = JS2VAL_TO_OBJECT(v);
			if (v_obj->kind != SimpleInstanceKind)
				break;
			v = checked_cast<SimpleInstance *>(v_obj)->super;
			if (JS2VAL_IS_NULL(v))
				break;
			if (v == thisValue)
				return JS2VAL_TRUE;
		}
		return JS2VAL_FALSE;
	}

    static js2val Object_propertyIsEnumerble(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
	{
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
		Multiname mn(meta->world.identifiers[*meta->toString(argv[0])]);
		bool isEnumerable;
		LocalMember *m = meta->findLocalMember(obj, &mn, ReadAccess, isEnumerable);
		if ((m == NULL) || !isEnumerable)
			return JS2VAL_FALSE;
		return JS2VAL_TRUE;
	}


    static js2val class_underbarProtoGet(JS2Metadata *meta, const js2val thisValue, js2val /* argv */ [], uint32 /* argc */)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
        ASSERT(obj->kind == ClassKind);
        JS2Class *c = checked_cast<JS2Class *>(obj);
        return OBJECT_TO_JS2VAL(meta->functionClass->prototype);
    }
 
    js2val Array_lengthGet(JS2Metadata *meta, const js2val thisValue, js2val /* argv */ [], uint32 /* argc */)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
        ArrayInstance *arrInst = checked_cast<ArrayInstance *>(obj);
        
        return meta->engine->allocNumber(arrInst->length);
    }

    js2val Array_lengthSet(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisValue));
        JS2Object *obj = JS2VAL_TO_OBJECT(thisValue);
        ArrayInstance *arrInst = checked_cast<ArrayInstance *>(obj);

        uint32 newLength = meta->valToUInt32(argv[0]);
        if (newLength < arrInst->length) {
            // need to delete all the elements above the new length
            bool deleteResult;
			for (uint32 i = newLength; i < arrInst->length; i++) {
				const String *str = meta->engine->numberToString(i);
				if (meta->world.identifiers.hasEntry(*str)) {
					Multiname mn(meta->world.identifiers[*str], meta->publicNamespace);
					meta->arrayClass->Delete(meta, thisValue, &mn, NULL, &deleteResult);
				}
            }
        }
        arrInst->length = newLength;
        return JS2VAL_UNDEFINED;
    }

    static js2val Object_valueOf(JS2Metadata *meta, const js2val thisValue, js2val /* argv */ [], uint32 /* argc */)
    {
        return thisValue;
    }

    
#define MAKEBUILTINCLASS(c, super, dynamic, final, name, defaultVal) c = new (this) JS2Class(super, NULL, new (this) Namespace(engine->private_StringAtom), dynamic, final, name); c->complete = true; c->defaultValue = defaultVal;

    JS2Metadata::JS2Metadata(World &world) :
        world(world),
//        engine(new JS2Engine(world)),
//        publicNamespace(new Namespace(engine->public_StringAtom)),
        bCon(new BytecodeContainer()),
//        glob(new Package(widenCString("global"), new Namespace(&world.identifiers["internal"]))),
//        env(new Environment(new MetaData::SystemFrame(), glob)),
        flags(JS1),
		version(JS2VERSION_DEFAULT),
        showTrees(false),
        referenceArena(NULL),
        pond(POND_SIZE, NULL)
    {
        engine = new JS2Engine(this, world);
        publicNamespace = new (this) Namespace(engine->public_StringAtom);
        glob = new (this) Package(widenCString("global"), new (this) Namespace(world.identifiers["internal"]));
        env = new (this) Environment(new (this) SystemFrame(), glob);


        rngInitialized = false;
//        engine->meta = this;

        cxt.openNamespaces.clear();
        cxt.openNamespaces.push_back(publicNamespace);

        MAKEBUILTINCLASS(objectClass, NULL, false, false, engine->Object_StringAtom, JS2VAL_VOID);
        MAKEBUILTINCLASS(undefinedClass, objectClass, false, true, engine->undefined_StringAtom, JS2VAL_VOID);
        nullClass = new (this) JS2NullClass(objectClass, NULL, new (this) Namespace(engine->private_StringAtom), false, true, engine->null_StringAtom); nullClass->complete = true; nullClass->defaultValue = JS2VAL_NULL;
        MAKEBUILTINCLASS(booleanClass, objectClass, false, true, world.identifiers["Boolean"], JS2VAL_FALSE);
        MAKEBUILTINCLASS(generalNumberClass, objectClass, false, false, world.identifiers["general number"], engine->nanValue);
        MAKEBUILTINCLASS(numberClass, generalNumberClass, false, true, world.identifiers["Number"], engine->nanValue);
        integerClass = new (this) JS2IntegerClass(numberClass, NULL, new (this) Namespace(engine->private_StringAtom), false, true, world.identifiers["Integer"]); integerClass->complete = true; integerClass->defaultValue = JS2VAL_ZERO;
        MAKEBUILTINCLASS(characterClass, objectClass, false, true, world.identifiers["Character"], JS2VAL_ZERO);
        stringClass = new (this) JS2StringClass(objectClass, NULL, new (this) Namespace(engine->private_StringAtom), false, true, world.identifiers["String"]); stringClass->complete = true; stringClass->defaultValue = JS2VAL_NULL;        
        MAKEBUILTINCLASS(namespaceClass, objectClass, false, true, world.identifiers["namespace"], JS2VAL_NULL);
        MAKEBUILTINCLASS(attributeClass, objectClass, false, true, world.identifiers["attribute"], JS2VAL_NULL);
        MAKEBUILTINCLASS(classClass, objectClass, false, true, world.identifiers["Class"], JS2VAL_NULL);                
        MAKEBUILTINCLASS(functionClass, objectClass, true, true, engine->Function_StringAtom, JS2VAL_NULL);
        MAKEBUILTINCLASS(packageClass, objectClass, true, true, world.identifiers["Package"], JS2VAL_NULL);
        argumentsClass = new (this) JS2ArgumentsClass(objectClass, NULL, new (this) Namespace(engine->private_StringAtom), false, true, world.identifiers["Object"]); argumentsClass->complete = true; argumentsClass->defaultValue = JS2VAL_NULL;

        // A 'forbidden' member, used to mark hidden bindings
        forbiddenMember = new LocalMember(Member::ForbiddenMember, true);
		forbiddenMember->acquire();
    
        FunctionInstance *fInst = NULL;
        DEFINE_ROOTKEEPER(this, rk1, fInst);
        Variable *v;
        
// XXX Built-in Attributes... XXX 
/*
XXX see EvalAttributeExpression, where identifiers are being handled for now...

        CompoundAttribute *attr = new CompoundAttribute();
        attr->dynamic = true;
        v = new Variable(attributeClass, OBJECT_TO_JS2VAL(attr), true);
        defineLocalMember(env, &world.identifiers["dynamic"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
*/

/*** ECMA 3  Global Object ***/
        // Non-function properties of the global object : 'undefined', 'NaN' and 'Infinity'
        createDynamicProperty(glob, engine->undefined_StringAtom, JS2VAL_UNDEFINED, ReadAccess, true, false);
        createDynamicProperty(glob, world.identifiers["NaN"], engine->nanValue, ReadAccess, true, false);
        createDynamicProperty(glob, world.identifiers["Infinity"], engine->posInfValue, ReadAccess, true, false);


/*** ECMA 3  Object Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(objectClass), true);
        defineLocalMember(env, world.identifiers["Object"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        // Function properties of the Object prototype object
        objectClass->prototype = OBJECT_TO_JS2VAL(new (this) SimpleInstance(this, NULL, objectClass));
        objectClass->construct = Object_Constructor;
        objectClass->call = Object_Constructor;
        // Adding "prototype" as a static member of the class - not a dynamic property
        env->addFrame(objectClass);
            v = new Variable(objectClass, OBJECT_TO_JS2VAL(objectClass->prototype), true);
            defineLocalMember(env, engine->prototype_StringAtom, NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
            v = new Variable(objectClass, INT_TO_JS2VAL(1), true);
            defineLocalMember(env, engine->length_StringAtom, NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
        env->removeTopFrame();

		glob->super = objectClass->prototype;

/*** ECMA 3  Function Class ***/
// Need this initialized early, as subsequent FunctionInstances need the Function.prototype value
        v = new Variable(classClass, OBJECT_TO_JS2VAL(functionClass), true);
        defineLocalMember(env, world.identifiers["Function"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initFunctionObject(this);

        // Function properties of the global object 
        addGlobalObjectFunction("isNaN", GlobalObject_isNaN, 1);
		addGlobalObjectFunction("isFinite", GlobalObject_isFinite, 1);
        addGlobalObjectFunction("eval", GlobalObject_eval, 1);
        addGlobalObjectFunction("toString", GlobalObject_toString, 1);
        addGlobalObjectFunction("valueOf", GlobalObject_toString, 1);
        addGlobalObjectFunction("unescape", GlobalObject_unescape, 1);
        addGlobalObjectFunction("escape", GlobalObject_escape, 1);
        addGlobalObjectFunction("parseInt", GlobalObject_parseInt, 2);
        addGlobalObjectFunction("parseFloat", GlobalObject_parseFloat, 1);
        addGlobalObjectFunction("version", GlobalObject_version, 1);


// Add __proto__ as a getter/setter instance member to Object & class
        Multiname proto_mn(world.identifiers["__proto__"], publicNamespace);
        fInst = createFunctionInstance(env, true, true, class_underbarProtoGet, 0, NULL);
        InstanceGetter *g = new InstanceGetter(&proto_mn, fInst, objectClass, true, true);
        defineInstanceMember(classClass, &cxt, proto_mn.name, *proto_mn.nsList, Attribute::NoOverride, false, g, 0);
        fInst = createFunctionInstance(env, true, true, class_underbarProtoGet, 0, NULL);
        InstanceSetter *s = new InstanceSetter(&proto_mn, fInst, objectClass, true, true);
        defineInstanceMember(classClass, &cxt, proto_mn.name, *proto_mn.nsList, Attribute::NoOverride, false, s, 0);

        fInst = createFunctionInstance(env, true, true, Object_underbarProtoGet, 0, NULL);
        g = new InstanceGetter(&proto_mn, fInst, objectClass, true, true);
        defineInstanceMember(objectClass, &cxt, proto_mn.name, *proto_mn.nsList, Attribute::NoOverride, false, g, 0);
        fInst = createFunctionInstance(env, true, true, Object_underbarProtoSet, 0, NULL);
        s = new InstanceSetter(&proto_mn, fInst, objectClass, true, true);
        defineInstanceMember(objectClass, &cxt, proto_mn.name, *proto_mn.nsList, Attribute::NoOverride, false, s, 0);


// Adding 'toString' etc to the Object.prototype XXX Or make this a static class member?
        fInst = createFunctionInstance(env, true, true, Object_toString, 0, NULL);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), engine->toString_StringAtom, OBJECT_TO_JS2VAL(fInst), ReadAccess, true, false);
        // and 'valueOf'
        fInst = createFunctionInstance(env, true, true, Object_valueOf, 0, NULL);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), engine->valueOf_StringAtom, OBJECT_TO_JS2VAL(fInst), ReadAccess, true, false);
        // and 'constructor'
        fInst = createFunctionInstance(env, true, true, Object_Constructor, 0, NULL);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), world.identifiers["constructor"], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
		// and various property/enumerable functions
        fInst = createFunctionInstance(env, true, true, Object_hasOwnProperty, 0, NULL);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), world.identifiers["hasOwnProperty"], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
        fInst = createFunctionInstance(env, true, true, Object_isPropertyOf, 0, NULL);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), world.identifiers["isPropertyOf"], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
        fInst = createFunctionInstance(env, true, true, Object_propertyIsEnumerble, 0, NULL);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), world.identifiers["propertyIsEnumerable"], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);


/*** ECMA 4  Integer Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(integerClass), true);
        defineLocalMember(env, world.identifiers["Integer"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        

/*** ECMA 3  Date Class ***/
        MAKEBUILTINCLASS(dateClass, objectClass, true, true, world.identifiers["Date"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(dateClass), true);
        defineLocalMember(env, world.identifiers["Date"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initDateObject(this);

/*** ECMA 3  RegExp Class ***/
        MAKEBUILTINCLASS(regexpClass, objectClass, true, true, world.identifiers["RegExp"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(regexpClass), true);
        defineLocalMember(env, world.identifiers["RegExp"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initRegExpObject(this);

/*** ECMA 3  String Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(stringClass), true);
        defineLocalMember(env, world.identifiers["String"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initStringObject(this);

/*** ECMA 3  Number Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(numberClass), true);
        defineLocalMember(env, world.identifiers["Number"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initNumberObject(this);

/*** ECMA 3  Boolean Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(booleanClass), true);
        defineLocalMember(env, world.identifiers["Boolean"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initBooleanObject(this);

/*** ECMA 3  Math Object ***/
        MAKEBUILTINCLASS(mathClass, objectClass, false, true, world.identifiers["Math"], JS2VAL_FALSE);
        SimpleInstance *mathObject = new (this) SimpleInstance(this, objectClass->prototype, mathClass);
        v = new Variable(objectClass, OBJECT_TO_JS2VAL(mathObject), true);
        defineLocalMember(env, world.identifiers["Math"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initMathObject(this, mathObject);

/*** ECMA 3  Array Class ***/
        arrayClass = new (this) JS2ArrayClass(objectClass, NULL, new (this) Namespace(engine->private_StringAtom), true, true, world.identifiers["Array"]); arrayClass->complete = true; arrayClass->defaultValue = JS2VAL_NULL;        
        v = new Variable(classClass, OBJECT_TO_JS2VAL(arrayClass), true);
        defineLocalMember(env, world.identifiers["Array"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initArrayObject(this);
/*
        Multiname length_mn(&world.identifiers["length"], publicNamespace);
        fInst = createFunctionInstance(env, true, true, Array_lengthGet, 0, NULL);
        g = new InstanceGetter(&length_mn, fInst, objectClass, true, true);
        defineInstanceMember(arrayClass, &cxt, length_mn.name, *length_mn.nsList, Attribute::NoOverride, false, g, 0);
        fInst = createFunctionInstance(env, true, true, Array_lengthSet, 0, NULL);
        s = new InstanceSetter(&length_mn, fInst, objectClass, true, true);
        defineInstanceMember(arrayClass, &cxt, length_mn.name, *length_mn.nsList, Attribute::NoOverride, false, s, 0);
*/
/*** ECMA 3  Error Classes ***/
        MAKEBUILTINCLASS(errorClass, objectClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(errorClass), true);
        defineLocalMember(env, world.identifiers["Error"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(evalErrorClass, errorClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(evalErrorClass), true);
        defineLocalMember(env, world.identifiers["EvalError"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(rangeErrorClass, errorClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(rangeErrorClass), true);
        defineLocalMember(env, world.identifiers["RangeError"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(referenceErrorClass, errorClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(referenceErrorClass), true);
        defineLocalMember(env, world.identifiers["ReferenceError"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(syntaxErrorClass, errorClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(syntaxErrorClass), true);
        defineLocalMember(env, world.identifiers["SyntaxError"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(typeErrorClass, errorClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(typeErrorClass), true);
        defineLocalMember(env, world.identifiers["TypeError"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(uriErrorClass, errorClass, true, true, world.identifiers["Error"], JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(uriErrorClass), true);
        defineLocalMember(env, world.identifiers["URIError"], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initErrorObject(this);

    }

    JS2Metadata::~JS2Metadata()
    {
        bConList.clear();
        targetList.clear();
        clear();                // don't blow off the contents of 'this' as the destructors for
                                // embedded objects will get messed up (as they run on exit).
        delete engine;
		delete forbiddenMember;
        if (bCon) delete bCon;
    }

    JS2Class *JS2Metadata::objectType(JS2Object *obj)
    {
        switch (obj->kind) {
        case AttributeObjectKind:
            return attributeClass;
        case MultinameKind:
            return namespaceClass;
        case ClassKind:
            return classClass;

        case SimpleInstanceKind: 
            return checked_cast<SimpleInstance *>(obj)->type;

        case PackageKind:
            return packageClass;

        case ParameterFrameKind: 
            return objectClass;

        case SystemKind:
        case BlockFrameKind: 
        default:
            ASSERT(false);
            return NULL;
        }
    }

    // objectType(o) returns an OBJECT o's most specific type.
    JS2Class *JS2Metadata::objectType(js2val objVal)
    {
        if (JS2VAL_IS_NULL(objVal))
            return nullClass;
        if (JS2VAL_IS_OBJECT(objVal))
            return objectType(JS2VAL_TO_OBJECT(objVal));
        if (JS2VAL_IS_VOID(objVal))
            return undefinedClass;
        if (JS2VAL_IS_NUMBER(objVal))
            return numberClass;
        if (JS2VAL_IS_STRING(objVal)) {
            if (JS2VAL_TO_STRING(objVal)->length() == 1)
                return stringClass; // XXX characterClass; Need the connection from class Character to
                                    // class String - i.e. access to all the functions in 'String.prototype' - 
                                    // but (some of) those routines depened on being called on a StringInstance...
            else 
                return stringClass;
        }
        if (JS2VAL_IS_BOOLEAN(objVal))
            return booleanClass;
        ASSERT(false);
        return NULL;
    }


    // hasType(o, c) returns true if o is an instance of class c (or one of c's subclasses). 
    // It considers null to be an instance of the classes Null and Object only.
    bool JS2Metadata::hasType(js2val objVal, JS2Class *c)
    {
        return c->isAncestor(objectType(objVal));
    }

    InstanceMember *JS2Metadata::findLocalInstanceMember(JS2Class *limit, Multiname *multiname, Access access)
    {
        InstanceMember *result = NULL;
        InstanceBindingEntry **ibeP = limit->instanceBindings[multiname->name];
        if (ibeP) {
            for (InstanceBindingEntry::NS_Iterator i = (*ibeP)->begin(), end = (*ibeP)->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding &ns = *i;
                if ((ns.second->accesses & access) && multiname->listContains(ns.first)) {
                    if (result && (ns.second->content != result))
                        reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                    else
                        result = ns.second->content;
                }
            }
        }
        return result;
    }        

    // Start from the root class (Object) and proceed through more specific classes that are ancestors of c
    InstanceMember *JS2Metadata::findBaseInstanceMember(JS2Class *c, Multiname *multiname, Access access)
    {
        InstanceMember *result = NULL;
        if (c->super) {
            result = findBaseInstanceMember(c->super, multiname, access);
            if (result) return result;
        }
        return findLocalInstanceMember(c, multiname, access);
    }

    // getDerivedInstanceMember returns the most derived instance member whose name includes that of mBase and 
    // whose access includes access. The caller of getDerivedInstanceMember ensures that such a member always exists. 
    // If accesses is readWrite then it is possible that this search could find both a getter and a setter defined in the same class;
    // in this case either the getter or the setter is returned at the implementation's discretion
    InstanceMember *JS2Metadata::getDerivedInstanceMember(JS2Class *c, InstanceMember *mBase, Access access)
    {
        InstanceBindingEntry **ibeP = c->instanceBindings[mBase->multiname->name];
        if (ibeP) {
            for (InstanceBindingEntry::NS_Iterator i = (*ibeP)->begin(), end = (*ibeP)->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding &ns = *i;
                if ((ns.second->accesses & access) && mBase->multiname->listContains(ns.first))
                    return ns.second->content;
            }
        }
        ASSERT(c->super);
        return getDerivedInstanceMember(c->super, mBase, access);
    }

    LocalMember *JS2Metadata::findLocalMember(JS2Object *container, Multiname *multiname, Access access, bool &enumerable)
    {
        LocalMember *found = NULL;
        LocalBindingMap *lMap;
        if (container->kind == SimpleInstanceKind)
            lMap = &checked_cast<SimpleInstance *>(container)->localBindings;
        else
            lMap = &checked_cast<NonWithFrame *>(container)->localBindings;
        
        if (lMap->size()) {
            LocalBindingEntry **lbeP = (*lMap)[multiname->name];
            if (lbeP) {
                for (LocalBindingEntry::NS_Iterator i = (*lbeP)->begin(), end = (*lbeP)->end(); (i != end); i++) {
                    LocalBindingEntry::NamespaceBinding &ns = *i;
                    if ((ns.second->accesses & access) && multiname->listContains(ns.first)) {
                        if (found && (ns.second->content != found))
                            reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                        else {
                            found = ns.second->content;
						    enumerable = ns.second->enumerable;
                        }
                    }
                }
            }
        }
        return found;
    }

    js2val JS2Metadata::getSuperObject(JS2Object *obj)
    {
        switch (obj->kind) {
        case SimpleInstanceKind:
            return checked_cast<SimpleInstance *>(obj)->super;
            break;
        case PackageKind:
            return checked_cast<Package *>(obj)->super;
            break;
        case ClassKind:
            return OBJECT_TO_JS2VAL(checked_cast<JS2Class *>(obj)->super);
        default:
            return JS2VAL_NULL;
        }
    }

    Member *JS2Metadata::findCommonMember(js2val *base, Multiname *multiname, Access access, bool flat)
    {
		bool isEnumerable;
        Member *m = NULL;
        if (JS2VAL_IS_PRIMITIVE(*base) && cxt.E3compatibility) {
            *base = toObject(*base);      // XXX E3 compatibility...
        }
        JS2Object *baseObj = JS2VAL_TO_OBJECT(*base);
        switch (baseObj->kind) {
        case SimpleInstanceKind:
            m = findLocalMember(baseObj, multiname, access, isEnumerable);
            break;
        case PackageKind:
            m = findLocalMember(baseObj, multiname, access, isEnumerable);
            break;
        case ClassKind:
            m = findLocalMember(baseObj, multiname, access, isEnumerable);
            if (m == NULL)
                m = findLocalInstanceMember(checked_cast<JS2Class *>(baseObj), multiname, access);
            break;
        default:
            return NULL;    // non-primitive, but not one of the above
        }
        if (m)
            return m;
        js2val superVal = getSuperObject(baseObj);
        if (!JS2VAL_IS_NULL(superVal) && !JS2VAL_IS_UNDEFINED(superVal)) {
            m = findCommonMember(&superVal, multiname, access, flat);
            if ((m != NULL) && flat 
                        && ((m->memberKind == Member::DynamicVariableMember)
                                || (m->memberKind == Member::FrameVariableMember)))
                m = NULL;
        }
        return m;
    }

    bool JS2Metadata::readInstanceMember(js2val containerVal, JS2Class *c, InstanceMember *mBase, Phase phase, js2val *rval)
    {
        InstanceMember *m = getDerivedInstanceMember(c, mBase, ReadAccess);
        switch (m->memberKind) {
        case Member::InstanceVariableMember:
            {
                InstanceVariable *mv = checked_cast<InstanceVariable *>(m);
                if ((phase == CompilePhase) && !mv->immutable)
                    reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
                Slot *s = findSlot(containerVal, mv);
                if (JS2VAL_IS_UNINITIALIZED(s->value))
                    reportError(Exception::uninitializedError, "Reference to uninitialized instance variable", engine->errorPos());
                *rval = s->value;
                return true;
            }
        case Member::InstanceMethodMember:
            {
                InstanceMethod *im = checked_cast<InstanceMethod *>(m);
                if (phase == CompilePhase)
                    reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
                FunctionInstance *fInst = new (this) FunctionInstance(this, functionClass->prototype, functionClass);
                fInst->isMethodClosure = true;
                fInst->fWrap = im->fInst->fWrap;
                fInst->thisObject = containerVal;
                *rval = OBJECT_TO_JS2VAL(fInst);
                return true;
            }
        case Member::InstanceGetterMember:
            {
                InstanceGetter *ig = checked_cast<InstanceGetter *>(m);
                *rval = invokeFunction(ig->fInst, containerVal, NULL, 0, NULL);
                return true;
            }
        default:
            ASSERT(false);
        }
        return false;
    }

    bool JS2Metadata::writeInstanceMember(js2val containerVal, JS2Class *c, InstanceMember *mBase, js2val newValue)
    {
        InstanceMember *m = getDerivedInstanceMember(c, mBase, WriteAccess);
        switch (m->memberKind) {
        case Member::InstanceVariableMember:
            {
                InstanceVariable *mv = checked_cast<InstanceVariable *>(m);
                Slot *s = findSlot(containerVal, mv);
                if (mv->immutable && !JS2VAL_IS_UNINITIALIZED(s->value))
                    reportError(Exception::compileExpressionError, "Reinitialization of constant", engine->errorPos());
                s->value = mv->type->ImplicitCoerce(this, newValue);
                return true;
            }
        case Member::InstanceSetterMember:
            {
                InstanceSetter *is = checked_cast<InstanceSetter *>(m);
                invokeFunction(is->fInst, containerVal, &newValue, 1, NULL);
                return true;
            }
        default:
            ASSERT(false);
        }
        return false;
    }

    bool JS2Metadata::readLocalMember(LocalMember *m, Phase phase, js2val *rval, Frame *container)
    {
        switch (m->memberKind) {
        case LocalMember::ForbiddenMember:
            reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
            break;
        case LocalMember::VariableMember:
            {
                Variable *v = checked_cast<Variable *>(m); 
                if ((phase == CompilePhase) && !v->immutable)
                    reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
                *rval = v->value;
                return true;
            }
        case LocalMember::FrameVariableMember:
            {
                ASSERT(container);
                if (phase == CompilePhase) 
                    reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
                FrameVariable *fv = checked_cast<FrameVariable *>(m);
                switch (fv->kind) {
                case FrameVariable::Package:
                    {
                        ASSERT(container->kind == PackageKind);
                        *rval = (*(checked_cast<Package *>(container))->frameSlots)[fv->frameSlot];
                    }
                    break;
                case FrameVariable::Local:
                    {
                        ASSERT(container->kind == BlockFrameKind);
                        *rval = (*(checked_cast<NonWithFrame *>(container))->frameSlots)[fv->frameSlot];
                    }
                    break;
                case FrameVariable::Parameter:
                    {
                        ASSERT(container->kind == ParameterFrameKind);
                        ASSERT(fv->frameSlot < (checked_cast<ParameterFrame *>(container))->frameSlots->size());
                        *rval = (checked_cast<ParameterFrame *>(container))->argSlots[fv->frameSlot];
//                        *rval = (*(checked_cast<ParameterFrame *>(container))->frameSlots)[fv->frameSlot];
                    }
                    break;
                }
            }
            return true;
        case LocalMember::DynamicVariableMember:
            if (phase == CompilePhase) 
                reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
            *rval = (checked_cast<DynamicVariable *>(m))->value;
            return true;
        case LocalMember::ConstructorMethodMember:
            if (phase == CompilePhase)
                reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
            *rval = (checked_cast<ConstructorMethod *>(m))->value;
            return true;
        case LocalMember::GetterMember:
            {
                Getter *g = checked_cast<Getter *>(m);
                *rval = invokeFunction(g->code, JS2VAL_VOID, NULL, 0, NULL);
            }
            return true;
        case LocalMember::SetterMember:
            ASSERT(false);
            break;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    // Write a value to the local member
    bool JS2Metadata::writeLocalMember(LocalMember *m, js2val newValue, bool initFlag, Frame *container)
    {
        switch (m->memberKind) {
        case LocalMember::ForbiddenMember:
        case LocalMember::ConstructorMethodMember:
            reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
            break;
        case LocalMember::VariableMember:
            {
                Variable *v = checked_cast<Variable *>(m);
                if (!initFlag
                        && (JS2VAL_IS_INACCESSIBLE(v->value) 
                                || (v->immutable && !JS2VAL_IS_UNINITIALIZED(v->value))))
                    if (!cxt.E3compatibility)
                        reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
                    else    // quietly ignore the write for JS1 compatibility
                        return true;
                v->value = v->type->ImplicitCoerce(this, newValue);
            }
            return true;
        case LocalMember::FrameVariableMember:
            {
                ASSERT(container);
                FrameVariable *fv = checked_cast<FrameVariable *>(m);
                switch (fv->kind) {
                case FrameVariable::Package:
                    {
                        ASSERT(container->kind == PackageKind);
                        (*(checked_cast<Package *>(container))->frameSlots)[fv->frameSlot] = newValue;
                    }
                    break;
                case FrameVariable::Local:
                    {
                        ASSERT(container->kind == BlockFrameKind);
                        (*(checked_cast<NonWithFrame *>(container))->frameSlots)[fv->frameSlot] = newValue;
                    }
                    break;
                case FrameVariable::Parameter:
                    {
                        ASSERT(container->kind == ParameterFrameKind);
                        ASSERT(fv->frameSlot < (checked_cast<ParameterFrame *>(container))->frameSlots->size());
                        (checked_cast<ParameterFrame *>(container))->argSlots[fv->frameSlot] = newValue;
//                        (*(checked_cast<ParameterFrame *>(container))->frameSlots)[fv->frameSlot] = newValue;
                    }
                    break;
                }
            }
            return true;
        case LocalMember::DynamicVariableMember:
            (checked_cast<DynamicVariable *>(m))->value = newValue;
            return true;
        case LocalMember::GetterMember:
            ASSERT(false);
            break;
        case LocalMember::SetterMember:
            {
                Setter *s = checked_cast<Setter *>(m);
                invokeFunction(s->code, JS2VAL_VOID, &newValue, 1, NULL);
            }
            return true;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    bool JS2Metadata::hasOwnProperty(JS2Object *obj, const StringAtom &name)
    {
        Multiname *mn = new (this) Multiname(name, publicNamespace);
        DEFINE_ROOTKEEPER(this, rk, mn);
        js2val val = OBJECT_TO_JS2VAL(obj);
        return (findCommonMember(&val, mn, ReadWriteAccess, true) != NULL);
    }

    // Add the local member with access etc. by name into the map, using the public namespace. An entry may or may not be in the map already
    void JS2Metadata::addPublicVariableToLocalMap(LocalBindingMap *lMap, const StringAtom &name, LocalMember *v, Access access, bool enumerable)
    {
        LocalBinding *new_b = new LocalBinding(access, v, enumerable);
        LocalBindingEntry **lbeP = (*lMap)[name];
        LocalBindingEntry *lbe;
        if (lbeP == NULL) {
            lbe = new LocalBindingEntry(name);
            lMap->insert(name, lbe);
        }
        else
            lbe = *lbeP;
        lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(publicNamespace, new_b));
    }

    // The caller must make sure that the created property does not already exist and does not conflict with any other property.
    DynamicVariable *JS2Metadata::createDynamicProperty(JS2Object *obj, const StringAtom &name, js2val initVal, Access access, bool sealed, bool enumerable)
    {
        DynamicVariable *dv = new DynamicVariable(initVal, sealed);
        LocalBindingMap *lMap;
        if (obj->kind == SimpleInstanceKind)
            lMap = &checked_cast<SimpleInstance *>(obj)->localBindings;
        else
            if (obj->kind == PackageKind)
                lMap = &checked_cast<Package *>(obj)->localBindings;
            else
                ASSERT(false);
        addPublicVariableToLocalMap(lMap, name, dv, access, enumerable);
        return dv;
    }


    // Use the slotIndex from the instanceVariable to access the slot
    Slot *JS2Metadata::findSlot(js2val thisObjVal, InstanceVariable *id)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisObjVal) 
                    && (JS2VAL_TO_OBJECT(thisObjVal)->kind == SimpleInstanceKind));
        JS2Object *thisObj = JS2VAL_TO_OBJECT(thisObjVal);
        return &checked_cast<SimpleInstance *>(thisObj)->fixedSlots[id->slotIndex];
    }


    // gc-mark all contained JS2Objects and their children 
    // and then invoke mark on all other structures that contain JS2Objects
    void JS2Metadata::markChildren()
    {        
        // XXX - maybe have a separate pool to allocate chunks
        // that are meant to be never collected?
        GCMARKOBJECT(publicNamespace);
        forbiddenMember->mark();

        GCMARKOBJECT(objectClass);
        GCMARKOBJECT(undefinedClass);
        GCMARKOBJECT(nullClass);
        GCMARKOBJECT(booleanClass);
        GCMARKOBJECT(generalNumberClass);
        GCMARKOBJECT(integerClass);
        GCMARKOBJECT(numberClass);
        GCMARKOBJECT(mathClass);
        GCMARKOBJECT(characterClass);
        GCMARKOBJECT(stringClass);
        GCMARKOBJECT(namespaceClass);
        GCMARKOBJECT(attributeClass);
        GCMARKOBJECT(classClass);
        GCMARKOBJECT(functionClass);
        GCMARKOBJECT(packageClass);
        GCMARKOBJECT(dateClass);
        GCMARKOBJECT(regexpClass);
        GCMARKOBJECT(arrayClass);
        GCMARKOBJECT(errorClass);
        GCMARKOBJECT(evalErrorClass);
        GCMARKOBJECT(rangeErrorClass);
        GCMARKOBJECT(referenceErrorClass);
        GCMARKOBJECT(syntaxErrorClass);
        GCMARKOBJECT(typeErrorClass);
        GCMARKOBJECT(uriErrorClass);
        GCMARKOBJECT(argumentsClass);

        for (BConListIterator i = bConList.begin(), end = bConList.end(); (i != end); i++)
            (*i)->mark();
        if (bCon)
            bCon->mark();
        if (engine)
            engine->mark();
        GCMARKOBJECT(env);

        GCMARKOBJECT(glob);
    
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
        Reader reader(engine->bCon->mSource, engine->bCon->mSourceLocation, 1);
        reader.fillLineStartsTable();
        uint32 lineNum = reader.posToLineNum(pos);
        size_t linePos = reader.getLine(lineNum, lineBegin, lineEnd);
        ASSERT(lineBegin && lineEnd && linePos <= pos);
        throw Exception(kind, x, reader.sourceLocation, 
                            lineNum, pos - linePos, pos, lineBegin, lineEnd);
    }

    // Accepts a String as the error argument and converts to char *
    void JS2Metadata::reportError(Exception::Kind kind, const char *message, size_t pos, const String &name)
    {
        std::string str(name.length(), char());
        std::transform(name.begin(), name.end(), str.begin(), narrow);
        reportError(kind, message, pos, str.c_str());
    }

    // Accepts a String * as the error argument and converts to char *
    void JS2Metadata::reportError(Exception::Kind kind, const char *message, size_t pos, const String *name)
    {
        std::string str(name->length(), char());
        std::transform(name->begin(), name->end(), str.begin(), narrow);
        reportError(kind, message, pos, str.c_str());
    }

    // Called after initBuiltinClass and after the prototype object is constructed
    void JS2Metadata::initBuiltinClassPrototype(JS2Class *builtinClass, FunctionData *protoFunctions)
    {
        env->addFrame(builtinClass);
        {
            Variable *v = new Variable(builtinClass, OBJECT_TO_JS2VAL(builtinClass->prototype), true);
            defineLocalMember(env, engine->prototype_StringAtom, NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
        }    
        env->removeTopFrame();

        // Add "constructor" as a dynamic property of the prototype
/*
        FunctionInstance *fInst = createFunctionInstance(env, true, true, builtinClass->construct, 0, NULL);
        ASSERT(JS2VAL_IS_OBJECT(builtinClass->prototype));
        createDynamicProperty(JS2VAL_TO_OBJECT(builtinClass->prototype), &world.identifiers["constructor"], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
*/
		// XXX Set the class object itself as the value of 'constructor'
        ASSERT(JS2VAL_IS_OBJECT(builtinClass->prototype));
        createDynamicProperty(JS2VAL_TO_OBJECT(builtinClass->prototype), world.identifiers["constructor"], OBJECT_TO_JS2VAL(builtinClass), ReadWriteAccess, false, false);

        FunctionData *pf = protoFunctions;
        if (pf) {
            while (pf->name) {
/*
                SimpleInstance *callInst = new SimpleInstance(this, functionClass->prototype, functionClass);
                callInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code, env);
                callInst->fWrap->length = pf->length;
                Multiname *mn = new Multiname(&world.identifiers[pf->name], publicNamespace);
                InstanceMember *m = new InstanceMethod(mn, callInst, true, false);
                defineInstanceMember(builtinClass, &cxt, mn->name, *mn->nsList, Attribute::NoOverride, false, m, 0);
*/
                FunctionInstance *fInst = createFunctionInstance(env, true, true, pf->code, pf->length, NULL);
                createDynamicProperty(JS2VAL_TO_OBJECT(builtinClass->prototype), world.identifiers[pf->name], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
                pf++;
            }
        }
    }

    void JS2Metadata::initBuiltinClass(JS2Class *builtinClass, FunctionData *staticFunctions, NativeCode *construct, NativeCode *call)
    {
        FunctionData *pf;

        builtinClass->construct = construct;
        builtinClass->call = call;

        // Adding "prototype" & "length", etc as static members of the class - not dynamic properties; XXX
        env->addFrame(builtinClass);
        {
            Variable *v = new Variable(integerClass, INT_TO_JS2VAL(1), true);
            defineLocalMember(env, engine->length_StringAtom, NULL, Attribute::NoOverride, false, ReadAccess, v, 0, false);
            
            pf = staticFunctions;
            if (pf) {
                while (pf->name) {
                    FunctionInstance *callInst = createFunctionInstance(env, true, true, pf->code, pf->length, NULL);
                    v = new Variable(functionClass, OBJECT_TO_JS2VAL(callInst), true);
                    defineLocalMember(env, world.identifiers[pf->name], NULL, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
                    pf++;
                }
            }
        }
        env->removeTopFrame();
    
    }

    // Add a pointer to the (address of a) gc-allocated object to the root list
    // (Note - we hand out an iterator, so it's essential to
    // use something like std::list that doesn't mess with locations)
    RootIterator JS2Metadata::addRoot(RootKeeper *t)
    {
        return rootList.insert(rootList.end(), t);
    }

    // Remove a root pointer
    void JS2Metadata::removeRoot(RootIterator ri)
    {
        rootList.erase(ri);
    }

    // Remove everything, regardless of rootlist
    void JS2Metadata::clear()
    {
        pond.resetMarks();
        pond.moveUnmarkedToFreeList(this);
    }

    // Mark all reachable objects and put the rest back on the freelist
    uint32 JS2Metadata::gc()
    {
        pond.resetMarks();
        markChildren();
        // Anything on the root list may also be a pointer to a JS2Object.
        for (RootIterator i = rootList.begin(), end = rootList.end(); (i != end); i++) {
            RootKeeper *r = *i;
            PondScum *scum = NULL;
            if (r->is_js2val) {
				if (r->js2val_count) {
					js2val *valp = *((js2val **)(r->p));
					for (uint32 c = 0; c < r->js2val_count; c++)
						JS2Object::markJS2Value(*valp++);
				}
				else {
					js2val *valp = (js2val *)(r->p);
					JS2Object::markJS2Value(*valp);
				}
            }
            else {
                JS2Object **objp = (JS2Object **)(r->p);
                if (*objp) {
                    scum = ((PondScum *)(*objp) - 1);
                    ASSERT(scum->owner && (scum->getSize() >= sizeof(PondScum)) && (scum->owner->sanity == POND_SANITY));
                    if (scum->isJS2Object()) {
                        JS2Object *obj = (JS2Object *)(scum + 1);
                        GCMARKOBJECT(obj)
                    }
                    else
                        JS2Object::mark(scum + 1);
                }
            }
        }
        return pond.moveUnmarkedToFreeList(this);
    }

    // Allocate a chunk of size s
    void *JS2Metadata::alloc(size_t s, PondScum::ScumFlag flag)
    {
        s += sizeof(PondScum);
        // make sure that the thing is a multiple of 16 bytes
        if (s & 0xF) s += 16 - (s & 0xF);
        ASSERT(s <= 0x7FFFFFFF);
        void *p = pond.allocFromPond(this, s, flag);
        ASSERT(((ptrdiff_t)p & 0xF) == 0);
        return p;
    }

    // Release a chunk back to it's pond
    void JS2Metadata::unalloc(void *t)
    {
        PondScum *p = (PondScum *)t - 1;
        ASSERT(p->owner && (p->getSize() >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
        p->owner->returnToPond(p);
    }

   
    
 /************************************************************************************
 *
 *  JS2Class
 *
 ************************************************************************************/

    JS2Class::JS2Class(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name) 
        : NonWithFrame(ClassKind), 
            super(super), 
            instanceInitOrder(NULL), 
            complete(false),
            name(name),
            prototype(proto), 
            typeofString(name),
            privateNamespace(privateNamespace), 
            dynamic(dynamic),
            final(final),
            defaultValue(JS2VAL_NULL),
            call(NULL),
            construct(JS2Engine::defaultConstructor),
            init(NULL),
            slotCount(super ? super->slotCount : 0)
    {
    }

    JS2Class::~JS2Class()            
    {
        for (InstanceBindingIterator rib = instanceBindings.begin(), riend = instanceBindings.end(); (rib != riend); rib++) {
            InstanceBindingEntry *ibe = *rib;
            for (InstanceBindingEntry::NS_Iterator i = ibe->begin(), end = ibe->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding ns = *i;
                delete ns.second;
            }
            delete ibe;
        }
    }


    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void JS2Class::markChildren()
    {
        NonWithFrame::markChildren();
        GCMARKOBJECT(super)
        GCMARKVALUE(prototype);
        GCMARKOBJECT(privateNamespace)
        GCMARKOBJECT(init)
        GCMARKVALUE(defaultValue);
        for (InstanceBindingIterator rib = instanceBindings.begin(), riend = instanceBindings.end(); (rib != riend); rib++) {
            InstanceBindingEntry *ibe = *rib;
            for (InstanceBindingEntry::NS_Iterator i = ibe->begin(), end = ibe->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding ns = *i;
                ns.second->content->mark();
            }
        }
    }

    // return true if 'heir' is this class or is any antecedent
    bool JS2Class::isAncestor(JS2Class *heir)
    {
        JS2Class *kinsman = this;
        do {
            if (heir == kinsman)
                return true;
            kinsman = kinsman->super;
        } while (kinsman);
        return false;
    }

    void JS2Class::emitDefaultValue(BytecodeContainer *bCon, size_t pos)
    {
        if (JS2VAL_IS_NULL(defaultValue))
            bCon->emitOp(eNull, pos);
        else 
        if (JS2VAL_IS_VOID(defaultValue))
            bCon->emitOp(eUndefined, pos);
        else 
        if (JS2VAL_IS_BOOLEAN(defaultValue) && !JS2VAL_TO_BOOLEAN(defaultValue))
            bCon->emitOp(eFalse, pos);
        else
        if ((JS2VAL_IS_LONG(defaultValue) || JS2VAL_IS_ULONG(defaultValue)) 
                && (*JS2VAL_TO_LONG(defaultValue) == 0))
            bCon->emitOp(eLongZero, pos);
        else
        if (JS2VAL_IS_INT(defaultValue)) {
            bCon->emitOp(eInteger, pos);
            bCon->addInt32(JS2VAL_TO_INT(defaultValue));
        }
        else
            NOT_REACHED("unrecognized default value");
    }


/************************************************************************************
 *
 *  LocalBindingEntry
 *
 ************************************************************************************/

    // Clear all the clone contents for this entry
    void LocalBindingEntry::clear()
    {
        for (NS_Iterator i = bindingList.begin(), end = bindingList.end(); (i != end); i++) {
            NamespaceBinding &ns = *i;
            ns.second->content->cloneContent = NULL;
        }
    }

    // This version of 'clone' is used to construct a duplicate LocalBinding entry 
    // with each LocalBinding content copied from the (perhaps previously) cloned member.
    // See 'instantiateFrame'.
    LocalBindingEntry *LocalBindingEntry::clone()
    {
        LocalBindingEntry *new_e = new LocalBindingEntry(name);
        for (NS_Iterator i = bindingList.begin(), end = bindingList.end(); (i != end); i++) {
            NamespaceBinding &ns = *i;
            LocalBinding *m = ns.second;
            if (m->content->cloneContent == NULL) {
                m->content->cloneContent = m->content->clone();
            }
            LocalBinding *new_b = new LocalBinding(m->accesses, m->content->cloneContent, m->enumerable);
            new_b->xplicit = m->xplicit;
            new_e->bindingList.push_back(NamespaceBinding(ns.first, new_b));
        }
        return new_e;
    }


/************************************************************************************
 *
 *  SimpleInstance
 *
 ************************************************************************************/

    
    void SimpleInstance::initializeSlots(JS2Class *type)
    {
        if (type->super)
            initializeSlots(type->super);
        for (InstanceBindingIterator rib = type->instanceBindings.begin(), riend = type->instanceBindings.end(); (rib != riend); rib++) {
            InstanceBindingEntry *ibe = *rib;
            for (InstanceBindingEntry::NS_Iterator i = ibe->begin(), end = ibe->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding ns = *i;
                InstanceMember *im = ns.second->content;
                if (im->memberKind == Member::InstanceVariableMember) {
                    InstanceVariable *iv = checked_cast<InstanceVariable *>(im);
                    fixedSlots[iv->slotIndex].value = iv->defaultValue;
                }
            }
        }
    }

    // Construct a Simple instance of a class. Set the
    // initial value of all slots to uninitialized.
    SimpleInstance::SimpleInstance(JS2Metadata *meta, js2val parent, JS2Class *type) 
        : JS2Object(SimpleInstanceKind),
            sealed(false),
            super(parent),
            type(type), 
            fixedSlots(new Slot[type->slotCount])
    {
        for (uint32 i = 0; i < type->slotCount; i++) {
            fixedSlots[i].value = JS2VAL_UNINITIALIZED;
        }
        initializeSlots(type);
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void SimpleInstance::markChildren()
    {
        GCMARKOBJECT(type)
        GCMARKVALUE(super);
        if (fixedSlots) {
            ASSERT(type);
            for (uint32 i = 0; (i < type->slotCount); i++) {
                GCMARKVALUE(fixedSlots[i].value);
            }
        }
        for (LocalBindingIterator bi = localBindings.begin(), bend = localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                ns.second->content->mark();
            }
        }            
    }

    SimpleInstance::~SimpleInstance()
    {
        for (LocalBindingIterator bi = localBindings.begin(), bend = localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                delete ns.second;
            }
            delete lbe;
        }
        delete [] fixedSlots;
    }


 /************************************************************************************
 *
 *  ArrayInstance
 *
 ************************************************************************************/

    ArrayInstance::ArrayInstance(JS2Metadata *meta, js2val parent, JS2Class *type) 
        : SimpleInstance(meta, parent, type) 
    {
        length = 0;
    }

 /************************************************************************************
 *
 *  FunctionInstance
 *
 ************************************************************************************/

    FunctionInstance::FunctionInstance(JS2Metadata *meta, js2val parent, JS2Class *type)
     : SimpleInstance(meta, parent, type), isMethodClosure(false), fWrap(NULL), thisObject(JS2VAL_VOID), sourceText(NULL)
    {
        // Add prototype property
        JS2Object *result = this;
        DEFINE_ROOTKEEPER(meta, rk1, result);

        JS2Object *protoObj = new (meta) SimpleInstance(meta, meta->objectClass->prototype, meta->objectClass);
        DEFINE_ROOTKEEPER(meta, rk2, protoObj);

        meta->createDynamicProperty(this, meta->engine->prototype_StringAtom, OBJECT_TO_JS2VAL(protoObj), ReadWriteAccess, true, false);
        meta->createDynamicProperty(protoObj, meta->world.identifiers["constructor"], OBJECT_TO_JS2VAL(this), ReadWriteAccess, true, false);
    }


    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void FunctionInstance::markChildren()
    {
        SimpleInstance::markChildren();
        if (fWrap) {
            GCMARKOBJECT(fWrap->env);
            GCMARKOBJECT(fWrap->compileFrame);
            if (fWrap->bCon)
                fWrap->bCon->mark();
        }
        GCMARKVALUE(thisObject);
		if (sourceText) JS2Object::mark(sourceText);
    }

    FunctionInstance::~FunctionInstance()
    {
        if (fWrap && !isMethodClosure)
            delete fWrap;
    }

/************************************************************************************
 *
 *  ArgumentsInstance
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void ArgumentsInstance::markChildren()
    {
        SimpleInstance::markChildren();
        if (mSlots) {
            for (uint32 i = 0; i < count; i++)
                GCMARKVALUE(mSlots[i]);
        }
        if (mSplitValue) {
            for (uint32 i = 0; i < count; i++)
                GCMARKVALUE(mSplitValue[i]);
        }
    }

    ArgumentsInstance::~ArgumentsInstance()
    {
        if (mSlots)
            delete mSlots;
        if (mSplit)
            delete mSplit;
        if (mSplitValue)
            delete mSplitValue;
    }

	RegExpInstance::~RegExpInstance()             { if (mRegExp) { js_DestroyRegExp(mRegExp); free(mRegExp);} }


/************************************************************************************
 *
 *  InstanceMethod
 *
 ************************************************************************************/

    InstanceMethod::~InstanceMethod()       
    { 
        delete fInst; 
    }

/************************************************************************************
 *
 *  Frame
 *
 ************************************************************************************/

    // Allocate a new value slot in this frame and stick it
    // on the list (which may need to be created) for gc tracking.
    uint16 NonWithFrame::allocateSlot()
    {
        if (frameSlots == NULL)
            frameSlots = new std::vector<js2val>;
        uint16 result = (uint16)(frameSlots->size());
        frameSlots->push_back(JS2VAL_VOID);
        return result;
    }

    NonWithFrame::~NonWithFrame()
    {
        for (LocalBindingIterator bi = localBindings.begin(), bend = localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                delete ns.second;
            }
            delete lbe;
        }
        if (frameSlots)
            delete frameSlots;
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void NonWithFrame::markChildren()
    {
        GCMARKOBJECT(pluralFrame)
        for (LocalBindingIterator bi = localBindings.begin(), bend = localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                ns.second->content->mark();
            }
        }            
        if (frameSlots) {
            for (std::vector<js2val>::iterator i = frameSlots->begin(), end = frameSlots->end(); (i != end); i++)
                GCMARKVALUE(*i);
        }
    }


 /************************************************************************************
 *
 *  Package
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void Package::markChildren()
    {
        NonWithFrame::markChildren();
        GCMARKVALUE(super);
        GCMARKOBJECT(internalNamespace)
        for (LocalBindingIterator bi = localBindings.begin(), bend = localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                ns.second->content->mark();
            }
        }            
    }


 /************************************************************************************
 *
 *  ParameterFrame
 *
 ************************************************************************************/

    void ParameterFrame::instantiate(Environment *env)
    {
//        ASSERT(pluralFrame->kind == ParameterFrameKind);
//        ParameterFrame *plural = checked_cast<ParameterFrame *>(pluralFrame);
//        env->instantiateFrame(pluralFrame, this, !plural->buildArguments);
    }

    // Assume that instantiate has been called, the plural frame will contain
    // the cloned Variables assigned into this (singular) frame. Use the 
    // incoming values to initialize the positionals.
    // Pad out with undefined values if argc is insufficient
    void ParameterFrame::assignArguments(JS2Metadata *meta, JS2Object *fnObj, js2val *argv, uint32 argc)
    {
        uint32 i;
        
        ArgumentsInstance *argsObj = NULL;
        DEFINE_ROOTKEEPER(meta, rk2, argsObj);

		// argCount is the number of slots acquired by the frame, it will
        // be the larger of the number of paramters defined or the number
        // of arguments passed.
        argCount = (frameSlots) ? frameSlots->size() : 0;
		if (argc > argCount)
			argCount = argc;

        if (buildArguments) {
            // If we're building an arguments object, the slots for the parameter frame are located
            // there so that the arguments object itself can survive beyond the life of the function.
            argsObj = new (meta) ArgumentsInstance(meta, meta->objectClass->prototype, meta->argumentsClass);
            if (argCount) {
                argsObj->mSlots = new js2val[argCount];
                argsObj->count = argCount;
                argsObj->mSplit = new bool[argCount];
                for (i = 0; (i < argCount); i++)
                    argsObj->mSplit[i] = false;
            }
            argSlots = argsObj->mSlots;
            // Add the 'arguments' property

            const StringAtom &name = meta->world.identifiers["arguments"];
            LocalBindingEntry **lbeP = localBindings[name];
            if (lbeP == NULL) {
                LocalBindingEntry *lbe = new LocalBindingEntry(name);
                LocalBinding *sb = new LocalBinding(ReadWriteAccess, new Variable(meta->objectClass, OBJECT_TO_JS2VAL(argsObj), false), false);
                lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(meta->publicNamespace, sb));
                localBindings.insert(name, lbe);
            }
            else {
                LocalBindingEntry::NamespaceBinding &ns = *((*lbeP)->begin());
                ASSERT(ns.first == meta->publicNamespace);
                ASSERT(ns.second->content->memberKind == Member::VariableMember);
                (checked_cast<Variable *>(ns.second->content))->value = OBJECT_TO_JS2VAL(argsObj);
            }
        }
        else {
            if (argSlots && argSlotsOwner)
                delete [] argSlots;
            if (argCount) {
                if (argCount <= argc) {
                    argSlots = argv;
                    argSlotsOwner = false;
                    return;
                }
                else {
                    argSlots = new js2val[argCount];
                    argSlotsOwner = true;
                }
            }
            else
                argSlots = NULL;
        }

        for (i = 0; (i < argc); i++) {
            if (i < argCount) {
                argSlots[i] = argv[i];
            }
        }
        for ( ; (i < argCount); i++) {
			argSlots[i] = JS2VAL_UNDEFINED;
        }
        if (buildArguments) {
            setLength(meta, argsObj, argc);
            meta->argumentsClass->WritePublic(meta, OBJECT_TO_JS2VAL(argsObj), meta->world.identifiers["callee"], true, OBJECT_TO_JS2VAL(fnObj));
        }
    }


    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void ParameterFrame::markChildren()
    {
        NonWithFrame::markChildren();
        GCMARKVALUE(thisObject);
        if (argSlots) {
            for (uint32 i = 0; i < argCount; i++)
                GCMARKVALUE(argSlots[i]);
        }
    }

    void ParameterFrame::releaseArgs()
    {
        if (!buildArguments && argSlots && argSlotsOwner)
            delete [] argSlots;
        argSlots = NULL;
    }


    ParameterFrame::~ParameterFrame()
    {
        releaseArgs();
    }

 /************************************************************************************
 *
 *  Variable
 *
 ************************************************************************************/

    void Variable::mark()                 
    { 
        GCMARKVALUE(value); 
        GCMARKOBJECT(type)
    }


 /************************************************************************************
 *
 *  BlockFrame
 *
 ************************************************************************************/

    void BlockFrame::instantiate(Environment *env)
    {
        if (pluralFrame)
            env->instantiateFrame(pluralFrame, this, true);
    }


 /************************************************************************************
 *
 *  InstanceVariable
 *
 ************************************************************************************/

    // An instance variable type could be future'd when a gc runs (i.e. validate
    // has executed, but the pre-eval stage has yet to determine the actual type)

    void InstanceVariable::mark()                 
    { 
        InstanceMember::mark();
        if (type != FUTURE_TYPE)
            GCMARKOBJECT(type);
    }



 /************************************************************************************
 *
 *  InstanceMethod
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void InstanceMethod::mark()                 
    { 
        InstanceMember::mark();
        GCMARKOBJECT(fInst); 
    }


/************************************************************************************
 *
 *  JS2Object
 *
 ************************************************************************************/

    void JS2Object::markJS2Value(js2val v)
    {
        if (JS2VAL_IS_OBJECT(v)) { 
            JS2Object *obj = JS2VAL_TO_OBJECT(v); 
            GCMARKOBJECT(obj);
        }
        else
        if (JS2VAL_IS_STRING(v)) 
            JS2Object::mark(JS2VAL_TO_STRING(v));
        else
        if (JS2VAL_IS_DOUBLE(v)) 
            JS2Object::mark(JS2VAL_TO_DOUBLE(v));
        else
        if (JS2VAL_IS_LONG(v)) 
            JS2Object::mark(JS2VAL_TO_LONG(v));
        else
        if (JS2VAL_IS_ULONG(v)) 
            JS2Object::mark(JS2VAL_TO_ULONG(v));
        else
        if (JS2VAL_IS_FLOAT(v)) 
            JS2Object::mark(JS2VAL_TO_FLOAT(v));
    }

/************************************************************************************
 *
 *  Pond
 *
 ************************************************************************************/

    Pond::Pond(size_t sz, Pond *next) : sanity(POND_SANITY), pondSize(sz + POND_SIZE), pondBase(new uint8[pondSize]), pondBottom(pondBase), pondTop(pondBase), freeHeader(NULL), nextPond(next) 
    {
        /*
         * Make sure the allocation base is at (n mod 16) == 8.
         * That way, each allocated chunk will have it's returned pointer
         * at (n mod 16) == 0 after allowing for the PondScum header at the
         * beginning.
         */
        int32 offset = ((ptrdiff_t)pondBottom % 16);
        if (offset != 8) {
            if (offset > 8)
                pondBottom += 8 + (16 - offset);
            else
                pondBottom += 8 - offset;
        }
        pondTop = pondBottom;
        pondSize -= (pondTop - pondBase);
    }
    
    // Allocate from this or the next Pond (make a new one if necessary)
    void *Pond::allocFromPond(JS2Metadata *meta, size_t sz, PondScum::ScumFlag flag)
    {
        // See if there's room left...
        if (sz > pondSize) {
            // If not, try the free list...
            PondScum *p = freeHeader;
            PondScum *pre = NULL;
            while (p) {
                ASSERT(p->getSize() > 0);
                if (p->getSize() >= sz) {
                    if (pre)
                        pre->owner = p->owner;
                    else
                        freeHeader = (PondScum *)(p->owner);
                    p->owner = this;
                    p->resetMark();      // might have lingering mark from previous gc
                    p->clearFlags();
                    p->setFlag(flag);
#ifdef DEBUG
                    memset((p + 1), 0xB7, p->getSize() - sizeof(PondScum));
#endif
                    return (p + 1);
                }
                pre = p;
                p = (PondScum *)(p->owner);
            }
            // ok, then try the next Pond
            if (nextPond == NULL) {
                // there isn't one; run the gc
                uint32 released = meta->gc();
                if (released > sz)
                    return meta->alloc(sz - sizeof(PondScum), flag);
                nextPond = new Pond(sz, nextPond);
            }
            return nextPond->allocFromPond(meta, sz, flag);
        }
        // there was room, so acquire it
        PondScum *p = (PondScum *)pondTop;
#ifdef DEBUG
        memset(p, 0xB7, sz);
#endif
        p->owner = this;
        p->setSize(sz);
        p->setFlag(flag);
        pondTop += sz;
        pondSize -= sz;
        return (p + 1);
    }

    // Stick the chunk at the start of the free list
    uint32 Pond::returnToPond(PondScum *p)
    {
        p->owner = (Pond *)freeHeader;
        uint8 *t = (uint8 *)(p + 1);
#ifdef DEBUG
        memset(t, 0xB3, p->getSize() - sizeof(PondScum));
#endif
        freeHeader = p;
        return p->getSize() - sizeof(PondScum);
    }

    // Clear the mark bit from all PondScums
    void Pond::resetMarks()
    {
        uint8 *t = pondBottom;
        while (t != pondTop) {
            PondScum *p = (PondScum *)t;
            p->resetMark();
            t += p->getSize();
        }
        if (nextPond)
            nextPond->resetMarks();
    }

    // Anything left unmarked is now moved to the free list
    uint32 Pond::moveUnmarkedToFreeList(JS2Metadata *meta)
    {
        uint32 released = 0;
        uint8 *t = pondBottom;
        while (t != pondTop) {
            PondScum *p = (PondScum *)t;
            if (!p->isMarked() && (p->owner == this)) {   // (owner != this) ==> already on free list
                if (p->isJS2Object()) {
                    JS2Object *obj = (JS2Object *)(p + 1);
                    obj->finalize();
                    delete obj;
                }
                else
                    if (p->isString()) {
                        String t;
                        String *s = (String *)(p + 1);
                        *s = t;
                    }
                released += returnToPond(p);
            }
            t += p->getSize();
        }
        if (nextPond)
            released += nextPond->moveUnmarkedToFreeList(meta);
        return released;
    }

}; // namespace MetaData
}; // namespace Javascript
