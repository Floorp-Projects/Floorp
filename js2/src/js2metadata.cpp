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
     
    FunctionInstance *JS2Metadata::validateStaticFunction(FunctionDefinition *fnDef, js2val compileThis, bool prototype, bool unchecked, Context *cxt, Environment *env)
    {
        ParameterFrame *compileFrame = new ParameterFrame(compileThis, prototype);
        DEFINE_ROOTKEEPER(rk1, compileFrame);
        
        FunctionInstance *result = new FunctionInstance(this, functionClass->prototype, functionClass);
        DEFINE_ROOTKEEPER(rk2, result);
        result->fWrap = new FunctionWrapper(unchecked, compileFrame, env);
        fnDef->fWrap = result->fWrap;

        Frame *curTopFrame = env->getTopFrame();
        CompilationData *oldData = startCompilationUnit(fnDef->fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
        try {
            env->addFrame(compileFrame);
            VariableBinding *pb = fnDef->parameters;
            if (pb) {
                NamespaceList publicNamespaceList;
                publicNamespaceList.push_back(publicNamespace);
                uint32 pCount = 0;
                while (pb) {
                    pCount++;
                    pb = pb->next;
                }
                if (prototype)
                    createDynamicProperty(result, engine->length_StringAtom, INT_TO_JS2VAL(pCount), ReadAccess, true, false);
                pb = fnDef->parameters;
                compileFrame->positional = new Variable *[pCount];
                compileFrame->positionalCount = pCount;
                pCount = 0;
                while (pb) {
                    // XXX define a static binding for each parameter
                    Variable *v = new Variable(objectClass, JS2VAL_UNDEFINED, false);
                    compileFrame->positional[pCount++] = v;
                    pb->mn = defineLocalMember(env, pb->name, publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, pb->pos, true);
                    pb = pb->next;
                }
            }
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

    /*
     * Validate an individual statement 'p', including it's children
     */
    void JS2Metadata::ValidateStmt(Context *cxt, Environment *env, Plurality pl, StmtNode *p) 
    {
        CompoundAttribute *a = NULL;
        DEFINE_ROOTKEEPER(rk, a);
        Frame *curTopFrame = env->getTopFrame();

        try {
            switch (p->getKind()) {
            case StmtNode::block:
            case StmtNode::group:
                {
                    BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
                    b->compileFrame = new BlockFrame();
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
    /*
        A labelled statement catches contained, named, 'breaks' but simply adds itself as a label for
        contained iteration statements. (i.e. one can 'break' out of a labelled statement, but not 'continue'
        one, however the statement label becomes a 'continuable' label for all contained iteration statements.)
    */
                    // Make sure there is no existing break target with the same name
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
                    g->blockCount = 0;
                    g->tgtID = -1;
                    for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); 
                                ((g->tgtID == -1) && (si != end)); si++) {
                        if (g->name) {
                            // Make sure the name is on the targetList as a viable break target...
                            // (only label statements can introduce names)
                            if ((*si)->getKind() == StmtNode::label) {
                                LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                if (l->name == *g->name) {
                                    g->tgtID = l->labelID;
                                    break;
                                }
                            }
                        }
                        else {
                            // anything at all will do
                            switch ((*si)->getKind()) {
                            case StmtNode::block:
                                g->blockCount++;
                                break;
                            case StmtNode::label:
                                {
                                    LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                    g->tgtID = l->labelID;
                                }
                                break;
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
                        reportError(Exception::syntaxError, "No such break target available", p->pos);
                }
                break;
            case StmtNode::Continue:
                {
                    GoStmtNode *g = checked_cast<GoStmtNode *>(p);
                    g->blockCount = 0;
                    g->tgtID = -1;
                    for (TargetListIterator si = targetList.begin(), end = targetList.end();
                                    ((g->tgtID == -1) && (si != end)); si++) {
                        if (g->name) {
                            // Make sure the name is on the targetList as a viable continue target...
                            if ((*si)->getKind() == StmtNode::label) {
                                LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                if (l->name == *g->name) {
                                    g->tgtID = l->labelID;
                                    break;
                                }
                            }
                        }
                        else {
                            // only some non-label statements will do
                            switch ((*si)->getKind()) {
                            case StmtNode::block:
                                g->blockCount++;
                                break;
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
                            }
                        }
                    }
                    if (g->tgtID == -1) 
                        reportError(Exception::syntaxError, "No such break target available", p->pos);
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
                    Frame *regionalFrame = *env->getRegionalFrame();
                    if (regionalFrame->kind != ParameterKind)
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
                    a = Attribute::toCompoundAttribute(attr);
                    if (a->dynamic)
                        reportError(Exception::definitionError, "Illegal attribute", p->pos);
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
                    bool unchecked = !cxt->strict && (env->getTopFrame()->kind != ClassKind)
                                        && (f->function.prefix == FunctionName::normal) && untyped;
                    bool prototype = unchecked || a->prototype;
                    Attribute::MemberModifier memberMod = a->memberMod;
                    if (env->getTopFrame()->kind == ClassKind) {
                        if (memberMod == Attribute::NoModifier)
                            memberMod = Attribute::Virtual;
                    }
                    else {
                        if (memberMod != Attribute::NoModifier)
                            reportError(Exception::definitionError, "Illegal attribute", p->pos);
                    }
                    if (prototype && ((f->function.prefix != FunctionName::normal) || (memberMod == Attribute::Constructor))) {
                        reportError(Exception::definitionError, "Illegal attribute", p->pos);
                    }
                    js2val compileThis = JS2VAL_VOID;
                    if (prototype || (memberMod == Attribute::Constructor) 
                                  || (memberMod == Attribute::Virtual) 
                                  || (memberMod == Attribute::Final))
                        compileThis = JS2VAL_INACCESSIBLE;
                    Frame *topFrame = env->getTopFrame();

                    switch (memberMod) {
                    case Attribute::NoModifier:
                    case Attribute::Static:
                        {
                            if (f->function.prefix != FunctionName::normal) {
                                // XXX getter/setter --> ????
                            }
                            else {
                                FunctionInstance *fObj = validateStaticFunction(&f->function, compileThis, prototype, unchecked, cxt, env);
                                if (unchecked 
                                        && (f->attributes == NULL)
                                        && ((topFrame->kind == PackageKind)
                                                        || (topFrame->kind == BlockFrameKind)
                                                        || (topFrame->kind == ParameterKind)) ) {
                                    LocalMember *v = defineHoistedVar(env, f->function.name, p, false, OBJECT_TO_JS2VAL(fObj));
                                }
                                else {
                                    Variable *v = new Variable(functionClass, OBJECT_TO_JS2VAL(fObj), true);
                                    defineLocalMember(env, f->function.name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
                                }
                            }
                        }
                        break;
                    case Attribute::Virtual:
                    case Attribute::Final:
                        {
                    // XXX Here the spec. has ???, so the following is tentative
                            FunctionInstance *fObj = validateStaticFunction(&f->function, compileThis, prototype, unchecked, cxt, env);
                            JS2Class *c = checked_cast<JS2Class *>(env->getTopFrame());
                            Multiname *mn = new Multiname(f->function.name, a->namespaces);
                            InstanceMember *m = new InstanceMethod(mn, checked_cast<SimpleInstance *>(fObj), true, true);
                            defineInstanceMember(c, cxt, f->function.name, a->namespaces, a->overrideMod, a->xplicit, m, p->pos);
                        }
                        break;
                    case Attribute::Constructor:
                        {
                    // XXX Here the spec. has ???, so the following is tentative
                            ASSERT(!prototype); // XXX right?
                            FunctionInstance *fObj = validateStaticFunction(&f->function, compileThis, prototype, unchecked, cxt, env);
                            ConstructorMethod *cm = new ConstructorMethod(OBJECT_TO_JS2VAL(fObj));
                            defineLocalMember(env, f->function.name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, cm, p->pos, true);
                        }
                        break;
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
                        const StringAtom *name = vb->name;
                        if (vb->type)
                            ValidateTypeExpression(cxt, env, vb->type);
                        vb->member = NULL;
                        if (vb->initializer)
                            ValidateExpression(cxt, env, vb->initializer);

                        if (!cxt->strict && ((regionalFrame->kind == PackageKind)
                                            || (regionalFrame->kind == ParameterKind))
                                        && !immutable
                                        && (vs->attributes == NULL)
                                        && (vb->type == NULL)) {
                            defineHoistedVar(env, name, p, true, JS2VAL_UNDEFINED);
                        }
                        else {
                            a = Attribute::toCompoundAttribute(attr);
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
                                    vb->mn = defineLocalMember(env, name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
                                    bCon->saveMultiname(vb->mn);
                                }
                                break;
                            case Attribute::Virtual:
                            case Attribute::Final: 
                                {
                                    Multiname *mn = new Multiname(name, a->namespaces);
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
                    a = Attribute::toCompoundAttribute(attr);
                    if (a->dynamic || a->prototype)
                        reportError(Exception::definitionError, "Illegal attribute", p->pos);
                    if ( ! ((a->memberMod == Attribute::NoModifier) || ((a->memberMod == Attribute::Static) && (env->getTopFrame()->kind == ClassKind))) )
                        reportError(Exception::definitionError, "Illegal attribute", p->pos);
                    Variable *v = new Variable(namespaceClass, OBJECT_TO_JS2VAL(new Namespace(&ns->name)), true);
                    defineLocalMember(env, &ns->name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
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
                    a = Attribute::toCompoundAttribute(attr);
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
                    JS2Class *c = new JS2Class(superClass, protoVal, new Namespace(engine->private_StringAtom), (a->dynamic || superClass->dynamic), true, final, engine->allocStringPtr(&classStmt->name));
                    classStmt->c = c;
                    Variable *v = new Variable(classClass, OBJECT_TO_JS2VAL(c), true);
                    defineLocalMember(env, &classStmt->name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos, true);
                    if (classStmt->body) {
                        env->addFrame(c);
                        ValidateStmtList(cxt, env, pl, classStmt->body->statements);
                        ASSERT(env->getTopFrame() == c);
                        env->removeTopFrame();
                    }
                    c->complete = true;
                }
                break;
            case StmtNode::With:
                {
                    UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                    ValidateExpression(cxt, env, w->expr);
                    ValidateStmt(cxt, env, pl, w->stmt);
                }
                break;
            case StmtNode::empty:
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
                BlockStmtNode *b = checked_cast<BlockStmtNode *>(p);
//                BlockFrame *runtimeFrame = new BlockFrame(b->compileFrame);
                env->addFrame(b->compileFrame);    // XXX is this right? shouldn't this be the compile frame until execution occurs?
                bCon->emitOp(ePushFrame, p->pos);
                bCon->addFrame(b->compileFrame);
                StmtNode *bp = b->statements;
                while (bp) {
                    SetupStmt(env, phase, bp);
                    bp = bp->next;
                }
                bCon->emitOp(ePopFrame, p->pos);
                env->removeTopFrame();
            }
            break;
        case StmtNode::label:
            {
                LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                SetupStmt(env, phase, l->stmt);
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
                bCon->addShort(g->blockCount);
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
                
                targetList.push_back(p);
                bCon->emitOp(eForValue, p->pos);

                Reference *v = NULL;
                if (f->initializer->getKind() == StmtNode::Var) {
                    VariableStmtNode *vs = checked_cast<VariableStmtNode *>(f->initializer);
                    VariableBinding *vb = vs->bindings;
                    v = new (*referenceArena) LexicalReference(vb->name, cxt.strict);
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
                SetupStmt(env, phase, f->stmt);
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
                SetupStmt(env, phase, f->stmt);
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
                uint16 swVarIndex = (checked_cast<NonWithFrame *>(env->getTopFrame()))->allocateSlot();
                BytecodeContainer::LabelID defaultLabel = NotALabel;

                Reference *r = SetupExprNode(env, phase, sw->expr, &exprType);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eFrameSlotWrite, p->pos);
                bCon->addShort(swVarIndex);

                // First time through, generate the conditional waterfall 
                StmtNode *s = sw->statements;
                while (s) {
                    if (s->getKind() == StmtNode::Case) {
                        ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                        if (c->expr) {
                            bCon->emitOp(eFrameSlotRead, c->pos);
                            bCon->addShort(swVarIndex);
                            Reference *r = SetupExprNode(env, phase, c->expr, &exprType);
                            if (r) r->emitReadBytecode(bCon, c->pos);
                            bCon->emitOp(eEqual, c->pos);
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

                bCon->setLabel(sw->breakLabelID);
            }
            break;
        case StmtNode::While:
            {
                UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                BytecodeContainer::LabelID loopTop = bCon->getLabel();
                bCon->emitBranch(eBranch, w->continueLabelID, p->pos);
                bCon->setLabel(loopTop);
                SetupStmt(env, phase, w->stmt);
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
                SetupStmt(env, phase, w->stmt);
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
                    ASSERT(bCon->mStackTop == 0);
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
                        Reference *r = new (*referenceArena) LexicalReference(&c->name, false);
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
            }
            break;
        case StmtNode::Function:
            {
                FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
                CompilationData *oldData = NULL;
                try {
                    oldData = startCompilationUnit(f->function.fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
                    env->addFrame(f->function.fWrap->compileFrame);
#ifdef DEBUG
                    bCon->fName = *f->function.name;
#endif
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
                                        v->value = type->implicitCoerce(this, newValue);
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
                                        LexicalReference *lVal = new (*referenceArena) LexicalReference(vb->mn, cxt.strict);
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
                                    LexicalReference *lVal = new (*referenceArena) LexicalReference(vb->mn, cxt.strict);
                                    referenceArena->registerDestructor(lVal);
                                    lVal->emitInitBytecode(bCon, p->pos);      
                                }
                                else {
                                    v->type->emitDefaultValue(bCon, p->pos);
                                    LexicalReference *lVal = new (*referenceArena) LexicalReference(vb->mn, cxt.strict);
                                    referenceArena->registerDestructor(lVal);
                                    lVal->emitInitBytecode(bCon, p->pos);      
                                }
                            }
                        }
                        else {
                            ASSERT(vb->member->memberKind == Member::InstanceVariableMember);
                            InstanceVariable *v = checked_cast<InstanceVariable *>(vb->member);
                            JS2Class *t;
                            if (vb->type)
                                t = EvalTypeExpression(env, CompilePhase, vb->type);
                            else {
                                ASSERT(false);
                                // XXX get type from overriden member?
/*
                                if (vb->osp->first->overriddenMember && (vb->osp->first->overriddenMember != POTENTIAL_CONFLICT))
                                    t = vb->osp->first->overriddenMember->type;
                                else
                                    if (vb->osp->second->overriddenMember && (vb->osp->second->overriddenMember != POTENTIAL_CONFLICT))
                                        t = vb->osp->second->overriddenMember->type;
                                    else
*/
                                        t = objectClass;
                            }
                            v->type = t;
                        }
                    }
                    else { // HoistedVariable
                        if (vb->initializer) {
                            Reference *r = SetupExprNode(env, phase, vb->initializer, &exprType);
                            if (r) r->emitReadBytecode(bCon, p->pos);
                            LexicalReference *lVal = new (*referenceArena) LexicalReference(vb->name, cxt.strict);
                            referenceArena->registerDestructor(lVal);
                            lVal->variableMultiname.addNamespace(publicNamespace);
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
                SetupStmt(env, phase, w->stmt);                        
                bCon->emitOp(eWithout, p->pos);
            }
            break;
        case StmtNode::empty:
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
                case Token::identifier:
                    if (name == world.identifiers["constructor"]) {
                        ca = new CompoundAttribute();
                        ca->memberMod = Attribute::Constructor;
                        return ca;
                    }
                    else
                    if (name == world.identifiers["override"]) {
                        ca = new CompoundAttribute();
                        ca->overrideMod = Attribute::DoOverride;
                        return ca;
                    }
                    else
                    if (name == world.identifiers["virtual"]) {
                        ca = new CompoundAttribute();
                        ca->memberMod = Attribute::Virtual;
                        return ca;
                    }
                    else
                    if (name == world.identifiers["dynamic"]) {
                        ca = new CompoundAttribute();
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
            if (b->attrKind == NamespaceAttr) {
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
            namespaces(NULL), xplicit(false), dynamic(false), memberMod(NoModifier), 
            overrideMod(NoOverride), prototype(false), unused(false) 
    { 
    }

    // Convert an attribute to a compoundAttribute. If the attribute
    // is NULL, return a default compoundAttribute
    CompoundAttribute *Attribute::toCompoundAttribute(Attribute *a)
    { 
        if (a) 
            return a->toCompoundAttribute(); 
        else
            return new CompoundAttribute();
    }

    // Convert a simple namespace to a compoundAttribute with that namespace
    CompoundAttribute *Namespace::toCompoundAttribute()    
    { 
        CompoundAttribute *t = new CompoundAttribute(); 
        t->addNamespace(this); 
        return t; 
    }

    // Convert a 'true' attribute to a default compoundAttribute
    CompoundAttribute *TrueAttribute::toCompoundAttribute()    
    { 
        return new CompoundAttribute(); 
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
        case ExprNode::arrayLiteral:
        case ExprNode::numUnit:
        case ExprNode::string:
        case ExprNode::boolean:
            break;
        case ExprNode::This:
            {
                if (env->findThis(this, true) == JS2VAL_VOID)
                    reportError(Exception::syntaxError, "No 'this' available", p->pos);
            }
            break;
        case ExprNode::objectLiteral:
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
                f->obj = validateStaticFunction(&f->function, JS2VAL_INACCESSIBLE, true, true, cxt, env);
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
            op = eEqual;
            goto boolBinary;
        case ExprNode::notIdentical:
            op = eNotEqual;
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
                bCon->addFloat64(checked_cast<NumberExprNode *>(p)->value, p->pos);
            }
            break;
        case ExprNode::regExp:
            {
                RegExpExprNode *v = checked_cast<RegExpExprNode *>(p);
                js2val args[2];
                args[0] = engine->allocString(v->re);
                args[1] = engine->allocString(&v->flags);
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
                
                returnRef = new (*referenceArena) LexicalReference(&name, ns, cxt.strict);
                referenceArena->registerDestructor(returnRef);
            }
            break;
        case ExprNode::identifier:
            {
                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
                returnRef = new (*referenceArena) LexicalReference(&i->name, cxt.strict);
                referenceArena->registerDestructor(returnRef);
                ((LexicalReference *)returnRef)->variableMultiname.addNamespace(cxt);
                // Try to find this identifier at compile time, we have to stop if we reach
                // a frame that supports dynamic properties - the identifier could be
                // created at runtime without us finding it here.
                // We're looking to find both the type of the reference (to store into exprType)
                // and to see if we can change the reference to a FrameSlot or Slot (for member
                // functions)
                Multiname *multiname = &((LexicalReference *)returnRef)->variableMultiname;
                FrameListIterator fi = env->getBegin();
                bool keepLooking = true;
                while (fi != env->getEnd() && keepLooking) {
                    Frame *fr = *fi;
                    if (fr->kind == WithFrameKind)
                        // XXX unless it's provably not a dynamic object that been with'd??
                        break;
                    NonWithFrame *pf = checked_cast<NonWithFrame *>(fr);
                    switch (pf->kind) {
                    default:
                        keepLooking = false;
                        break;
                    case BlockFrameKind:
                        {
                            LocalMember *m = findLocalMember(pf, multiname, ReadAccess);
                            if (m) {
                                switch (checked_cast<LocalMember *>(m)->memberKind) {
                                case LocalMember::VariableMember:
                                    *exprType = checked_cast<Variable *>(m)->type;
                                    break;
                                case LocalMember::FrameVariableMember:
                                    ASSERT(!checked_cast<FrameVariable *>(m)->packageSlot);
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
                            if (mBase)
                                // XXX *exprType = mBase->...
                                keepLooking = false;
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
                                            ASSERT(checked_cast<FrameVariable *>(m)->packageSlot);
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
#if 0
                    if (*exprType) {
                        MemberDescriptor m2;
                        Multiname multiname(&i->name);
                        if (findLocalMember(*exprType, &multiname, ReadAccess, CompilePhase, &m2)) {
                            if (m2.ns) {
                                QualifiedName qname(m2.ns, multiname.name);
                                InstanceMember *m = findInstanceMember(*exprType, &qname, ReadAccess);
                                if (m->kind == InstanceMember::InstanceVariableKind)
                                    returnRef = new SlotReference(checked_cast<InstanceVariable *>(m)->slotIndex);
                            }
                        }
                    }
#endif
                    if (returnRef == NULL) {
                        returnRef = new (*referenceArena) DotReference(&i->name);
                        referenceArena->registerDestructor(returnRef);
                        checked_cast<DotReference *>(returnRef)->propertyMultiname.addNamespace(cxt);
                    }
                } 
                else {
                    if (b->op2->getKind() == ExprNode::qualify) {
                        Reference *rVal = SetupExprNode(env, phase, b->op2, exprType);
                        ASSERT(rVal && checked_cast<LexicalReference *>(rVal));
                        returnRef = new (*referenceArena) DotReference(&((LexicalReference *)rVal)->variableMultiname);
                        referenceArena->registerDestructor(returnRef);
                        checked_cast<DotReference *>(returnRef)->propertyMultiname.addNamespace(cxt);
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
                bCon->emitOp(eNew, p->pos, -(argCount + 1) + 1);    // pop argCount args, the type/function, and push a result
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
        case ExprNode::functionLiteral:
            {
                FunctionExprNode *f = checked_cast<FunctionExprNode *>(p);
                CompilationData *oldData = startCompilationUnit(f->function.fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
                env->addFrame(f->function.fWrap->compileFrame);
                SetupStmt(env, phase, f->function.body);
                // XXX need to make sure that all paths lead to an exit of some kind
                bCon->emitOp(eReturnVoid, p->pos);
                env->removeTopFrame();
                restoreCompilationUnit(oldData);
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
        FrameListIterator fi = getBegin();
        while (fi != getEnd()) {
            if ((*fi)->kind == ClassKind)
                return checked_cast<JS2Class *>(*fi);
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

    // findThis returns the value of this. If allowPrototypeThis is true, allow this to be defined 
    // by either an instance member of a class or a prototype function. If allowPrototypeThis is 
    // false, allow this to be defined only by an instance member of a class.
    js2val Environment::findThis(JS2Metadata *meta, bool allowPrototypeThis)
    {
        FrameListIterator fi = getBegin();
        while (fi != getEnd()) {
            Frame *pf = *fi;
            if ((pf->kind == ParameterKind)
                    && !JS2VAL_IS_NULL(checked_cast<ParameterFrame *>(pf)->thisObject))
                if (allowPrototypeThis || !checked_cast<ParameterFrame *>(pf)->prototype)
                    return checked_cast<ParameterFrame *>(pf)->thisObject;
            // XXX for ECMA3, when we hit a package (read GlobalObject) return that as the 'this'
            if ((pf->kind == PackageKind) && meta->cxt.E3compatibility)
                return OBJECT_TO_JS2VAL(pf);
            fi++;
        }
        return JS2VAL_VOID;
    }

    // Read the value of a lexical reference - it's an error if that reference
    // doesn't have a binding somewhere.
    // Attempt the read in each frame in the current environment, stopping at the
    // first succesful effort. If the property can't be found in any frame, it's 
    // an error.
    void Environment::lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase, js2val *rval, js2val *base)
    {
        LookupKind lookup(true, findThis(meta, false));
        FrameListIterator fi = getBegin();
        bool result = false;
        while (fi != getEnd()) {
            switch ((*fi)->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(*fi));
                    js2val frame = OBJECT_TO_JS2VAL(*fi);
                    result = limit->read(meta, &frame, limit, multiname, &lookup, phase, rval);
                }
                break;
            case SystemKind:
            case ParameterKind:
            case BlockFrameKind:
                {
                    LocalMember *m = meta->findLocalMember(*fi, multiname, ReadAccess);
                    if (m)
                        result = meta->readLocalMember(m, phase, rval);
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(*fi);
                    // XXX uninitialized 'with' object?
                    js2val withVal = OBJECT_TO_JS2VAL(wf->obj);
                    JS2Class *limit = meta->objectType(withVal);
                    result = limit->read(meta, &withVal, limit, multiname, &lookup, phase, rval);
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
        LookupKind lookup(true, findThis(meta, false));
        FrameListIterator fi = getBegin();
        bool result = false;
        while (fi != getEnd()) {
            switch ((*fi)->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(*fi));
                    result = limit->write(meta, OBJECT_TO_JS2VAL(*fi), limit, multiname, &lookup, false, newValue, false);
                }
                break;
            case SystemKind:
            case ParameterKind:
            case BlockFrameKind:
                {
                    LocalMember *m = meta->findLocalMember(*fi, multiname, WriteAccess);
                    if (m) {
                        meta->writeLocalMember(m, newValue, false);
                        result = true;
                    }
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(*fi);
                    // XXX uninitialized 'with' object?
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(wf->obj));
                    result = limit->write(meta, OBJECT_TO_JS2VAL(wf->obj), limit, multiname, &lookup, false, newValue, false);
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
            result = limit->write(meta, OBJECT_TO_JS2VAL(pkg), limit, multiname, &lookup, true, newValue, false);
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
        LookupKind lookup(true, findThis(meta, false));
        FrameListIterator fi = getBegin();
        bool result = false;
        while (fi != getEnd()) {
            switch ((*fi)->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(*fi));
                    result = limit->write(meta, OBJECT_TO_JS2VAL(*fi), limit, multiname, &lookup, false, newValue, true);
                }
                break;
            case SystemKind:
            case ParameterKind:
            case BlockFrameKind:
                {
                    LocalMember *m = meta->findLocalMember(*fi, multiname, WriteAccess);
                    if (m) {
                        meta->writeLocalMember(m, newValue, true);
                        result = true;
                    }
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(*fi);
                    // XXX uninitialized 'with' object?
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(wf->obj));
                    result = limit->write(meta, OBJECT_TO_JS2VAL(wf->obj), limit, multiname, &lookup, false, newValue, true);
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
        result = limit->write(meta, OBJECT_TO_JS2VAL(pkg), limit, multiname, &lookup, true, newValue, true);
        if (result)
            return;
    }

    // Delete the named property in the current environment, return true if the property
    // can't be found, or the result of the deleteProperty call if it was found.
    bool Environment::lexicalDelete(JS2Metadata *meta, Multiname *multiname, Phase phase)
    {
        LookupKind lookup(true, findThis(meta, false));
        FrameListIterator fi = getBegin();
        bool result = false;
        while (fi != getEnd()) {
            switch ((*fi)->kind) {
            case ClassKind:
            case PackageKind:
                {
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(*fi));
                    if (limit->deleteProperty(meta, OBJECT_TO_JS2VAL(*fi), limit, multiname, &lookup, &result))
                        return result;
                }
                break;
            case SystemKind:
            case ParameterKind:
            case BlockFrameKind:
                {
                    LocalMember *m = meta->findLocalMember(*fi, multiname, WriteAccess);
                    if (m)
                        return false;
                }
                break;
            case WithFrameKind:
                {
                    WithFrame *wf = checked_cast<WithFrame *>(*fi);
                    // XXX uninitialized 'with' object?
                    JS2Class *limit = meta->objectType(OBJECT_TO_JS2VAL(wf->obj));
                    if (limit->deleteProperty(meta, OBJECT_TO_JS2VAL(wf->obj), limit, multiname, &lookup, &result))
                        return result;
                }
                break;
            }
            fi++;
        }
        return true;
    }

    // Clone the pluralFrame bindings into the singularFrame, instantiating new members for each binding
    void Environment::instantiateFrame(NonWithFrame *pluralFrame, NonWithFrame *singularFrame)
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
        if (pluralFrame->slots) {
            size_t count = pluralFrame->slots->size();
            singularFrame->slots = new std::vector<js2val>(count);
            for (size_t i = 0; i < count; i++)
                (*singularFrame->slots)[i] = (*pluralFrame->slots)[i];
        }
    }

    // need to mark all the frames in the environment - otherwise a marked frame that
    // came initially from the bytecodeContainer may prevent the markChildren call
    // from finding frames further down the list.
    void Environment::markChildren()
    { 
        FrameListIterator fi = getBegin();
        while (fi != getEnd()) {
            GCMARKOBJECT(*fi)
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

    QualifiedName Multiname::selectPrimaryName(JS2Metadata *meta)
    {
        if (nsList->size() == 1)
            return QualifiedName(nsList->back(), name);
        else {
            if (listContains(meta->publicNamespace))
                return QualifiedName(meta->publicNamespace, name);
            else {
                meta->reportError(Exception::propertyAccessError, "No good primary name {0}", meta->engine->errorPos(), name);
                return QualifiedName();
            }
        }
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void Multiname::markChildren()
    {
        for (NamespaceListIterator n = nsList->begin(), end = nsList->end(); (n != end); n++) {
            GCMARKOBJECT(*n)
        }
        if (name) JS2Object::mark(name);
    }

    bool Multiname::subsetOf(Multiname &mn)
    {
        if (*name != *mn.name)
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
    Multiname *JS2Metadata::defineLocalMember(Environment *env, const String *id, NamespaceList &namespaces, 
                                                Attribute::OverrideModifier overrideMod, bool xplicit, Access access,
                                                LocalMember *m, size_t pos, bool enumerable)
    {
        NonWithFrame *innerFrame = checked_cast<NonWithFrame *>(*(env->getBegin()));
        if ((overrideMod != Attribute::NoOverride) || (xplicit && innerFrame->kind != PackageKind))
            reportError(Exception::definitionError, "Illegal definition", pos);
        
        Multiname *multiname = new Multiname(id);
        if (namespaces.empty()) 
            multiname->addNamespace(publicNamespace);
        else
            multiname->addNamespace(namespaces);

        // Search the local frame for an overlapping definition
        LocalBindingEntry **lbeP = innerFrame->localBindings[*id];
        if (lbeP) {
            for (LocalBindingEntry::NS_Iterator i = (*lbeP)->begin(), end = (*lbeP)->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding &ns = *i;
                if ((ns.second->accesses & access) && multiname->listContains(ns.first))
                    reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
            }
        }

        // Check all frames below the current - up to the RegionalFrame - for a non-forbidden definition
        FrameListIterator fi = env->getBegin();
        Frame *regionalFrame = *env->getRegionalFrame();
        if (innerFrame != regionalFrame) {
            // The frame iterator is pointing at the top of the environment's
            // frame list, start at the one below that and continue to the frame
            // returned by 'getRegionalFrame()'.
            Frame *fr = *++fi;
            while (true) {
                if (fr->kind != WithFrameKind) {
                    NonWithFrame *nwfr = checked_cast<NonWithFrame *>(fr);
                    LocalBindingEntry **rbeP = nwfr->localBindings[*id];
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
            lbe = new LocalBindingEntry(*id);
            innerFrame->localBindings.insert(*id, lbe);
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
                        LocalBindingEntry **rbeP = nwfr->localBindings[*id];
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
                            LocalBindingEntry *rbe = new LocalBindingEntry(*id);
                            nwfr->localBindings.insert(*id, rbe);
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
            InstanceBindingEntry **ibeP = c->instanceBindings[*qname->name];
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
                Multiname *mn = new Multiname(multiname->name, *nli);
                DEFINE_ROOTKEEPER(rk, mn);
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

    InstanceMember *JS2Metadata::defineInstanceMember(JS2Class *c, Context *cxt, const String *id, NamespaceList &namespaces, 
                                                                    Attribute::OverrideModifier overrideMod, bool xplicit,
                                                                    InstanceMember *m, size_t pos)
    {
        if (xplicit)
            reportError(Exception::definitionError, "Illegal use of explicit", pos);
        Access access = m->instanceMemberAccess();
        Multiname requestedMultiname(id, namespaces);
        Multiname openMultiname(id, cxt);
        Multiname definedMultiname;
        Multiname searchedMultiname;
        if (requestedMultiname.nsList->empty()) {
            definedMultiname = Multiname(id, publicNamespace);
            searchedMultiname = openMultiname;
        }
        else {
            definedMultiname = requestedMultiname;
            searchedMultiname = requestedMultiname;
        }
        InstanceMember *mBase = searchForOverrides(c, &searchedMultiname, access, pos);
        InstanceMember *mOverridden = NULL;
        if (mBase) {
            mOverridden = getDerivedInstanceMember(c, mBase, access);
            definedMultiname = *mOverridden->multiname;
            if (!requestedMultiname.subsetOf(definedMultiname))
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
        InstanceBindingEntry **ibeP = c->instanceBindings[*id];
        if (ibeP) {
            for (InstanceBindingEntry::NS_Iterator i = (*ibeP)->begin(), end = (*ibeP)->end(); (i != end); i++) {
                InstanceBindingEntry::NamespaceBinding &ns = *i;
                if ((access & ns.second->content->instanceMemberAccess()) && (definedMultiname.listContains(ns.first)))
                    reportError(Exception::definitionError, "Illegal override", pos);
            }
        }
        switch (overrideMod) {
        case Attribute::NoOverride:
            if (mBase || searchForOverrides(c, &openMultiname, access, pos))
                reportError(Exception::definitionError, "Illegal override", pos);
            break;
        case Attribute::DoOverride: 
            if (mBase)
                reportError(Exception::definitionError, "Illegal override", pos);
            break;
        case Attribute::DontOverride:
            if (mBase == NULL)
                reportError(Exception::definitionError, "Illegal override", pos);
            break;
        }
        m->multiname = new Multiname(definedMultiname);
        InstanceBindingEntry *ibe;
        if (ibeP == NULL) {
            ibe = new InstanceBindingEntry(*id);
            c->instanceBindings.insert(*id, ibe);
        }
        else
            ibe = *ibeP;
        for (NamespaceListIterator nli = definedMultiname.nsList->begin(), nlend = definedMultiname.nsList->end(); (nli != nlend); nli++) {
            // XXX here and in defineLocal... why a new binding for each namespace?
            // (other than it would mess up the destructor sequence :-)
            InstanceBinding *ib = new InstanceBinding(access, m);
            ibe->bindingList.push_back(InstanceBindingEntry::NamespaceBinding(*nli, ib));
        }
        return mOverridden;
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
    
    LocalMember *JS2Metadata::defineHoistedVar(Environment *env, const String *id, StmtNode *p, bool isVar, js2val initVal)
    {
        LocalMember *result = NULL;
        FrameListIterator regionalFrameEnd = env->getRegionalEnvironment();
        NonWithFrame *regionalFrame = checked_cast<NonWithFrame *>(*regionalFrameEnd);
        ASSERT((regionalFrame->kind == PackageKind) || (regionalFrame->kind == ParameterKind));          
rescan:
        // run through all the existing bindings, to see if this variable already exists.
        LocalBinding *bindingResult = NULL;
        bool foundMultiple = false;
        LocalBindingEntry **lbeP = regionalFrame->localBindings[*id];
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
                && (regionalFrame->kind == ParameterKind)
                && (regionalFrameEnd != env->getBegin())) {
            // re-scan in the frame above the parameter frame
            regionalFrame = checked_cast<NonWithFrame *>(*(regionalFrameEnd - 1));
            goto rescan;
        }
        
        if (bindingResult == NULL) {
            LocalBindingEntry *lbe;
            if (lbeP == NULL) {
                lbe = new LocalBindingEntry(*id);
                regionalFrame->localBindings.insert(*id, lbe);
            }
            else
                lbe = *lbeP;
            result = new FrameVariable(regionalFrame->allocateSlot(), (regionalFrame->kind == PackageKind));
            (*regionalFrame->slots)[checked_cast<FrameVariable *>(result)->frameSlot] = initVal;
            LocalBinding *sb = new LocalBinding(ReadWriteAccess, result, true);
            lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(publicNamespace, sb));
        }
        else {
            if (foundMultiple)
                reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
            else {
                if ((bindingResult->accesses != ReadWriteAccess)
                        || ((bindingResult->content->memberKind != LocalMember::DynamicVariableMember)
                              && (bindingResult->content->memberKind != LocalMember::FrameVariableMember)))
                    reportError(Exception::definitionError, "Illegal redefinition of {0}", p->pos, id);
                else {
                    result = bindingResult->content;
                    writeLocalMember(result, initVal, true);
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
//            meta->engine->localFrame = meta->engine->activationStackTop[-1].localFrame;
            js2val result = meta->readEvalString(*meta->toString(argv[0]), widenCString("Eval Source"));
            meta->engine->rts();
            return result;
        }
        else
            return JS2VAL_UNDEFINED;
    }

#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (uint32)(isdigit(c) ? (c) - '0' : 10 + tolower(c) - 'a')

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
        
        return STRING_TO_JS2VAL(meta->engine->allocStringPtr(&meta->world.identifiers[newchars]));
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

        return STRING_TO_JS2VAL(meta->engine->allocStringPtr(&meta->world.identifiers[newchars]));
    }

    static js2val GlobalObject_parseInt(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        const String *str = meta->toString(argv[0]);
        const char16 *chars = str->data();
        uint32 length = str->length();
        const char16 *numEnd;
        uint base = 0;
        
        if (argc > 1) {
            float64 d = meta->toFloat64(argv[1]);
            if (!JSDOUBLE_IS_FINITE(d) || ((base = (int32)d) != d))
                return meta->engine->nanValue;
            if (base == 0)
                base = 10;
            else
                if ((base < 2) || (base > 36))
                    return meta->engine->nanValue;
        }
        
        return meta->engine->allocNumber(stringToInteger(chars, chars + length, numEnd, base));
    }

    static js2val GlobalObject_parseFloat(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 argc)
    {
        const String *str = meta->toString(argv[0]);
        return meta->engine->allocNumber(meta->convertStringToDouble(str));
    }

    void JS2Metadata::addGlobalObjectFunction(char *name, NativeCode *code, uint32 length)
    {
        SimpleInstance *fInst = new SimpleInstance(this, functionClass->prototype, functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_VOID, true), code, env);
        createDynamicProperty(glob, &world.identifiers[name], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, true);
        createDynamicProperty(fInst, engine->length_StringAtom, INT_TO_JS2VAL(length), ReadAccess, true, false);
    }

    static js2val Object_Constructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {
        if (argc) {
            if (JS2VAL_IS_OBJECT(argv[0]))
                return argv[0];     // XXX special handling for host objects?
            else
                return meta->toObject(argv[0]);
        }
        else
            return OBJECT_TO_JS2VAL(new SimpleInstance(meta, meta->objectClass->prototype, meta->objectClass));
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
            JS2Class *type = (checked_cast<SimpleInstance *>(obj))->type;
            String s = "[object " + *type->getName() + "]";
            return STRING_TO_JS2VAL(meta->engine->allocString(s));
        }
    }
    
    static js2val Object_valueOf(JS2Metadata *meta, const js2val thisValue, js2val /* argv */ [], uint32 /* argc */)
    {
        return thisValue;
    }

bool nullClass_ReadProperty(JS2Metadata *meta, js2val *base, JS2Class *limit, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval) { return false; }
bool nullClass_ReadPublicProperty(JS2Metadata *meta, js2val *base, JS2Class *limit, const String *name, Phase phase, js2val *rval) { return false; }
bool nullClass_BracketRead(JS2Metadata *meta, js2val *base, JS2Class *limit, Multiname *multiname, Phase phase, js2val *rval) { return false; }
bool nullClass_arrayWriteProperty(JS2Metadata *meta, js2val base, JS2Class *limit, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue) { return false; }
bool nullClass_WriteProperty(JS2Metadata *meta, js2val base, JS2Class *limit, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue, bool initFlag) { return false; }
bool nullClass_WritePublicProperty(JS2Metadata *meta, js2val base, JS2Class *limit, const String *name, bool createIfMissing, js2val newValue) { return false; }
bool nullClass_BracketWrite(JS2Metadata *meta, js2val base, JS2Class *limit, Multiname *multiname, js2val newValue) { return false; }
bool nullClass_DeleteProperty(JS2Metadata *meta, js2val base, JS2Class *limit, Multiname *multiname, LookupKind *lookupKind, bool *result) { return false; }
bool nullClass_DeletePublic(JS2Metadata *meta, js2val base, JS2Class *limit, const String *name, bool *result) { return false; }
bool nullClass_BracketDelete(JS2Metadata *meta, js2val base, JS2Class *limit, Multiname *multiname, bool *result) { return false; }
    
#define MAKEBUILTINCLASS(c, super, dynamic, allowNull, final, name, defaultVal) c = new JS2Class(super, NULL, new Namespace(engine->private_StringAtom), dynamic, allowNull, final, name); c->complete = true; c->defaultValue = defaultVal;

    JS2Metadata::JS2Metadata(World &world) : JS2Object(MetaDataKind),
        world(world),
        engine(new JS2Engine(world)),
        publicNamespace(new Namespace(engine->public_StringAtom)),
        bCon(new BytecodeContainer()),
        glob(new Package(new Namespace(&world.identifiers["internal"]))),
        env(new Environment(new MetaData::SystemFrame(), glob)),
        flags(JS1),
        showTrees(false),
        referenceArena(NULL)
    {
        engine->meta = this;

        cxt.openNamespaces.clear();
        cxt.openNamespaces.push_back(publicNamespace);

        MAKEBUILTINCLASS(objectClass, NULL, false, true, false, engine->Object_StringAtom, JS2VAL_VOID);
        MAKEBUILTINCLASS(undefinedClass, objectClass, false, false, true, engine->undefined_StringAtom, JS2VAL_VOID);
        MAKEBUILTINCLASS(nullClass, objectClass, false, true, true, engine->null_StringAtom, JS2VAL_NULL);
        nullClass->read = nullClass_ReadProperty;
        nullClass->readPublic = nullClass_ReadPublicProperty;
        nullClass->write = nullClass_WriteProperty;
        nullClass->writePublic = nullClass_WritePublicProperty;
        nullClass->deleteProperty = nullClass_DeleteProperty;
        nullClass->deletePublic = nullClass_DeletePublic;
        nullClass->bracketRead = nullClass_BracketRead;
        nullClass->bracketWrite = nullClass_BracketWrite;
        nullClass->bracketDelete = nullClass_BracketDelete;

        MAKEBUILTINCLASS(booleanClass, objectClass, false, false, true, engine->allocStringPtr(&world.identifiers["Boolean"]), JS2VAL_FALSE);
        MAKEBUILTINCLASS(generalNumberClass, objectClass, false, false, false, engine->allocStringPtr(&world.identifiers["general number"]), engine->nanValue);
        MAKEBUILTINCLASS(numberClass, generalNumberClass, false, false, true, engine->allocStringPtr(&world.identifiers["Number"]), engine->nanValue);
        MAKEBUILTINCLASS(characterClass, objectClass, false, false, true, engine->allocStringPtr(&world.identifiers["Character"]), JS2VAL_ZERO);
        MAKEBUILTINCLASS(stringClass, objectClass, false, false, true, engine->allocStringPtr(&world.identifiers["String"]), JS2VAL_NULL);
        MAKEBUILTINCLASS(namespaceClass, objectClass, false, true, true, engine->allocStringPtr(&world.identifiers["namespace"]), JS2VAL_NULL);
        MAKEBUILTINCLASS(attributeClass, objectClass, false, true, true, engine->allocStringPtr(&world.identifiers["attribute"]), JS2VAL_NULL);
        MAKEBUILTINCLASS(classClass, objectClass, false, true, true, engine->allocStringPtr(&world.identifiers["Class"]), JS2VAL_NULL);
        MAKEBUILTINCLASS(functionClass, objectClass, true, true, true, engine->Function_StringAtom, JS2VAL_NULL);
        MAKEBUILTINCLASS(packageClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["Package"]), JS2VAL_NULL);

        


        // A 'forbidden' member, used to mark hidden bindings
        forbiddenMember = new LocalMember(Member::ForbiddenMember, true);

        // needed for class instance variables etc...
        NamespaceList publicNamespaceList;
        publicNamespaceList.push_back(publicNamespace);
        Variable *v;

        
        
// XXX Built-in Attributes... XXX 
/*
XXX see EvalAttributeExpression, where identifiers are being handled for now...

        CompoundAttribute *attr = new CompoundAttribute();
        attr->dynamic = true;
        v = new Variable(attributeClass, OBJECT_TO_JS2VAL(attr), true);
        defineLocalMember(env, &world.identifiers["dynamic"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
*/

/*** ECMA 3  Global Object ***/
        // Non-function properties of the global object : 'undefined', 'NaN' and 'Infinity'
        createDynamicProperty(glob, engine->undefined_StringAtom, JS2VAL_UNDEFINED, ReadAccess, true, false);
        createDynamicProperty(glob, &world.identifiers["NaN"], engine->nanValue, ReadAccess, true, false);
        createDynamicProperty(glob, &world.identifiers["Infinity"], engine->posInfValue, ReadAccess, true, false);
        // XXX add 'version()' 
        createDynamicProperty(glob, &world.identifiers["version"], INT_TO_JS2VAL(0), ReadAccess, true, false);
        // Function properties of the global object 
        addGlobalObjectFunction("isNaN", GlobalObject_isNaN, 1);
        addGlobalObjectFunction("eval", GlobalObject_eval, 1);
        addGlobalObjectFunction("toString", GlobalObject_toString, 1);
        addGlobalObjectFunction("unescape", GlobalObject_unescape, 1);
        addGlobalObjectFunction("escape", GlobalObject_escape, 1);
        addGlobalObjectFunction("parseInt", GlobalObject_parseInt, 2);
        addGlobalObjectFunction("parseFloat", GlobalObject_parseFloat, 1);


/*** ECMA 3  Object Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(objectClass), true);
        defineLocalMember(env, &world.identifiers["Object"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        // Function properties of the Object prototype object
        objectClass->prototype = OBJECT_TO_JS2VAL(new SimpleInstance(this, NULL, objectClass));
        objectClass->construct = Object_Constructor;
        // Adding "prototype" as a static member of the class - not a dynamic property
        env->addFrame(objectClass);
            v = new Variable(objectClass, OBJECT_TO_JS2VAL(objectClass->prototype), true);
            defineLocalMember(env, engine->prototype_StringAtom, publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
        env->removeTopFrame();

/*** ECMA 3  Function Class ***/
// Need this initialized early, as subsequent FunctionInstances need the Function.prototype value
        v = new Variable(classClass, OBJECT_TO_JS2VAL(functionClass), true);
        defineLocalMember(env, &world.identifiers["Function"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initFunctionObject(this);

// Adding 'toString' to the Object.prototype XXX Or make this a static class member?
        FunctionInstance *fInst = new FunctionInstance(this, functionClass->prototype, functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_VOID, true), Object_toString, env);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), engine->toString_StringAtom, OBJECT_TO_JS2VAL(fInst), ReadAccess, true, false);
        createDynamicProperty(fInst, engine->length_StringAtom, INT_TO_JS2VAL(0), ReadAccess, true, false);
        // and 'valueOf'
        fInst = new FunctionInstance(this, functionClass->prototype, functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_VOID, true), Object_valueOf, env);
        createDynamicProperty(JS2VAL_TO_OBJECT(objectClass->prototype), engine->valueOf_StringAtom, OBJECT_TO_JS2VAL(fInst), ReadAccess, true, false);
        createDynamicProperty(fInst, engine->length_StringAtom, INT_TO_JS2VAL(0), ReadAccess, true, false);


/*** ECMA 3  Date Class ***/
        MAKEBUILTINCLASS(dateClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["Date"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(dateClass), true);
        defineLocalMember(env, &world.identifiers["Date"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initDateObject(this);

/*** ECMA 3  RegExp Class ***/
        MAKEBUILTINCLASS(regexpClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["RegExp"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(regexpClass), true);
        defineLocalMember(env, &world.identifiers["RegExp"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initRegExpObject(this);

/*** ECMA 3  String Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(stringClass), true);
        defineLocalMember(env, &world.identifiers["String"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initStringObject(this);

/*** ECMA 3  Number Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(numberClass), true);
        defineLocalMember(env, &world.identifiers["Number"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initNumberObject(this);

/*** ECMA 3  Boolean Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(booleanClass), true);
        defineLocalMember(env, &world.identifiers["Boolean"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initBooleanObject(this);

/*** ECMA 3  Math Class ***/
        MAKEBUILTINCLASS(mathClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["Math"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(mathClass), true);
        defineLocalMember(env, &world.identifiers["Math"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initMathObject(this);

/*** ECMA 3  Array Class ***/
        MAKEBUILTINCLASS(arrayClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["Array"]), JS2VAL_NULL);
        arrayClass->write = arrayWriteProperty;
        arrayClass->writePublic = arrayWritePublic;
        v = new Variable(classClass, OBJECT_TO_JS2VAL(arrayClass), true);
        defineLocalMember(env, &world.identifiers["Array"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initArrayObject(this);

/*** ECMA 3  Error Classes ***/
        MAKEBUILTINCLASS(errorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["Error"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(errorClass), true);
        defineLocalMember(env, &world.identifiers["Error"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(evalErrorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["EvalError"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(evalErrorClass), true);
        defineLocalMember(env, &world.identifiers["EvalError"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(rangeErrorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["RangeError"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(rangeErrorClass), true);
        defineLocalMember(env, &world.identifiers["RangeError"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(referenceErrorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["ReferenceError"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(referenceErrorClass), true);
        defineLocalMember(env, &world.identifiers["ReferenceError"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(syntaxErrorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["SyntaxError"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(syntaxErrorClass), true);
        defineLocalMember(env, &world.identifiers["SyntaxError"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(typeErrorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["TypeError"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(typeErrorClass), true);
        defineLocalMember(env, &world.identifiers["TypeError"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        MAKEBUILTINCLASS(uriErrorClass, objectClass, true, true, true, engine->allocStringPtr(&world.identifiers["UriError"]), JS2VAL_NULL);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(uriErrorClass), true);
        defineLocalMember(env, &world.identifiers["UriError"], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, true);
        initErrorObject(this);

    }

    JS2Metadata::~JS2Metadata()
    {
        bConList.clear();
        targetList.clear();
        JS2Object::clear(this); // don't blow off the contents of 'this' as the destructors for
                                // embedded objects will get messed up (as they run on exit).
        delete engine;
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

        case MethodClosureKind:
            return functionClass;

        case SystemKind:
        case ParameterKind: 
        case BlockFrameKind: 
        default:
            ASSERT(false);
            return NULL;
        }
    }

    // objectType(o) returns an OBJECT o's most specific type.
    JS2Class *JS2Metadata::objectType(js2val objVal)
    {
        if (JS2VAL_IS_VOID(objVal))
            return undefinedClass;
        if (JS2VAL_IS_NULL(objVal))
            return nullClass;
        if (JS2VAL_IS_BOOLEAN(objVal))
            return booleanClass;
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
        ASSERT(JS2VAL_IS_OBJECT(objVal));
        return objectType(JS2VAL_TO_OBJECT(objVal));
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
        InstanceBindingEntry **ibeP = limit->instanceBindings[*multiname->name];
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
        InstanceBindingEntry **ibeP = c->instanceBindings[*mBase->multiname->name];
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

    LocalMember *JS2Metadata::findLocalMember(JS2Object *container, Multiname *multiname, Access access)
    {
        LocalMember *found = NULL;
        LocalBindingMap *lMap;
        if (container->kind == SimpleInstanceKind)
            lMap = &checked_cast<SimpleInstance *>(container)->localBindings;
        else
            lMap = &checked_cast<NonWithFrame *>(container)->localBindings;
        
        LocalBindingEntry **lbeP = (*lMap)[*multiname->name];
        if (lbeP) {
            for (LocalBindingEntry::NS_Iterator i = (*lbeP)->begin(), end = (*lbeP)->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding &ns = *i;
                if ((ns.second->accesses & access) && multiname->listContains(ns.first)) {
                    if (found && (ns.second->content != found))
                        reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                    else {
                        found = ns.second->content;
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
        Member *m = NULL;
        if (JS2VAL_IS_PRIMITIVE(*base) && cxt.E3compatibility) {
            *base = toObject(*base);      // XXX E3 compatibility...
        }
        JS2Object *baseObj = JS2VAL_TO_OBJECT(*base);
        switch (baseObj->kind) {
        case SimpleInstanceKind:
            m = findLocalMember(baseObj, multiname, access);
            break;
        case PackageKind:
            m = findLocalMember(baseObj, multiname, access);
            break;
        case ClassKind:
            m = findLocalMember(baseObj, multiname, access);
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
                s->value = mv->type->implicitCoerce(this, newValue);
                return true;
            }
        default:
            ASSERT(false);
        }
        return false;
    }

    bool JS2Metadata::readLocalMember(LocalMember *m, Phase phase, js2val *rval)
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
                if (phase == CompilePhase) 
                    reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
                FrameVariable *f = checked_cast<FrameVariable *>(m);
                if (f->packageSlot)
                    *rval = (*env->getPackageFrame()->slots)[f->frameSlot];
                else {
                    FrameListIterator fi = env->getRegionalFrame();
                    ASSERT((*fi)->kind == ParameterKind);
                    *rval = (*checked_cast<NonWithFrame *>(*(fi - 1))->slots)[f->frameSlot];
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
        case LocalMember::SetterMember:
            break;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    // Write a value to the local member
    bool JS2Metadata::writeLocalMember(LocalMember *m, js2val newValue, bool initFlag) // phase not used?
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
                v->value = v->type->implicitCoerce(this, newValue);
            }
            return true;
        case LocalMember::FrameVariableMember:
            {
                FrameVariable *f = checked_cast<FrameVariable *>(m);
                if (f->packageSlot)
                    (*env->getPackageFrame()->slots)[f->frameSlot] = newValue;
                else {
                    FrameListIterator fi = env->getRegionalFrame();
                    ASSERT((*fi)->kind == ParameterKind);
                    (*checked_cast<NonWithFrame *>(*(fi - 1))->slots)[f->frameSlot] = newValue;
                }
            }
            return true;
        case LocalMember::DynamicVariableMember:
            (checked_cast<DynamicVariable *>(m))->value = newValue;
            return true;
        case LocalMember::GetterMember:
        case LocalMember::SetterMember:
            break;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    bool JS2Metadata::hasOwnProperty(JS2Object *obj, const String *name)
    {
        Multiname *mn = new Multiname(name, publicNamespace);
        DEFINE_ROOTKEEPER(rk, mn);
        js2val val = OBJECT_TO_JS2VAL(obj);
        return (findCommonMember(&val, mn, ReadWriteAccess, true) != NULL);
    }

    void JS2Metadata::createDynamicProperty(JS2Object *obj, const String *name, js2val initVal, Access access, bool sealed, bool enumerable) 
    {
        DEFINE_ROOTKEEPER(rk, name);
        QualifiedName qName(publicNamespace, name); 
        createDynamicProperty(obj, &qName, initVal, access, sealed, enumerable); 
    }

    // The caller must make sure that the created property does not already exist and does not conflict with any other property.
    void JS2Metadata::createDynamicProperty(JS2Object *obj, QualifiedName *qName, js2val initVal, Access access, bool sealed, bool enumerable)
    {
        DynamicVariable *dv = new DynamicVariable(initVal, sealed);
        LocalBinding *new_b = new LocalBinding(access, dv, enumerable);
        LocalBindingMap *lMap;
        if (obj->kind == SimpleInstanceKind)
            lMap = &checked_cast<SimpleInstance *>(obj)->localBindings;
        else
            if (obj->kind == PackageKind)
                lMap = &checked_cast<Package *>(obj)->localBindings;
            else
                ASSERT(false);

        LocalBindingEntry **lbeP = (*lMap)[*qName->name];
        LocalBindingEntry *lbe;
        if (lbeP == NULL) {
            lbe = new LocalBindingEntry(*qName->name);
            lMap->insert(*qName->name, lbe);
        }
        else
            lbe = *lbeP;
        lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(qName->nameSpace, new_b));
    }

    // Use the slotIndex from the instanceVariable to access the slot
    Slot *JS2Metadata::findSlot(js2val thisObjVal, InstanceVariable *id)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisObjVal) 
                    && (JS2VAL_TO_OBJECT(thisObjVal)->kind == SimpleInstanceKind));
        JS2Object *thisObj = JS2VAL_TO_OBJECT(thisObjVal);
        return &checked_cast<SimpleInstance *>(thisObj)->slots[id->slotIndex];
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
        GCMARKOBJECT(numberClass);
        GCMARKOBJECT(characterClass);
        GCMARKOBJECT(stringClass);
        GCMARKOBJECT(namespaceClass);
        GCMARKOBJECT(attributeClass);
        GCMARKOBJECT(classClass);
        GCMARKOBJECT(functionClass);
        GCMARKOBJECT(packageClass);
        GCMARKOBJECT(dateClass);
        GCMARKOBJECT(regexpClass);
        GCMARKOBJECT(mathClass);
        GCMARKOBJECT(arrayClass);
        GCMARKOBJECT(errorClass);
        GCMARKOBJECT(evalErrorClass);
        GCMARKOBJECT(rangeErrorClass);
        GCMARKOBJECT(referenceErrorClass);
        GCMARKOBJECT(syntaxErrorClass);
        GCMARKOBJECT(typeErrorClass);
        GCMARKOBJECT(uriErrorClass);

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

 
    void JS2Metadata::initBuiltinClass(JS2Class *builtinClass, FunctionData *protoFunctions, FunctionData *staticFunctions, NativeCode *construct, NativeCode *call)
    {
        FunctionData *pf;

        builtinClass->construct = construct;
        builtinClass->call = call;

        NamespaceList publicNamespaceList;
        publicNamespaceList.push_back(publicNamespace);
    
        // Adding "prototype" & "length", etc as static members of the class - not dynamic properties; XXX
        env->addFrame(builtinClass);
        {
            Variable *v = new Variable(builtinClass, OBJECT_TO_JS2VAL(builtinClass->prototype), true);
            defineLocalMember(env, engine->prototype_StringAtom, publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
            v = new Variable(builtinClass, INT_TO_JS2VAL(1), true);
            defineLocalMember(env, engine->length_StringAtom, publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);

            pf = staticFunctions;
            if (pf) {
                while (pf->name) {
                    SimpleInstance *callInst = new SimpleInstance(this, functionClass->prototype, functionClass);
                    callInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code, env);
                    v = new Variable(functionClass, OBJECT_TO_JS2VAL(callInst), true);
                    defineLocalMember(env, &world.identifiers[pf->name], publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0, false);
                    createDynamicProperty(callInst, engine->length_StringAtom, INT_TO_JS2VAL(pf->length), ReadAccess, true, false);
                    pf++;
                }
            }
        }
        env->removeTopFrame();
    
        // Add "constructor" as a dynamic property of the prototype
        FunctionInstance *fInst = new FunctionInstance(this, functionClass->prototype, functionClass);
        createDynamicProperty(fInst, engine->length_StringAtom, INT_TO_JS2VAL(1), ReadAccess, true, false);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), construct, env);
        ASSERT(JS2VAL_IS_OBJECT(builtinClass->prototype));
        createDynamicProperty(JS2VAL_TO_OBJECT(builtinClass->prototype), &world.identifiers["constructor"], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
    
        pf = protoFunctions;
        if (pf) {
            while (pf->name) {
/*
                SimpleInstance *callInst = new SimpleInstance(this, functionClass->prototype, functionClass);
                callInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code, env);
                Multiname *mn = new Multiname(&world.identifiers[pf->name], publicNamespaceList);
                InstanceMember *m = new InstanceMethod(mn, callInst, true, false);
                defineInstanceMember(builtinClass, &cxt, mn->name, *mn->nsList, Attribute::NoOverride, false, m, 0);
*/
                FunctionInstance *fInst = new FunctionInstance(this, functionClass->prototype, functionClass);
                fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_INACCESSIBLE, true), pf->code, env);
                createDynamicProperty(JS2VAL_TO_OBJECT(builtinClass->prototype), &world.identifiers[pf->name], OBJECT_TO_JS2VAL(fInst), ReadWriteAccess, false, false);
                createDynamicProperty(fInst, engine->length_StringAtom, INT_TO_JS2VAL(pf->length), ReadAccess, true, false);
                pf++;
            }
        }
    }

   
    
 /************************************************************************************
 *
 *  JS2Class
 *
 ************************************************************************************/

    JS2Class::JS2Class(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool allowNull, bool final, const String *name) 
        : NonWithFrame(ClassKind), 
            super(super), 
            instanceInitOrder(NULL), 
            complete(false), 
            prototype(proto), 
            typeofString(name),
            privateNamespace(privateNamespace), 
            dynamic(dynamic),
            final(final),
            defaultValue(JS2VAL_NULL),
            call(NULL),
            construct(JS2Engine::defaultConstructor),
            read(defaultReadProperty),
            readPublic(defaultReadPublicProperty),
            write(defaultWriteProperty),
            writePublic(defaultWritePublicProperty),
            deleteProperty(defaultDeleteProperty),
            deletePublic(defaultDeletePublic),
            bracketRead(defaultBracketRead),
            bracketWrite(defaultBracketWrite),
            bracketDelete(defaultBracketDelete),
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
        if (typeofString) JS2Object::mark(typeofString);
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

    js2val JS2Class::implicitCoerce(JS2Metadata *meta, js2val newValue)
    {
        if (JS2VAL_IS_NULL(newValue) || meta->objectType(newValue)->isAncestor(this) )
            return newValue;
        meta->reportError(Exception::badValueError, "Illegal coercion", meta->engine->errorPos());
        return JS2VAL_VOID;
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

    
    // Construct a Simple instance of a class. Set the
    // initial value of all slots to uninitialized.
    SimpleInstance::SimpleInstance(JS2Metadata *meta, js2val parent, JS2Class *type) 
        : JS2Object(SimpleInstanceKind),
            sealed(false),
            super(parent),
            type(type), 
            slots(new Slot[type->slotCount]),
            fWrap(NULL)
    {
        for (uint32 i = 0; i < type->slotCount; i++) {
            slots[i].value = JS2VAL_UNINITIALIZED;
        }
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void SimpleInstance::markChildren()
    {
        GCMARKOBJECT(type)
        GCMARKVALUE(super);
        if (fWrap) {
            GCMARKOBJECT(fWrap->compileFrame);
            GCMARKOBJECT(fWrap->env);
            if (fWrap->bCon)
                fWrap->bCon->mark();
        }
        if (slots) {
            ASSERT(type);
            for (uint32 i = 0; (i < type->slotCount); i++) {
                GCMARKVALUE(slots[i].value);
            }
        }
        for (LocalBindingIterator bi = localBindings.begin(), bend = localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                if (ns.first->name) JS2Object::mark(ns.first->name);
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
        delete [] slots;
        if (fWrap)
            delete fWrap;
    }

 /************************************************************************************
 *
 *  AlienInstance
 *
 ************************************************************************************/

    bool AlienInstance::readProperty(Multiname *m, js2val *rval)
    {
        return false;
    }

    void AlienInstance::writeProperty(Multiname *m, js2val rval)
    {
    }

 /************************************************************************************
 *
 *  Getter
 *
 ************************************************************************************/

    void Getter::mark()
    {
        GCMARKOBJECT(type); 
    }

 /************************************************************************************
 *
 *  Setter
 *
 ************************************************************************************/

    void Setter::mark()
    {
        GCMARKOBJECT(type); 
    }

 /************************************************************************************
 *
 *  ArrayInstance
 *
 ************************************************************************************/

    ArrayInstance::ArrayInstance(JS2Metadata *meta, js2val parent, JS2Class *type) 
        : SimpleInstance(meta, parent, type) 
    {
        JS2Object *result = this;
        DEFINE_ROOTKEEPER(rk1, result);

        meta->createDynamicProperty(this, meta->engine->length_StringAtom, INT_TO_JS2VAL(0), ReadWriteAccess, true, false);
    }

 /************************************************************************************
 *
 *  FunctionInstance
 *
 ************************************************************************************/

    FunctionInstance::FunctionInstance(JS2Metadata *meta, js2val parent, JS2Class *type)
     : SimpleInstance(meta, parent, type) 
    {
        // Add prototype property
        JS2Object *result = this;
        DEFINE_ROOTKEEPER(rk1, result);

        JS2Object *obj = new SimpleInstance(meta, meta->objectClass->prototype, meta->objectClass);
        DEFINE_ROOTKEEPER(rk2, obj);

        meta->createDynamicProperty(this, meta->engine->prototype_StringAtom, OBJECT_TO_JS2VAL(obj), ReadWriteAccess, false, true);
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
    }


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
 *  MethodClosure
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void MethodClosure::markChildren()     
    { 
        GCMARKVALUE(thisObject);
        GCMARKOBJECT(method->fInst)
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
        if (slots == NULL)
            slots = new std::vector<js2val>;
        uint16 result = (uint16)(slots->size());
        slots->push_back(JS2VAL_VOID);
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
        if (slots)
            delete slots;
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
        if (slots) {
            for (std::vector<js2val>::iterator i = slots->begin(), end = slots->end(); (i != end); i++)
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
        env->instantiateFrame(pluralFrame, this);
    }

    // Assume that instantiate has been called, the plural frame will contain
    // the cloned Variables assigned into this (singular) frame. Use the 
    // incoming values to initialize the positionals.
    // Pad out to 'length' args with undefined values if argCount is insufficient
    void ParameterFrame::assignArguments(JS2Metadata *meta, JS2Object *fnObj, js2val *argBase, uint32 argCount, uint32 length)
    {
        Multiname *mn = new Multiname(NULL, meta->publicNamespace);
        DEFINE_ROOTKEEPER(rk1, mn);

        ASSERT(pluralFrame->kind == ParameterKind);
        ParameterFrame *plural = checked_cast<ParameterFrame *>(pluralFrame);
        ASSERT((plural->positionalCount == 0) || (plural->positional != NULL));
        
        SimpleInstance *argsObj = new SimpleInstance(meta, meta->objectClass->prototype, meta->objectClass);
        DEFINE_ROOTKEEPER(rk2, argsObj);

        // Add the 'arguments' property
        String name(widenCString("arguments"));
        ASSERT(localBindings[name] == NULL);
        LocalBindingEntry *lbe = new LocalBindingEntry(name);
        LocalBinding *sb = new LocalBinding(ReadAccess, new Variable(meta->arrayClass, OBJECT_TO_JS2VAL(argsObj), true), false);
        lbe->bindingList.push_back(LocalBindingEntry::NamespaceBinding(meta->publicNamespace, sb));
        localBindings.insert(name, lbe);

        uint32 i;
        for (i = 0; (i < argCount); i++) {
            if (i < plural->positionalCount) {
                ASSERT(plural->positional[i]->cloneContent);
                ASSERT(plural->positional[i]->cloneContent->memberKind == Member::VariableMember);
                (checked_cast<Variable *>(plural->positional[i]->cloneContent))->value = argBase[i];
            }
            meta->objectClass->writePublic(meta, OBJECT_TO_JS2VAL(argsObj), meta->objectClass, meta->engine->numberToString(i), true, argBase[i]);
        }
        while (i++ < length) {
            if (i < plural->positionalCount) {
                ASSERT(plural->positional[i]->cloneContent);
                ASSERT(plural->positional[i]->cloneContent->memberKind == Member::VariableMember);
                (checked_cast<Variable *>(plural->positional[i]->cloneContent))->value = JS2VAL_UNDEFINED;
            }
            meta->objectClass->writePublic(meta, OBJECT_TO_JS2VAL(argsObj), meta->objectClass, meta->engine->numberToString(i), true, JS2VAL_UNDEFINED);
        }
        setLength(meta, argsObj, argCount);
        meta->objectClass->writePublic(meta, OBJECT_TO_JS2VAL(argsObj), meta->objectClass, meta->engine->allocStringPtr("callee"), true, OBJECT_TO_JS2VAL(fnObj));
    }


    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void ParameterFrame::markChildren()
    {
        NonWithFrame::markChildren();
        GCMARKVALUE(thisObject);
    }

    ParameterFrame::~ParameterFrame()
    {
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
            env->instantiateFrame(pluralFrame, this);
    }


 /************************************************************************************
 *
 *  InstanceMember
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void InstanceMember::mark()                 
    { 
        GCMARKOBJECT(multiname);
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
        GCMARKOBJECT(fInst); 
    }


/************************************************************************************
 *
 *  JS2Object
 *
 ************************************************************************************/

    Pond JS2Object::pond(POND_SIZE, NULL);
#ifdef DEBUG
    std::list<RootKeeper *> JS2Object::rootList;
#else
    std::list<PondScum **> JS2Object::rootList;
#endif

    // Add a pointer to the (address of a) gc-allocated object to the root list
    // (Note - we hand out an iterator, so it's essential to
    // use something like std::list that doesn't mess with locations)
#ifdef DEBUG
    JS2Object::RootIterator JS2Object::addRoot(RootKeeper *t)
    {
        return rootList.insert(rootList.end(), t);
    }
#else
    JS2Object::RootIterator JS2Object::addRoot(void *t)
    {
        PondScum **p = (PondScum **)t;
        ASSERT(p);
        return rootList.insert(rootList.end(), p);
    }
#endif

    // Remove a root pointer
    void JS2Object::removeRoot(RootIterator ri)
    {
        rootList.erase(ri);
    }

    void JS2Object::clear(JS2Metadata *meta)
    {
        pond.resetMarks();
        JS2Object::mark(meta);
        pond.moveUnmarkedToFreeList();
    }

    // Mark all reachable objects and put the rest back on the freelist
    uint32 JS2Object::gc()
    {
        pond.resetMarks();
        // Anything on the root list may also be a pointer to a JS2Object.
        for (RootIterator i = rootList.begin(), end = rootList.end(); (i != end); i++) {
#ifdef DEBUG
            RootKeeper *r = *i;
            if (*(r->p)) {
                PondScum *p = (*(r->p) - 1);
                ASSERT(p->owner && (p->getSize() >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
                if (p->isJS2Object()) {
                    JS2Object *obj = (JS2Object *)(p + 1);
                    GCMARKOBJECT(obj)
                }
                else
                    mark(p + 1);
            }
#else
            if (**i) {
                PondScum *p = (**i) - 1;
                ASSERT(p->owner && (p->getSize() >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
                if (p->isJS2Object()) {
                    JS2Object *obj = (JS2Object *)(p + 1);
                    GCMARKOBJECT(obj)
                }
                else
                    mark(p + 1);
            }
#endif
        }
        return pond.moveUnmarkedToFreeList();
    }

    // Allocate a chunk of size s
    void *JS2Object::alloc(size_t s, PondScum::ScumFlag flag)
    {
        s += sizeof(PondScum);
        // make sure that the thing is a multiple of 16 bytes
        if (s & 0xF) s += 16 - (s & 0xF);
        ASSERT(s <= 0x7FFFFFFF);
        void *p = pond.allocFromPond(s, flag);
        ASSERT(((ptrdiff_t)p & 0xF) == 0);
        return p;
    }

    // Release a chunk back to it's pond
    void JS2Object::unalloc(void *t)
    {
        PondScum *p = (PondScum *)t - 1;
        ASSERT(p->owner && (p->getSize() >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
        p->owner->returnToPond(p);
    }

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
    void *Pond::allocFromPond(size_t sz, PondScum::ScumFlag flag)
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
                uint32 released = JS2Object::gc();
                if (released > sz)
                    return JS2Object::alloc(sz - sizeof(PondScum), flag);
                nextPond = new Pond(sz, nextPond);
            }
            return nextPond->allocFromPond(sz, flag);
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
    uint32 Pond::moveUnmarkedToFreeList()
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
            released += nextPond->moveUnmarkedToFreeList();
        return released;
    }

}; // namespace MetaData
}; // namespace Javascript
