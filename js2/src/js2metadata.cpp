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

    inline char narrow(char16 ch) { return char(ch); }

    js2val JS2Metadata::readEvalString(const String &str, const String& fileName)
    {
        js2val result = JS2VAL_VOID;

        Arena a;
        Pragma::Flags flags = Pragma::es4;
        Parser p(world, a, flags, str, fileName);
        try {
            StmtNode *parsedStatements = p.parseProgram();
            ASSERT(p.lexer.peek(true).hasKind(Token::end));
            if (true)
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
                CompilationData *oldData = startCompilationUnit(NULL, str, fileName);
                ValidateStmtList(parsedStatements);
                result = ExecuteStmtList(RunPhase, parsedStatements);
                restoreCompilationUnit(oldData);
            }
        }
        catch (Exception &x) {
//            ASSERT(false);
            throw x;
        }
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
    void JS2Metadata::ValidateStmt(Context *cxt, Environment *env, StmtNode *p) 
    {
        CompoundAttribute *a = NULL;
        JS2Object::RootIterator ri = JS2Object::addRoot(&a);
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
                    ValidateStmtList(cxt, env, b->statements);
                    env->removeTopFrame();
                }
                break;
            case StmtNode::label:
                {
                    LabelStmtNode *l = checked_cast<LabelStmtNode *>(p);
                    l->labelID = bCon->getLabel();
    /*
        A labelled statement catches contained, named, 'breaks' but simply adds itself as a label for
        contained iteration statements. (i.e. you can 'break' out of a labelled statement, but not 'continue'
        one, however the statement label becomes a 'continuable' label for all contained iteration statements.
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
                    ValidateStmt(cxt, env, l->stmt);
                    targetList.pop_back();
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
            case StmtNode::ForIn:
                // XXX need to validate that first expression is a single binding ?
            case StmtNode::For:
                {
                    ForStmtNode *f = checked_cast<ForStmtNode *>(p);
                    f->breakLabelID = bCon->getLabel();
                    f->continueLabelID = bCon->getLabel();
                    if (f->initializer)
                        ValidateStmt(cxt, env, f->initializer);
                    if (f->expr2)
                        ValidateExpression(cxt, env, f->expr2);
                    if (f->expr3)
                        ValidateExpression(cxt, env, f->expr3);
                    f->breakLabelID = bCon->getLabel();
                    f->continueLabelID = bCon->getLabel();
                    targetList.push_back(p);
                    ValidateStmt(cxt, env, f->stmt);
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
                            ValidateStmt(cxt, env, s);
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
                    ValidateStmt(cxt, env, w->stmt);
                    targetList.pop_back();
                }
                break;
            case StmtNode::Break:
                {
                    GoStmtNode *g = checked_cast<GoStmtNode *>(p);
                    bool found = false;
                    for (TargetListReverseIterator si = targetList.rbegin(), end = targetList.rend(); (si != end); si++) {
                        if (g->name) {
                            // Make sure the name is on the targetList as a viable break target...
                            // (only label statements can introduce names)
                            if ((*si)->getKind() == StmtNode::label) {
                                LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                if (l->name == *g->name) {
                                    g->tgtID = l->labelID;
                                    found = true;
                                    break;
                                }
                            }
                        }
                        else {
                            // anything at all will do
                            switch ((*si)->getKind()) {
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
                            found = true;
                            break;
                        }
                    }
                    if (!found) 
                        reportError(Exception::syntaxError, "No such break target available", p->pos);
                }
                break;
            case StmtNode::Continue:
                {
                    GoStmtNode *g = checked_cast<GoStmtNode *>(p);
                    bool found = false;
                    for (TargetListIterator si = targetList.begin(), end = targetList.end(); (si != end); si++) {
                        if (g->name) {
                            // Make sure the name is on the targetList as a viable continue target...
                            if ((*si)->getKind() == StmtNode::label) {
                                LabelStmtNode *l = checked_cast<LabelStmtNode *>(*si);
                                if (l->name == *g->name) {
                                    g->tgtID = l->labelID;
                                    found = true;
                                    break;
                                }
                            }
                        }
                        else {
                            // only some non-label statements will do
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
                                    ForStmtNode *f = checked_cast<ForStmtNode *>(p);
                                    g->tgtID = f->breakLabelID;
                                }
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found) 
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
                    ValidateStmt(cxt, env, t->stmt);
                    if (t->finally)
                        ValidateStmt(cxt, env, t->finally);
                    CatchClause *c = t->catches;
                    while (c) {                    
                        ValidateStmt(cxt, env, c->stmt);
                        if (c->type)
                            ValidateExpression(cxt, env, c->type);
                        c = c->next;
                    }
                }
                break;
            case StmtNode::Return:
                {
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
                    bool untyped = true;
                    while (vb) {
                        if (vb->type) {
                            untyped = false;
                            break;
                        }
                        vb = vb->next;
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
                    ParameterFrame *compileFrame = new ParameterFrame(compileThis, prototype);
                    Frame *topFrame = env->getTopFrame();

                    if (unchecked 
                            && ((topFrame->kind == GlobalObjectKind)
                                            || (topFrame->kind == ParameterKind))
                            && (f->attributes == NULL)) {
                        HoistedVar *v = defineHoistedVar(env, f->function.name, p);
                        // XXX Here the spec. has ???, so the following is tentative
                        DynamicInstance *dInst = new DynamicInstance(functionClass);
                        dInst->fWrap = new FunctionWrapper(unchecked, compileFrame);
                        f->fWrap = dInst->fWrap;
                        v->value = OBJECT_TO_JS2VAL(dInst);
                    }
                    else {
                        FixedInstance *fInst = new FixedInstance(functionClass);
                        fInst->fWrap = new FunctionWrapper(unchecked, compileFrame);
                        f->fWrap = fInst->fWrap;
                        switch (memberMod) {
                        case Attribute::NoModifier:
                        case Attribute::Static:
                            {
                                Variable *v = new Variable(functionClass, OBJECT_TO_JS2VAL(fInst), true);
                                defineStaticMember(env, f->function.name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos);
                            }
                            break;
                        case Attribute::Virtual:
                        case Attribute::Final:
                            {
                                JS2Class *c = checked_cast<JS2Class *>(env->getTopFrame());
                                InstanceMember *m = new InstanceMethod(fInst);
                                defineInstanceMember(c, cxt, f->function.name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, m, p->pos);
                            }
                            break;
                        case Attribute::Constructor:
                            {
                                // XXX 
                            }
                            break;
                        }
                    }


                    CompilationData *oldData = startCompilationUnit(f->fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
                    env->addFrame(compileFrame);
                    VariableBinding *pb = f->function.parameters;
                    if (pb) {
                        NamespaceList publicNamespaceList;
                        publicNamespaceList.push_back(publicNamespace);
                        uint32 pCount = 0;
                        while (pb) {
                            pCount++;
                            pb = pb->next;
                        }
                        pb = f->function.parameters;
                        compileFrame->positional = new Variable *[pCount];
                        compileFrame->positionalCount = pCount;
                        pCount = 0;
                        while (pb) {
                            // XXX define a static binding for each parameter
                            Variable *v = new Variable();
                            compileFrame->positional[pCount++] = v;
                            pb->mn = defineStaticMember(env, pb->name, &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, p->pos);
                            pb = pb->next;
                        }
                    }
                    ValidateStmt(cxt, env, f->function.body);
                    env->removeTopFrame();
                    restoreCompilationUnit(oldData);
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
                    Frame *regionalFrame = env->getRegionalFrame();
                    while (vb)  {
                        const StringAtom *name = vb->name;
                        if (vb->type)
                            ValidateTypeExpression(cxt, env, vb->type);
                        vb->member = NULL;

                        if (cxt->strict && ((regionalFrame->kind == GlobalObjectKind)
                                            || (regionalFrame->kind == ParameterKind))
                                        && !immutable
                                        && (vs->attributes == NULL)
                                        && (vb->type == NULL)) {
                            defineHoistedVar(env, name, p);
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
                                    // Set type to FUTURE_TYPE - it will be resolved during 'PreEval'. The value is either FUTURE_VALUE
                                    // for 'const' - in which case the expression is compile time evaluated (or attempted) or set
                                    // to INACCESSIBLE until run time initialization occurs.
                                    Variable *v = new Variable(FUTURE_TYPE, immutable ? JS2VAL_FUTUREVALUE : JS2VAL_INACCESSIBLE, immutable);
                                    vb->member = v;
                                    v->vb = vb;
                                    vb->mn = defineStaticMember(env, name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos);
                                }
                                break;
                            case Attribute::Virtual:
                            case Attribute::Final: 
                                {
                                    JS2Class *c = checked_cast<JS2Class *>(env->getTopFrame());
                                    InstanceMember *m = new InstanceVariable(FUTURE_TYPE, immutable, (memberMod == Attribute::Final), c->slotCount++);
                                    vb->member = m;
                                    vb->osp = defineInstanceMember(c, cxt, name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, m, p->pos);
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
                    defineStaticMember(env, &ns->name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos);
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
                    JS2Object *proto = NULL;
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
                    JS2Class *c = new JS2Class(superClass, proto, new Namespace(engine->private_StringAtom), (a->dynamic || superClass->dynamic), true, final, &classStmt->name);
                    classStmt->c = c;
                    Variable *v = new Variable(classClass, OBJECT_TO_JS2VAL(c), true);
                    defineStaticMember(env, &classStmt->name, a->namespaces, a->overrideMod, a->xplicit, ReadWriteAccess, v, p->pos);
                    if (classStmt->body) {
                        env->addFrame(c);
                        ValidateStmtList(cxt, env, classStmt->body->statements);
                        ASSERT(env->getTopFrame() == c);
                        env->removeTopFrame();
                    }
                    c->complete = true;
                }
                break;
            case StmtNode::empty:
                break;
            }   // switch (p->getKind())
        }
        catch (Exception x) {
            env->setTopFrame(curTopFrame);
            JS2Object::removeRoot(ri);
            throw x;
        }
        JS2Object::removeRoot(ri);
    }


    /*
     * Evaluate the linked list of statement nodes beginning at 'p' 
     * (generate bytecode and then execute that bytecode
     */
    js2val JS2Metadata::ExecuteStmtList(Phase phase, StmtNode *p)
    {
        size_t lastPos = p->pos;
        while (p) {
            EvalStmt(&env, phase, p);
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
                    v->type = EvalTypeExpression(&env, CompilePhase, v->vb->type);
                }
                else
                    v->type = objectClass;
            }
        }
        return v->type;
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
                BlockFrame *runtimeFrame = new BlockFrame(b->compileFrame);
                env->addFrame(runtimeFrame);    // XXX is this right? shouldn't this be the compile frame until execution occurs?
                bCon->emitOp(ePushFrame, p->pos);
                bCon->addFrame(runtimeFrame);
                StmtNode *bp = b->statements;
                while (bp) {
                    EvalStmt(env, phase, bp);
                    bp = bp->next;
                }
                bCon->emitOp(ePopFrame, p->pos);
                env->removeTopFrame();
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
                bCon->emitBranch(eBranchFalse, falseStmt, p->pos);
                EvalStmt(env, phase, i->stmt);
                bCon->emitBranch(eBranch, skipOverFalseStmt, p->pos);
                bCon->setLabel(falseStmt);
                EvalStmt(env, phase, i->stmt2);
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
                bCon->emitBranch(eBranch, g->tgtID, p->pos);
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
                Reference *v = NULL;
                if (f->initializer->getKind() == StmtNode::Var) {
                    VariableStmtNode *vs = checked_cast<VariableStmtNode *>(f->initializer);
                    VariableBinding *vb = vs->bindings;
                    v = new LexicalReference(vb->name, cxt.strict);
                }
                else {
                    if (f->initializer->getKind() == StmtNode::expression) {
                        ExprStmtNode *e = checked_cast<ExprStmtNode *>(f->initializer);
                        v = EvalExprNode(env, phase, e->expr);
                        if (v == NULL)
                            reportError(Exception::semanticError, "for..in needs an lValue", p->pos);
                    }
                    else
                        NOT_REACHED("what else??");
                }            

                BytecodeContainer::LabelID loopTop = bCon->getLabel();

                Reference *r = EvalExprNode(env, phase, f->expr2);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eFirst, p->pos);
                bCon->emitBranch(eBranchFalse, f->breakLabelID, p->pos);

                bCon->setLabel(loopTop);
                targetList.push_back(p);
                bCon->emitOp(eForValue, p->pos);
                v->emitWriteBytecode(bCon, p->pos);
                bCon->emitOp(ePop, p->pos);     // clear iterator value from stack
                EvalStmt(env, phase, f->stmt);
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
                    EvalStmt(env, phase, f->initializer);
                if (f->expr2)
                    bCon->emitBranch(eBranch, testLocation, p->pos);
                bCon->setLabel(loopTop);
                targetList.push_back(p);
                EvalStmt(env, phase, f->stmt);
                targetList.pop_back();
                bCon->setLabel(f->continueLabelID);
                if (f->expr3) {
                    Reference *r = EvalExprNode(env, phase, f->expr3);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                }
                bCon->setLabel(testLocation);
                if (f->expr2) {
                    Reference *r = EvalExprNode(env, phase, f->expr2);
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
                uint16 swVarIndex = env->getTopFrame()->allocateTemp();
                BytecodeContainer::LabelID defaultLabel = NotALabel;

                Reference *r = EvalExprNode(env, phase, sw->expr);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eSlotWrite, p->pos);
                bCon->addShort(swVarIndex);

                // First time through, generate the conditional waterfall 
                StmtNode *s = sw->statements;
                while (s) {
                    if (s->getKind() == StmtNode::Case) {
                        ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                        if (c->expr) {
                            bCon->emitOp(eSlotRead, c->pos);
                            bCon->addShort(swVarIndex);
                            Reference *r = EvalExprNode(env, phase, c->expr);
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
                targetList.push_back(p);
                s = sw->statements;
                while (s) {
                    if (s->getKind() == StmtNode::Case) {
                        ExprStmtNode *c = checked_cast<ExprStmtNode *>(s);
                        bCon->setLabel(c->labelID);
                    }
                    else
                        EvalStmt(env, phase, s);
                    s = s->next;
                }
                targetList.pop_back();

                bCon->setLabel(sw->breakLabelID);
            }
            break;
        case StmtNode::While:
            {
                UnaryStmtNode *w = checked_cast<UnaryStmtNode *>(p);
                BytecodeContainer::LabelID loopTop = bCon->getLabel();
                bCon->emitBranch(eBranch, w->continueLabelID, p->pos);
                bCon->setLabel(loopTop);
                targetList.push_back(p);
                EvalStmt(env, phase, w->stmt);
                targetList.pop_back();
                bCon->setLabel(w->continueLabelID);
                Reference *r = EvalExprNode(env, phase, w->expr);
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
                EvalStmt(env, phase, w->stmt);
                targetList.pop_back();
                bCon->setLabel(w->continueLabelID);
                Reference *r = EvalExprNode(env, phase, w->expr);
                if (r) r->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchTrue, loopTop, p->pos);
                bCon->setLabel(w->breakLabelID);
            }
            break;
        case StmtNode::Throw:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                Reference *r = EvalExprNode(env, phase, e->expr);
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
                EvalStmt(env, phase, t->stmt);

                if (t->finally) {
                    bCon->emitBranch(eCallFinally, t_finallyLabel, p->pos);
                    bCon->emitBranch(eBranch, finishedLabel, p->pos);

                    bCon->setLabel(t_finallyLabel);
                    bCon->emitOp(eHandler, p->pos);
                    EvalStmt(env, phase, t->finally);
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
                            Reference *r = EvalExprNode(env, phase, c->type);
                            if (r) r->emitReadBytecode(bCon, p->pos);
                            bCon->emitOp(eIs, p->pos);
                            bCon->emitBranch(eBranchFalse, nextCatch, p->pos);
                        }
                        // write the exception object (on stack top) into the named
                        // local variable
                        Reference *r = new LexicalReference(&c->name, false);
                        r->emitWriteBytecode(bCon, p->pos);
                        bCon->emitOp(ePop, p->pos);
                        EvalStmt(env, phase, c->stmt);
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
                    Reference *r = EvalExprNode(env, phase, e->expr);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    bCon->emitOp(eReturn, p->pos);
                }
            }
            break;
        case StmtNode::Function:
            {
                FunctionStmtNode *f = checked_cast<FunctionStmtNode *>(p);
                CompilationData *oldData = startCompilationUnit(f->fWrap->bCon, bCon->mSource, bCon->mSourceLocation);
                env->addFrame(f->fWrap->compileFrame);
                EvalStmt(env, phase, f->function.body);
                // XXX need to make sure that all paths lead to an exit of some kind
                bCon->emitOp(eReturnVoid, p->pos);
                env->removeTopFrame();
                restoreCompilationUnit(oldData);
            }
            break;
        case StmtNode::Var:
        case StmtNode::Const:
            {
                // Note that the code here is the PreEval code plus the emit of the Eval bytecode
                VariableStmtNode *vs = checked_cast<VariableStmtNode *>(p);                
                VariableBinding *vb = vs->bindings;
                while (vb)  {
                    if (vb->member) {   // static or instance variable
                        if (vb->member->kind == Member::Variable) {
                            Variable *v = checked_cast<Variable *>(vb->member);
                            JS2Class *type = getVariableType(v, CompilePhase, p->pos);
                            if (JS2VAL_IS_FUTURE(v->value)) {   // it's a const, execute the initializer
                                v->value = JS2VAL_INACCESSIBLE;
                                if (vb->initializer) {
                                    try {
                                        js2val newValue = EvalExpression(env, CompilePhase, vb->initializer);
                                        v->value = engine->assignmentConversion(newValue, type);
                                    }
                                    catch (Exception x) {
                                        // If a compileExpressionError occurred, then the initialiser is 
                                        // not a compile-time constant expression. 
                                        if (x.kind != Exception::compileExpressionError)
                                            throw x;
                                    }
                                    // XXX more here - does 'static' act c-like?
                                    //
                                    // eGET_TOP_FRAME    <-- establish base??? why isn't this a lexical reference?
                                    // eDotRead <v->mn>
                                    // eIS_INACCESSIBLE      ??
                                    // eBRANCH_FALSE <lbl>   ?? eBRANCH_ACC <lbl>
                                    //      eGET_TOP_FRAME
                                    //      <vb->initializer code>
                                    //      <convert to 'type'>
                                    //      eDotWrite <v->mn>
                                    // <lbl>:
                                }
                                else
                                    // Would only have come here if the variable was immutable - i.e. a 'const' definition
                                    // XXX why isn't this handled at validation-time?
                                    reportError(Exception::compileExpressionError, "Missing compile time expression", p->pos);
                            }
                            else {
                                if (vb->initializer) {
                                    try {
                                        js2val newValue = EvalExpression(env, CompilePhase, vb->initializer);
                                        v->value = engine->assignmentConversion(newValue, type);
                                    }
                                    catch (Exception x) {
                                        // If a compileExpressionError occurred, then the initialiser is not a compile-time 
                                        // constant expression. In this case, ignore the error and leave the value of the 
                                        // variable inaccessible until it is defined at run time.
                                        if (x.kind != Exception::compileExpressionError)
                                            throw x;
                                        Reference *r = EvalExprNode(env, phase, vb->initializer);
                                        if (r) r->emitReadBytecode(bCon, p->pos);
                                        LexicalReference *lVal = new LexicalReference(vb->name, cxt.strict);
                                        lVal->variableMultiname->addNamespace(publicNamespace);
                                        lVal->emitWriteBytecode(bCon, p->pos);                                                        
                                    }
                                }
                            }
                        }
                        else {
                            ASSERT(vb->member->kind == Member::InstanceVariableKind);
                            InstanceVariable *v = checked_cast<InstanceVariable *>(vb->member);
                            JS2Class *t;
                            if (vb->type)
                                t = EvalTypeExpression(env, CompilePhase, vb->type);
                            else {
                                if (vb->osp->first->overriddenMember && (vb->osp->first->overriddenMember != POTENTIAL_CONFLICT))
                                    t = vb->osp->first->overriddenMember->type;
                                else
                                    if (vb->osp->second->overriddenMember && (vb->osp->second->overriddenMember != POTENTIAL_CONFLICT))
                                        t = vb->osp->second->overriddenMember->type;
                                    else
                                        t = objectClass;
                            }
                            v->type = t;
                        }
                    }
                    else { // HoistedVariable
                        if (vb->initializer) {
                            Reference *r = EvalExprNode(env, phase, vb->initializer);
                            if (r) r->emitReadBytecode(bCon, p->pos);
                            LexicalReference *lVal = new LexicalReference(vb->name, cxt.strict);
                            lVal->variableMultiname->addNamespace(publicNamespace);
                            lVal->emitWriteBytecode(bCon, p->pos);                                                        
                        }
                    }
                    vb = vb->next;
                }
            }
            break;
        case StmtNode::expression:
            {
                ExprStmtNode *e = checked_cast<ExprStmtNode *>(p);
                Reference *r = EvalExprNode(env, phase, e->expr);
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
                        EvalStmt(env, phase, bp);
                        bp = bp->next;
                    }
                    ASSERT(env->getTopFrame() == c);
                    env->removeTopFrame();
                    bCon->emitOp(ePopFrame, p->pos);
                }
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
            for (NamespaceListIterator i = cb->namespaces->begin(), end = cb->namespaces->end(); (i != end); i++)
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
        if (namespaces) {
            for (NamespaceListIterator i = namespaces->begin(), end = namespaces->end(); (i != end); i++) {
                GCMARKOBJECT(*i)
            }
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
        JS2Object::gc(this);            // XXX testing stability
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
                if (env->findThis(true) == JS2VAL_VOID)
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
        default:
            NOT_REACHED("Not Yet Implemented");
        } // switch (p->getKind())
    }



    /*
     * Evaluate an expression 'p' AND execute the associated bytecode
     */
    js2val JS2Metadata::EvalExpression(Environment *env, Phase phase, ExprNode *p)
    {
        js2val retval;
        uint8 *savePC = NULL;

        CompilationData *oldData = startCompilationUnit(NULL, bCon->mSource, bCon->mSourceLocation);
        try {
            Reference *r = EvalExprNode(env, phase, p);
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

    /*
     * Evaluate the expression (i.e. generate bytecode, but don't execute) rooted at p.
     */
    Reference *JS2Metadata::EvalExprNode(Environment *env, Phase phase, ExprNode *p)
    {
        Reference *returnRef = NULL;
        JS2Op op;

        switch (p->getKind()) {

        case ExprNode::parentheses:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                returnRef = EvalExprNode(env, phase, u->op);
            }
            break;
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
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) {
                    lVal->emitReadForWriteBackBytecode(bCon, p->pos);
                    Reference *rVal = EvalExprNode(env, phase, b->op2);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                }
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
                bCon->emitOp(op, p->pos);
                lVal->emitWriteBackBytecode(bCon, p->pos);
            }
            break;
        case ExprNode::lessThan:
            op = eLess;
            goto doBinary;
        case ExprNode::lessThanOrEqual:
            op = eLessEqual;
            goto doBinary;
        case ExprNode::greaterThan:
            op = eGreater;
            goto doBinary;
        case ExprNode::greaterThanOrEqual:
            op = eGreaterEqual;
            goto doBinary;
        case ExprNode::equal:
            op = eEqual;
            goto doBinary;
        case ExprNode::notEqual:
            op = eNotEqual;
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
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(op, p->pos);
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
                Reference *rVal = EvalExprNode(env, phase, u->op);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(op, p->pos);
            }
            break;

        case ExprNode::logicalAndEquals:
            {
                if (phase == CompilePhase) reportError(Exception::compileExpressionError, "Inappropriate compile time expression", p->pos);
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) 
                    lVal->emitReadForWriteBackBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchFalse, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
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
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) 
                    lVal->emitReadForWriteBackBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "Assignment needs an lValue", p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchTrue, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->setLabel(skipOverSecondHalf);
                lVal->emitWriteBackBytecode(bCon, p->pos);
            }
            break;

        case ExprNode::logicalAnd:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchFalse, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->setLabel(skipOverSecondHalf);
            }
            break;

        case ExprNode::logicalXor:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eLogicalXor, p->pos);
            }
            break;

        case ExprNode::logicalOr:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                BytecodeContainer::LabelID skipOverSecondHalf = bCon->getLabel();
                Reference *lVal = EvalExprNode(env, phase, b->op1);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eDup, p->pos);
                bCon->emitBranch(eBranchTrue, skipOverSecondHalf, p->pos);
                bCon->emitOp(ePop, p->pos);
                Reference *rVal = EvalExprNode(env, phase, b->op2);
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
                Reference *lVal = EvalExprNode(env, phase, c->op1);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranchFalse, falseConditionExpression, p->pos);

                lVal = EvalExprNode(env, phase, c->op2);
                if (lVal) lVal->emitReadBytecode(bCon, p->pos);
                bCon->emitBranch(eBranch, labelAtBottom, p->pos);

                bCon->setLabel(falseConditionExpression);
                //adjustStack(-1);        // the true case will leave a stack entry pending
                                          // but we can discard it since only one path will be taken.
                lVal = EvalExprNode(env, phase, c->op3);
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
                
                returnRef = new LexicalReference(&name, ns, cxt.strict);
            }
            break;
        case ExprNode::identifier:
            {
                IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(p);
                returnRef = new LexicalReference(&i->name, cxt.strict);
                ((LexicalReference *)returnRef)->variableMultiname->addNamespace(cxt);
            }
            break;
        case ExprNode::Delete:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, u->op);
                if (lVal)
                    lVal->emitDeleteBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "Delete needs an lValue", p->pos);
            }
            break;
        case ExprNode::postIncrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, u->op);
                if (lVal)
                    lVal->emitPostIncBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PostIncrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::postDecrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, u->op);
                if (lVal)
                    lVal->emitPostDecBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PostDecrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::preIncrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, u->op);
                if (lVal)
                    lVal->emitPreIncBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PreIncrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::preDecrement:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *lVal = EvalExprNode(env, phase, u->op);
                if (lVal)
                    lVal->emitPreDecBytecode(bCon, p->pos);
                else
                    reportError(Exception::semanticError, "PreDecrement needs an lValue", p->pos);
            }
            break;
        case ExprNode::index:
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *baseVal = EvalExprNode(env, phase, i->op);
                if (baseVal) baseVal->emitReadBytecode(bCon, p->pos);
                ExprPairList *ep = i->pairs;
                while (ep) {    // Validate has made sure there is only one, unnamed argument
                    Reference *argVal = EvalExprNode(env, phase, ep->value);
                    if (argVal) argVal->emitReadBytecode(bCon, p->pos);
                    ep = ep->next;
                }
                returnRef = new BracketReference();
            }
            break;
        case ExprNode::dot:
            {
                BinaryExprNode *b = checked_cast<BinaryExprNode *>(p);
                Reference *baseVal = EvalExprNode(env, phase, b->op1);
                if (baseVal) baseVal->emitReadBytecode(bCon, p->pos);

                if (b->op2->getKind() == ExprNode::identifier) {
                    IdentifierExprNode *i = checked_cast<IdentifierExprNode *>(b->op2);
                    returnRef = new DotReference(&i->name);
                } 
                else {
                    if (b->op2->getKind() == ExprNode::qualify) {
                        Reference *rVal = EvalExprNode(env, phase, b->op2);                        
                        ASSERT(rVal && checked_cast<LexicalReference *>(rVal));
                        returnRef = new DotReference(((LexicalReference *)rVal)->variableMultiname);
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
        case ExprNode::arrayLiteral:
            {
                int32 argCount = 0;
                PairListExprNode *plen = checked_cast<PairListExprNode *>(p);
                ExprPairList *e = plen->pairs;
                while (e) {
                    if (e->value) {
                        Reference *rVal = EvalExprNode(env, phase, e->value);
                        if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    }
                    argCount++;
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
                    Reference *rVal = EvalExprNode(env, phase, e->value);
                    if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                    switch (e->field->getKind()) {
                    case ExprNode::identifier:
                        bCon->addString(&checked_cast<IdentifierExprNode *>(e->field)->name, p->pos);
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
                bCon->emitOp(eNewObject, p->pos, -argCount + 1);    // pop argCount args and push a new object
                bCon->addShort((uint16)argCount);
            }
            break;
        case ExprNode::Typeof:
            {
                UnaryExprNode *u = checked_cast<UnaryExprNode *>(p);
                Reference *rVal = EvalExprNode(env, phase, u->op);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                bCon->emitOp(eTypeof, p->pos);
            }
            break;
        case ExprNode::call:
            {
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *rVal = EvalExprNode(env, phase, i->op);
                if (rVal) rVal->emitReadForInvokeBytecode(bCon, p->pos);
                ExprPairList *args = i->pairs;
                uint16 argCount = 0;
                while (args) {
                    Reference *r = EvalExprNode(env, phase, args->value);
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
                InvokeExprNode *i = checked_cast<InvokeExprNode *>(p);
                Reference *rVal = EvalExprNode(env, phase, i->op);
                if (rVal) rVal->emitReadBytecode(bCon, p->pos);
                ExprPairList *args = i->pairs;
                uint16 argCount = 0;
                while (args) {
                    Reference *r = EvalExprNode(env, phase, args->value);
                    if (r) r->emitReadBytecode(bCon, p->pos);
                    argCount++;
                    args = args->next;
                }
                bCon->emitOp(eNew, p->pos, -(argCount + 1) + 1);    // pop argCount args, the type/function, and push a result
                bCon->addShort(argCount);
            }
            break;
        default:
            NOT_REACHED("Not Yet Implemented");
        }
        return returnRef;
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
            if ((pf->kind == ParameterKind)
                    && !JS2VAL_IS_NULL(checked_cast<ParameterFrame *>(pf)->thisObject))
                if (allowPrototypeThis || !checked_cast<ParameterFrame *>(pf)->prototype)
                    return checked_cast<ParameterFrame *>(pf)->thisObject;
            pf = pf->nextFrame;
        }
        return JS2VAL_VOID;
    }

    // Read the value of a lexical reference - it's an error if that reference
    // doesn't have a binding somewhere.
    // Attempt the read in each frame in the current environment, stopping at the
    // first succesful effort. If the property can't be found in any frame, it's 
    // an error.
    js2val Environment::lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase)
    {
        LookupKind lookup(true, findThis(false));
        Frame *pf = firstFrame;
        while (pf) {
            js2val rval;    // XXX gc?
            if (meta->readProperty(pf, multiname, &lookup, phase, &rval))
                return rval;
            pf = pf->nextFrame;
        }
        meta->reportError(Exception::referenceError, "{0} is undefined", meta->engine->errorPos(), multiname->name);
        return JS2VAL_VOID;
    }

    // Attempt the write in the top frame in the current environment - if the property
    // exists, then fine. Otherwise create the property there.
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

    // Delete the named property in the current environment, return true if the property
    // can't be found, or the result of the deleteProperty call if it was found.
    bool Environment::lexicalDelete(JS2Metadata *meta, Multiname *multiname, Phase phase)
    {
        LookupKind lookup(true, findThis(false));
        Frame *pf = firstFrame;
        while (pf) {
            bool result;
            if (meta->deleteProperty(pf, multiname, &lookup, phase, &result))
                return result;
            pf = pf->nextFrame;
        }
        return true;
    }

    // Clone the pluralFrame bindings into the singularFrame, instantiating new members for each binding
    void Environment::instantiateFrame(Frame *pluralFrame, Frame *singularFrame)
    {
        StaticBindingIterator sbi, sbend;
        for (sbi = pluralFrame->staticReadBindings.begin(), sbend = pluralFrame->staticReadBindings.end(); (sbi != sbend); sbi++) {
            sbi->second->content->cloneContent = NULL;
        }
        for (sbi = pluralFrame->staticWriteBindings.begin(), sbend = pluralFrame->staticWriteBindings.end(); (sbi != sbend); sbi++) {
            sbi->second->content->cloneContent = NULL;
        }
        for (sbi = pluralFrame->staticReadBindings.begin(), sbend = pluralFrame->staticReadBindings.end(); (sbi != sbend); sbi++) {
            StaticBinding *sb;
            StaticBinding *m = sbi->second;
            if (m->content->cloneContent == NULL) {
                m->content->cloneContent = m->content->clone();
            }
            sb = new StaticBinding(m->qname, m->content->cloneContent);
            sb->xplicit = m->xplicit;
            const StaticBindingMap::value_type e(sbi->first, sb);
            singularFrame->staticReadBindings.insert(e);
        }
        for (sbi = pluralFrame->staticWriteBindings.begin(), sbend = pluralFrame->staticWriteBindings.end(); (sbi != sbend); sbi++) {
            StaticBinding *sb;
            StaticBinding *m = sbi->second;
            if (m->content->cloneContent == NULL) {
                m->content->cloneContent = m->content->clone();
            }
            sb = new StaticBinding(m->qname, m->content->cloneContent);
            sb->xplicit = m->xplicit;
            const StaticBindingMap::value_type e(sbi->first, sb);
            singularFrame->staticWriteBindings.insert(e);
        }

    }

    // need to mark all the frames in the environment - otherwise a marked frame that
    // came initially from the bytecodeContainer may prevent the markChildren call
    // from finding frames further down the list.
    void Environment::mark()
    { 
        Frame *pf = firstFrame;
        while (pf) {
            GCMARKOBJECT(pf)
            pf = pf->nextFrame;
        }
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
        if (nsList.empty())
            return true;
        for (NamespaceListIterator n = nsList.begin(), end = nsList.end(); (n != end); n++) {
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
    void Multiname::addNamespace(NamespaceList *ns)
    {
        for (NamespaceListIterator nli = ns->begin(), end = ns->end();
                (nli != end); nli++)
            nsList.push_back(*nli);
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void Multiname::markChildren()
    {
        for (NamespaceListIterator n = nsList.begin(), end = nsList.end(); (n != end); n++) {
            GCMARKOBJECT(*n)
        }
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
    Multiname *JS2Metadata::defineStaticMember(Environment *env, const StringAtom *id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, StaticMember *m, size_t pos)
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
	StaticBindingIterator b, end; 
        for (b = localFrame->staticReadBindings.lower_bound(*id),
                end = localFrame->staticReadBindings.upper_bound(*id); (b != end); b++) {
            if (mn->matches(b->second->qname))
                reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
        }
        

        // Check all frames below the current - up to the RegionalFrame - for a non-forbidden definition
        Frame *regionalFrame = env->getRegionalFrame();
        if (localFrame != regionalFrame) {
            Frame *fr = localFrame->nextFrame;
            while (fr != regionalFrame) {
                for (b = fr->staticReadBindings.lower_bound(*id),
                        end = fr->staticReadBindings.upper_bound(*id); (b != end); b++) {
                    if (mn->matches(b->second->qname) && (b->second->content->kind != StaticMember::Forbidden))
                        reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
                }
                fr = fr->nextFrame;
            }
        }
        if (regionalFrame->kind == GlobalObjectKind) {
            GlobalObject *gObj = checked_cast<GlobalObject *>(regionalFrame);
            DynamicPropertyIterator dp = gObj->dynamicProperties.find(*id);
            if (dp != gObj->dynamicProperties.end())
                reportError(Exception::definitionError, "Duplicate definition {0}", pos, id);
        }
        
        for (NamespaceListIterator nli = mn->nsList.begin(), nlend = mn->nsList.end(); (nli != nlend); nli++) {
            QualifiedName qName(*nli, *id);
            StaticBinding *sb = new StaticBinding(qName, m);
            const StaticBindingMap::value_type e(*id, sb);
            if (access & ReadAccess)
                regionalFrame->staticReadBindings.insert(e);
            if (access & WriteAccess)
                regionalFrame->staticWriteBindings.insert(e);
        }
        
        if (localFrame != regionalFrame) {
            Frame *fr = localFrame->nextFrame;
            while (fr != regionalFrame) {
                for (NamespaceListIterator nli = mn->nsList.begin(), nlend = mn->nsList.end(); (nli != nlend); nli++) {
                    QualifiedName qName(*nli, *id);
                    StaticBinding *sb = new StaticBinding(qName, forbiddenMember);
                    const StaticBindingMap::value_type e(*id, sb);
                    if (access & ReadAccess)
                        fr->staticReadBindings.insert(e);
                    if (access & WriteAccess)
                        fr->staticWriteBindings.insert(e);
                }
                fr = fr->nextFrame;
            }
        }
        return mn;
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
    OverrideStatus *JS2Metadata::searchForOverrides(JS2Class *c, const StringAtom *id, NamespaceList *namespaces, Access access, size_t pos)
    {
        OverrideStatus *os = new OverrideStatus(NULL, id);
        for (NamespaceListIterator ns = namespaces->begin(), end = namespaces->end(); (ns != end); ns++) {
            QualifiedName qname(*ns, *id);
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
    OverrideStatus *JS2Metadata::resolveOverrides(JS2Class *c, Context *cxt, const StringAtom *id, NamespaceList *namespaces, Access access, bool expectMethod, size_t pos)
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
                    os = new OverrideStatus(NULL, id);
                    os->multiname.addNamespace(namespaces);
                }
                else {
                    os = new OverrideStatus(POTENTIAL_CONFLICT, id);    // Didn't find the member with a specified namespace, but did with
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
            QualifiedName qname(*nli, *id);
            if (access & ReadAccess) {
                for (InstanceBindingIterator b = c->instanceReadBindings.lower_bound(*id),
                        end = c->instanceReadBindings.upper_bound(*id); (b != end); b++) {
                    if (qname == b->second->qname)
                        reportError(Exception::definitionError, "Illegal override", pos);
                }
            }        
            if (access & WriteAccess) {
                for (InstanceBindingIterator b = c->instanceWriteBindings.lower_bound(*id),
                        end = c->instanceWriteBindings.upper_bound(*id); (b != end); b++) {
                    if (qname == b->second->qname)
                        reportError(Exception::definitionError, "Illegal override", pos);
                }
            }
        }
        // Make sure we're getting what we expected
        if (expectMethod) {
            if (os->overriddenMember && (os->overriddenMember != POTENTIAL_CONFLICT) && (os->overriddenMember->kind != InstanceMember::InstanceMethodKind))
                reportError(Exception::definitionError, "Illegal override, expected method", pos);
        }
        else {
            if (os->overriddenMember && (os->overriddenMember != POTENTIAL_CONFLICT) && (os->overriddenMember->kind == InstanceMember::InstanceMethodKind))
                reportError(Exception::definitionError, "Illegal override, didn't expect method", pos);
        }

        return os;
    }

    // Define an instance member in the class. Verify that, if any overriding is happening, it's legal. The result pair indicates
    // the members being overridden.
    OverrideStatusPair *JS2Metadata::defineInstanceMember(JS2Class *c, Context *cxt, const StringAtom *id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, InstanceMember *m, size_t pos)
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

        if ((readStatus->overriddenMember && (readStatus->overriddenMember != POTENTIAL_CONFLICT))
                || (writeStatus->overriddenMember && (writeStatus->overriddenMember != POTENTIAL_CONFLICT))) {
            if ((overrideMod != Attribute::DoOverride) && (overrideMod != Attribute::OverrideUndefined))
                reportError(Exception::definitionError, "Illegal override", pos);
        }
        else {
            if ((readStatus->overriddenMember == POTENTIAL_CONFLICT) || (writeStatus->overriddenMember == POTENTIAL_CONFLICT)) {
                if ((overrideMod != Attribute::DontOverride) && (overrideMod != Attribute::OverrideUndefined))
                    reportError(Exception::definitionError, "Illegal override", pos);
            }
        }

        NamespaceListIterator nli, nlend;
        for (nli = readStatus->multiname.nsList.begin(), nlend = readStatus->multiname.nsList.end(); (nli != nlend); nli++) {
            QualifiedName qName(*nli, *id);
            InstanceBinding *ib = new InstanceBinding(qName, m);
            const InstanceBindingMap::value_type e(*id, ib);
            c->instanceReadBindings.insert(e);
        }
        
        for (nli = writeStatus->multiname.nsList.begin(), nlend = writeStatus->multiname.nsList.end(); (nli != nlend); nli++) {
            QualifiedName qName(*nli, *id);
            InstanceBinding *ib = new InstanceBinding(qName, m);
            const InstanceBindingMap::value_type e(*id, ib);
            c->instanceWriteBindings.insert(e);
        }
        
        return new OverrideStatusPair(readStatus, writeStatus);;
    }

    // Define a hoisted var in the current frame (either Global or a Function)
    HoistedVar *JS2Metadata::defineHoistedVar(Environment *env, const StringAtom *id, StmtNode *p)
    {
        HoistedVar *result = NULL;
        QualifiedName qName(publicNamespace, *id);
        Frame *regionalFrame = env->getRegionalFrame();
        ASSERT((env->getTopFrame()->kind == GlobalObjectKind) || (env->getTopFrame()->kind == ParameterKind));
    
        // run through all the existing bindings, both read and write, to see if this
        // variable already exists.
        StaticBindingIterator b, end;
        for (b = regionalFrame->staticReadBindings.lower_bound(*id),
                end = regionalFrame->staticReadBindings.upper_bound(*id); (b != end); b++) {
            if (b->second->qname == qName) {
                if (b->second->content->kind != StaticMember::HoistedVariable)
                    reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
                else {
                    result = checked_cast<HoistedVar *>(b->second->content);
                    break;
                }
            }
        }
        for (b = regionalFrame->staticWriteBindings.lower_bound(*id),
                end = regionalFrame->staticWriteBindings.upper_bound(*id); (b != end); b++) {
            if (b->second->qname == qName) {
                if (b->second->content->kind != StaticMember::HoistedVariable)
                    reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
                else {
                    result = checked_cast<HoistedVar *>(b->second->content);
                    break;
                }
            }
        }
        if (result == NULL) {
            if (regionalFrame->kind == GlobalObjectKind) {
                GlobalObject *gObj = checked_cast<GlobalObject *>(regionalFrame);
                DynamicPropertyIterator dp = gObj->dynamicProperties.find(*id);
                if (dp != gObj->dynamicProperties.end())
                    reportError(Exception::definitionError, "Duplicate definition {0}", p->pos, id);
            }
            // XXX ok to use same binding in read & write maps?
            result = new HoistedVar();
            StaticBinding *sb = new StaticBinding(qName, result);
            const StaticBindingMap::value_type e(*id, sb);

            // XXX ok to use same value_type in different multimaps?
            regionalFrame->staticReadBindings.insert(e);
            regionalFrame->staticWriteBindings.insert(e);
        }
        //else A hoisted binding of the same var already exists, so there is no need to create another one
        return result;
    }

    static js2val Object_toString(JS2Metadata *meta, const js2val /* thisValue */, js2val /* argv */ [], uint32 /* argc */)
    {
        return STRING_TO_JS2VAL(meta->engine->object_StringAtom);
    }
    
    static js2val GlobalObject_isNaN(JS2Metadata *meta, const js2val /* thisValue */, js2val argv[], uint32 /* argc */)
    {
        float64 d = meta->toFloat64(argv[0]);
        return BOOLEAN_TO_JS2VAL(JSDOUBLE_IS_NaN(d));
    }

    void JS2Metadata::addGlobalObjectFunction(char *name, NativeCode *code)
    {
        FixedInstance *fInst = new FixedInstance(functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_VOID, true), code);
        writeDynamicProperty(glob, new Multiname(&world.identifiers[name], publicNamespace), true, OBJECT_TO_JS2VAL(fInst), RunPhase);
    }

#define MAKEBUILTINCLASS(c, super, dynamic, allowNull, final, name) c = new JS2Class(super, NULL, new Namespace(engine->private_StringAtom), dynamic, allowNull, final, name); c->complete = true

    JS2Metadata::JS2Metadata(World &world) :
        world(world),
        engine(new JS2Engine(world)),
        publicNamespace(new Namespace(engine->public_StringAtom)),
        bCon(new BytecodeContainer()),
        glob(new GlobalObject(world)),
        env(new MetaData::SystemFrame(), glob)
    {
        engine->meta = this;

        cxt.openNamespaces.clear();
        cxt.openNamespaces.push_back(publicNamespace);

        MAKEBUILTINCLASS(objectClass, NULL, false, true, false, engine->object_StringAtom);
        MAKEBUILTINCLASS(undefinedClass, objectClass, false, false, true, engine->undefined_StringAtom);
        MAKEBUILTINCLASS(nullClass, objectClass, false, true, true, engine->null_StringAtom);
        MAKEBUILTINCLASS(booleanClass, objectClass, false, false, true, &world.identifiers["boolean"]);
        MAKEBUILTINCLASS(generalNumberClass, objectClass, false, false, false, &world.identifiers["general number"]);
        MAKEBUILTINCLASS(numberClass, generalNumberClass, false, false, true, &world.identifiers["number"]);
        MAKEBUILTINCLASS(characterClass, objectClass, false, false, true, &world.identifiers["character"]);
        MAKEBUILTINCLASS(stringClass, objectClass, false, false, true, &world.identifiers["string"]);
        MAKEBUILTINCLASS(namespaceClass, objectClass, false, true, true, &world.identifiers["namespace"]);
        MAKEBUILTINCLASS(attributeClass, objectClass, false, true, true, &world.identifiers["attribute"]);
        MAKEBUILTINCLASS(classClass, objectClass, false, true, true, &world.identifiers["class"]);
        MAKEBUILTINCLASS(functionClass, objectClass, false, true, true, engine->function_StringAtom);
        MAKEBUILTINCLASS(prototypeClass, objectClass, true, true, true, &world.identifiers["prototype"]);
        MAKEBUILTINCLASS(packageClass, objectClass, true, true, true, &world.identifiers["package"]);
        
        // A 'forbidden' member, used to mark hidden bindings
        forbiddenMember = new StaticMember(Member::Forbidden);


/*** ECMA 3  Global Object ***/
        // Non-function properties of the global object : 'undefined', 'NaN' and 'Infinity'
// XXX Or are these fixed properties?
        writeDynamicProperty(glob, new Multiname(engine->undefined_StringAtom, publicNamespace), true, JS2VAL_UNDEFINED, RunPhase);
        writeDynamicProperty(glob, new Multiname(&world.identifiers["NaN"], publicNamespace), true, engine->nanValue, RunPhase);
        writeDynamicProperty(glob, new Multiname(&world.identifiers["Infinity"], publicNamespace), true, engine->posInfValue, RunPhase);
        // Function properties of the global object 
        addGlobalObjectFunction("isNaN", GlobalObject_isNaN);


/*** ECMA 3  Object Class ***/
        // Function properties of the Object prototype object
        objectClass->prototype = new PrototypeInstance(NULL, objectClass);
// XXX Or make this a static class member?
        FixedInstance *fInst = new FixedInstance(functionClass);
        fInst->fWrap = new FunctionWrapper(true, new ParameterFrame(JS2VAL_VOID, true), Object_toString);
        writeDynamicProperty(objectClass->prototype, new Multiname(engine->toString_StringAtom, publicNamespace), true, OBJECT_TO_JS2VAL(fInst), RunPhase);


        // needed for class instance variables etc...
        NamespaceList publicNamespaceList;
        publicNamespaceList.push_back(publicNamespace);
        Variable *v;

/*** ECMA 3  Date Class ***/
        MAKEBUILTINCLASS(dateClass, objectClass, true, true, true, &world.identifiers["Date"]);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(dateClass), true);
        defineStaticMember(&env, &world.identifiers["Date"], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
 //       dateClass->prototype = new PrototypeInstance(NULL, dateClass);
        initDateObject(this);

/*** ECMA 3  RegExp Class ***/
        MAKEBUILTINCLASS(regexpClass, objectClass, true, true, true, &world.identifiers["RegExp"]);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(regexpClass), true);
        defineStaticMember(&env, &world.identifiers["RegExp"], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        initRegExpObject(this);

/*** ECMA 3  String Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(stringClass), true);
        defineStaticMember(&env, &world.identifiers["String"], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        initStringObject(this);

/*** ECMA 3  Number Class ***/
        v = new Variable(classClass, OBJECT_TO_JS2VAL(numberClass), true);
        defineStaticMember(&env, &world.identifiers["Number"], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        initNumberObject(this);

/*** ECMA 3  Math Class ***/
        MAKEBUILTINCLASS(mathClass, objectClass, true, true, true, &world.identifiers["Math"]);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(mathClass), true);
        defineStaticMember(&env, &world.identifiers["Math"], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        initMathObject(this);

/*** ECMA 3  Array Class ***/
        MAKEBUILTINCLASS(arrayClass, objectClass, true, true, true, &world.identifiers["Array"]);
        v = new Variable(classClass, OBJECT_TO_JS2VAL(arrayClass), true);
        defineStaticMember(&env, &world.identifiers["Array"], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        initArrayObject(this);


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
        JS2Object *obj = JS2VAL_TO_OBJECT(objVal);
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

        case MethodClosureKind:
            return functionClass;

        case SystemKind:
        case ParameterKind: 
        case BlockKind: 
        default:
            ASSERT(false);
            return NULL;
        }
    }

    // Scan this object and, if appropriate, it's prototype chain
    // looking for the given name. Return the containing object.
    JS2Object *JS2Metadata::lookupDynamicProperty(JS2Object *obj, const String *name)
    {
        ASSERT(obj);
        DynamicPropertyMap *dMap = NULL;
        bool isPrototypeInstance = false;
        if (obj->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(obj))->dynamicProperties;
        else
        if (obj->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(obj))->dynamicProperties;
        else {
            ASSERT(obj->kind == PrototypeInstanceKind);
            isPrototypeInstance = true;
            dMap = &(checked_cast<PrototypeInstance *>(obj))->dynamicProperties;
        }
        for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
            if (i->first == *name) {
                return obj;
            }
        }
        if (isPrototypeInstance) {
            PrototypeInstance *pInst = checked_cast<PrototypeInstance *>(obj);
            if (pInst->parent)
                return lookupDynamicProperty(pInst->parent, name);
        }
        return NULL;
    }

    // Return true if the object contains the property, but don't
    // scan the prototype chain. Only scan dynamic properties. XXX
    bool JS2Metadata::hasOwnProperty(JS2Object *obj, const String *name)
    {
        ASSERT(obj);
        DynamicPropertyMap *dMap = NULL;
        bool isPrototypeInstance = false;
        if (obj->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(obj))->dynamicProperties;
        else
        if (obj->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(obj))->dynamicProperties;
        else {
            ASSERT(obj->kind == PrototypeInstanceKind);
            isPrototypeInstance = true;
            dMap = &(checked_cast<PrototypeInstance *>(obj))->dynamicProperties;
        }
        for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
            if (i->first == *name) {
                return true;
            }
        }
        return false;
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
        const String *name = multiname->name;
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
            if (i->first == *name) {
                *rval = i->second;
                return true;
            }
        }
        if (isPrototypeInstance) {
            PrototypeInstance *pInst = checked_cast<PrototypeInstance *>(container);
            if (pInst->parent)
                return readDynamicProperty(pInst->parent, multiname, lookupKind, phase, rval);
        }
        if (lookupKind->isPropertyLookup()) {
            *rval = JS2VAL_UNDEFINED;
            return true;
        }
        return false;   // 'None'
    }

    void DynamicInstance::writeProperty(JS2Metadata * /* meta */, const String *name, js2val newValue)
    {
        const DynamicPropertyMap::value_type e(*name, newValue);
        dynamicProperties.insert(e);
    }

    void ArrayInstance::writeProperty(JS2Metadata *meta, const String *name, js2val newValue)
    {
        // An index has to pass the test that :
        //   ToString(ToUint32(ToString(index))) == ToString(index)     
        // (we already have done the 'ToString(index)' part, so require
        //
        //  ToString(ToUint32(name)) == name
        //
        const DynamicPropertyMap::value_type e(*name, newValue);
        dynamicProperties.insert(e);

        const char16 *numEnd;        
        float64 f = stringToDouble(name->data(), name->data() + name->length(), numEnd);
        uint32 index = JS2Engine::float64toUInt32(f);

        if (index == f) {
            uint32 length = getLength(meta, this);
            if (index >= length)
                setLength(meta, this, index + 1);
        }
    }

    // Write a value to a dynamic container - inserting into the map if not already there (if createIfMissing)
    bool JS2Metadata::writeDynamicProperty(JS2Object *container, Multiname *multiname, bool createIfMissing, js2val newValue, Phase phase)
    {
        ASSERT(container && ((container->kind == DynamicInstanceKind) 
                                || (container->kind == GlobalObjectKind)
                                || (container->kind == PrototypeInstanceKind)));
        if (!multiname->onList(publicNamespace))
            return false;
        const String *name = multiname->name;
        DynamicPropertyMap *dMap = NULL;
        if (container->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(container))->dynamicProperties;
        else
        if (container->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(container))->dynamicProperties;
        else 
            dMap = &(checked_cast<PrototypeInstance *>(container))->dynamicProperties;
        for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
            if (i->first == *name) {
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
                dynInst->writeProperty(this, name, newValue);
                return true;
            }
        }
        else {
            if (container->kind == GlobalObjectKind) {
                GlobalObject *glob = checked_cast<GlobalObject *>(container);
                StaticMember *m = findFlatMember(glob, multiname, ReadAccess, phase);
                if (m == NULL) {
                    const DynamicPropertyMap::value_type e(*name, newValue);
                    glob->dynamicProperties.insert(e);
                    return true;
                }
            }
            else {
                PrototypeInstance *pInst = checked_cast<PrototypeInstance *>(container);
                const DynamicPropertyMap::value_type e(*name, newValue);
                pInst->dynamicProperties.insert(e);
                return true;
            }
        }
        return false;   // 'None'
    }

    // Read a value from the static member
    bool JS2Metadata::readStaticMember(StaticMember *m, Phase phase, js2val *rval)
    {
        if (m == NULL)
            return false;   // 'None'
        switch (m->kind) {
        case StaticMember::Forbidden:
            reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
            break;
        case StaticMember::Variable:
            {
                Variable *v = checked_cast<Variable *>(m); 
                if ((phase == CompilePhase) && !v->immutable)
                    reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
                *rval = v->value;
                return true;
            }
        case StaticMember::HoistedVariable:
            if (phase == CompilePhase) 
                reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
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

    // Write a value to the static member
    bool JS2Metadata::writeStaticMember(StaticMember *m, js2val newValue, Phase /* phase */) // phase not used?
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
            InstanceBinding *ib = resolveInstanceMemberName(c, multiname, ReadAccess, phase);
            if ((ib == NULL) && isDynamicInstance) 
                return readDynamicProperty(JS2VAL_TO_OBJECT(containerVal), multiname, lookupKind, phase, rval);
            else {
                // XXX Spec. would have us passing a primitive here since ES4 is 'not addressing' the issue
                // of so-called wrapper objects.
                return readInstanceMember(toObject(containerVal), c, (ib) ? &ib->qname : NULL, phase, rval);
            }
        }
        JS2Object *container = JS2VAL_TO_OBJECT(containerVal);
        switch (container->kind) {
        case AttributeObjectKind:
        case MultinameKind:
        case FixedInstanceKind: 
        case MethodClosureKind:
            goto readClassProperty;
        case DynamicInstanceKind:
            isDynamicInstance = true;
            goto readClassProperty;

        case SystemKind:
        case GlobalObjectKind: 
        case PackageKind:
        case ParameterKind: 
        case BlockKind: 
        case ClassKind:
            return readProperty(checked_cast<Frame *>(container), multiname, lookupKind, phase, rval);

        case PrototypeInstanceKind: 
            return readDynamicProperty(container, multiname, lookupKind, phase, rval);

        default:
            ASSERT(false);
            return false;
        }
    }

    // Use the slotIndex from the instanceVariable to access the slot
    Slot *JS2Metadata::findSlot(js2val thisObjVal, InstanceVariable *id)
    {
        ASSERT(JS2VAL_IS_OBJECT(thisObjVal) 
                    && ((JS2VAL_TO_OBJECT(thisObjVal)->kind == DynamicInstanceKind)
                        || (JS2VAL_TO_OBJECT(thisObjVal)->kind == FixedInstanceKind)));
        JS2Object *thisObj = JS2VAL_TO_OBJECT(thisObjVal);
        if (thisObj->kind == DynamicInstanceKind)
            return &checked_cast<DynamicInstance *>(thisObj)->slots[id->slotIndex];
        else
            return &checked_cast<FixedInstance *>(thisObj)->slots[id->slotIndex];
    }

    // Read the value of an instanceMember, if valid
    bool JS2Metadata::readInstanceMember(js2val containerVal, JS2Class *c, QualifiedName *qname, Phase phase, js2val *rval)
    {
        InstanceMember *m = findInstanceMember(c, qname, ReadAccess);
        if (m == NULL) return false;
        switch (m->kind) {
        case InstanceMember::InstanceVariableKind:
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
            break;

        case InstanceMember::InstanceMethodKind:
            {
                *rval = OBJECT_TO_JS2VAL(new MethodClosure(containerVal, checked_cast<InstanceMethod *>(m)));
                return true;
            }
            break;

        case InstanceMember::InstanceAccessorKind:
            break;
        }
        ASSERT(false);
        return false;
    }

    // Read the value of a property in the frame. Return true/false if that frame has
    // the property or not. If it does, return it's value
    bool JS2Metadata::readProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval)
    {
        if (container->kind != ClassKind) {
            // Must be System, Global, Package, Parameter or Block
            StaticMember *m = findFlatMember(container, multiname, ReadAccess, phase);
            if (!m && (container->kind == GlobalObjectKind))
                return readDynamicProperty(container, multiname, lookupKind, phase, rval);
            else
                return readStaticMember(m, phase, rval);
        }
        else {
            // XXX using JS2VAL_UNINITIALIZED to signal generic 'this'
            js2val thisObject;
            if (lookupKind->isPropertyLookup()) 
                thisObject = JS2VAL_UNINITIALIZED;
            else
                thisObject = lookupKind->thisObject;
            MemberDescriptor m2;
            if (!findStaticMember(checked_cast<JS2Class *>(container), multiname, ReadAccess, phase, &m2) 
                        || m2.staticMember)
                return readStaticMember(m2.staticMember, phase, rval);
            else {
                if (JS2VAL_IS_NULL(thisObject))
                    reportError(Exception::propertyAccessError, "Null 'this' object", engine->errorPos());
                if (JS2VAL_IS_INACCESSIBLE(thisObject))
                    reportError(Exception::compileExpressionError, "Inaccesible 'this' object", engine->errorPos());
                if (JS2VAL_IS_UNINITIALIZED(thisObject)) {
                    // 'this' is {generic}
                    // XXX is ??? in spec.
                }
                return readInstanceMember(thisObject, objectType(thisObject), m2.qname, phase, rval);
            }
        }
    }

    // Write the value of a property in the container. Return true/false if that container has
    // the property or not.
    bool JS2Metadata::writeProperty(js2val containerVal, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue, Phase phase)
    {
        JS2Class *c = NULL;
        if (JS2VAL_IS_PRIMITIVE(containerVal))
            return false;
        JS2Object *container = JS2VAL_TO_OBJECT(containerVal);
        switch (container->kind) {
        case AttributeObjectKind:
        case MultinameKind:
        case MethodClosureKind:
            return false;

        case FixedInstanceKind:
            c = checked_cast<FixedInstance *>(container)->type;
            goto instanceWrite;
        case DynamicInstanceKind:
            c = checked_cast<DynamicInstance *>(container)->type;
            goto instanceWrite;
    instanceWrite:
            {
                InstanceBinding *ib = resolveInstanceMemberName(c, multiname, WriteAccess, phase);
                if ((ib == NULL) && (container->kind == DynamicInstanceKind))
                    return writeDynamicProperty(container, multiname, createIfMissing, newValue, phase);
                else
                    return writeInstanceMember(containerVal, c, (ib) ? &ib->qname : NULL, newValue, phase); 
            }

        case SystemKind:
        case GlobalObjectKind: 
        case PackageKind:
        case ParameterKind: 
        case BlockKind: 
        case ClassKind:
            return writeProperty(checked_cast<Frame *>(container), multiname, lookupKind, createIfMissing, newValue, phase);

        case PrototypeInstanceKind: 
            return writeDynamicProperty(container, multiname, createIfMissing, newValue, phase);

        default:
            ASSERT(false);
            return false;
        }

    }
    
    // Write the value of an instance member into a container instance.
    // Only instanceVariables are writable.
    bool JS2Metadata::writeInstanceMember(js2val containerVal, JS2Class *c, QualifiedName *qname, js2val newValue, Phase phase)
    {
        if (phase == CompilePhase)
            reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
        InstanceMember *m = findInstanceMember(c, qname, WriteAccess);
        if (m == NULL) return false;
        switch (m->kind) {
        case InstanceMember::InstanceVariableKind:
            {
                InstanceVariable *mv = checked_cast<InstanceVariable *>(m);
                Slot *s = findSlot(containerVal, mv);
                if (mv->immutable && JS2VAL_IS_INITIALIZED(s->value))
                    reportError(Exception::propertyAccessError, "Reinitialization of constant", engine->errorPos());
                s->value = engine->assignmentConversion(newValue, mv->type);
                return true;
            }
        
        case InstanceMember::InstanceMethodKind:
        case InstanceMember::InstanceAccessorKind:
            reportError(Exception::propertyAccessError, "Attempt to write to a method", engine->errorPos());
            break;
        }
        ASSERT(false);
        return false;

    }

    // Write the value of a property in the frame. Return true/false if that frame has
    // the property or not.
    bool JS2Metadata::writeProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue, Phase phase)
    {
        if (container->kind != ClassKind) {
            // Must be System, Global, Package, Parameter or Block
            StaticMember *m = findFlatMember(container, multiname, WriteAccess, phase);
            if (!m && (container->kind == GlobalObjectKind))
                return writeDynamicProperty(container, multiname, createIfMissing, newValue, phase);
            else
                return writeStaticMember(m, newValue, phase);
        }
        else {
            // XXX using JS2VAL_UNINITIALIZED to signal generic 'this'
            js2val thisObject;
            if (lookupKind->isPropertyLookup()) 
                thisObject = JS2VAL_UNINITIALIZED;
            else
                thisObject = lookupKind->thisObject;
            MemberDescriptor m2;
            if (!findStaticMember(checked_cast<JS2Class *>(container), multiname, WriteAccess, phase, &m2) 
                            || m2.staticMember)
                return writeStaticMember(m2.staticMember, newValue, phase);
            else {
                if (JS2VAL_IS_NULL(thisObject))
                    reportError(Exception::propertyAccessError, "Null 'this' object", engine->errorPos());
                if (JS2VAL_IS_VOID(thisObject))
                    reportError(Exception::compileExpressionError, "Undefined 'this' object", engine->errorPos());
                if (JS2VAL_IS_UNINITIALIZED(thisObject)) {
                    // 'this' is {generic}
                    // XXX is ??? in spec.
                }
                return writeInstanceMember(thisObject, objectType(thisObject), m2.qname, newValue, phase);
            }
        }
    }

    bool JS2Metadata::deleteProperty(js2val containerVal, Multiname *multiname, LookupKind *lookupKind, Phase phase, bool *result)
    {
        ASSERT(phase == RunPhase);
        bool isDynamicInstance = false;
        if (JS2VAL_IS_PRIMITIVE(containerVal)) {
deleteClassProperty:
            JS2Class *c = objectType(containerVal);
            InstanceBinding *ib = resolveInstanceMemberName(c, multiname, ReadAccess, phase);
            if ((ib == NULL) && isDynamicInstance) 
                return deleteDynamicProperty(JS2VAL_TO_OBJECT(containerVal), multiname, lookupKind, result);
            else 
                return deleteInstanceMember(c, (ib) ? &ib->qname : NULL, result);
        }
        JS2Object *container = JS2VAL_TO_OBJECT(containerVal);
        switch (container->kind) {
        case AttributeObjectKind:
        case MultinameKind:
        case FixedInstanceKind: 
        case MethodClosureKind:
            goto deleteClassProperty;
        case DynamicInstanceKind:
            isDynamicInstance = true;
            goto deleteClassProperty;

        case SystemKind:
        case GlobalObjectKind: 
        case PackageKind:
        case ParameterKind: 
        case BlockKind: 
        case ClassKind:
            return deleteProperty(checked_cast<Frame *>(container), multiname, lookupKind, phase, result);

        case PrototypeInstanceKind: 
            return deleteDynamicProperty(container, multiname, lookupKind, result);

        default:
            ASSERT(false);
            return false;
        }
    }

    bool JS2Metadata::deleteProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, Phase phase, bool *result)
    {
        ASSERT(phase == RunPhase);
        if (container->kind != ClassKind) {
            // Must be System, Global, Package, Parameter or Block
            StaticMember *m = findFlatMember(container, multiname, ReadAccess, phase);
            if (!m && (container->kind == GlobalObjectKind))
                return deleteDynamicProperty(container, multiname, lookupKind, result);
            else
                return deleteStaticMember(m, result);
        }
        else {
            // XXX using JS2VAL_UNINITIALIZED to signal generic 'this'
            js2val thisObject;
            if (lookupKind->isPropertyLookup()) 
                thisObject = JS2VAL_UNINITIALIZED;
            else
                thisObject = lookupKind->thisObject;
            MemberDescriptor m2;
            if (findStaticMember(checked_cast<JS2Class *>(container), multiname, ReadAccess, phase, &m2) && m2.staticMember)
                return deleteStaticMember(m2.staticMember, result);
            else {
                if (JS2VAL_IS_NULL(thisObject))
                    reportError(Exception::propertyAccessError, "Null 'this' object", engine->errorPos());
                if (JS2VAL_IS_UNINITIALIZED(thisObject)) {
                    *result = false;
                    return true;
                }
                return deleteInstanceMember(objectType(thisObject), m2.qname, result);
            }
        }
    }

    bool JS2Metadata::deleteDynamicProperty(JS2Object *container, Multiname *multiname, LookupKind * /* lookupKind */, bool *result)
    {
        ASSERT(container && ((container->kind == DynamicInstanceKind) 
                                || (container->kind == GlobalObjectKind)
                                || (container->kind == PrototypeInstanceKind)));
        if (!multiname->onList(publicNamespace))
            return false;
        const String *name = multiname->name;
        DynamicPropertyMap *dMap = NULL;
        if (container->kind == DynamicInstanceKind)
            dMap = &(checked_cast<DynamicInstance *>(container))->dynamicProperties;
        else
        if (container->kind == GlobalObjectKind)
            dMap = &(checked_cast<GlobalObject *>(container))->dynamicProperties;
        else {
            dMap = &(checked_cast<PrototypeInstance *>(container))->dynamicProperties;
        }
        for (DynamicPropertyIterator i = dMap->begin(), end = dMap->end(); (i != end); i++) {
            if (i->first == *name) {
                dMap->erase(i);
                *result = true;
                return true;
            }
        }
        return false;
    }

    bool JS2Metadata::deleteStaticMember(StaticMember *m, bool *result)
    {
        if (m == NULL)
            return false;   // 'None'
        switch (m->kind) {
        case StaticMember::Forbidden:
            reportError(Exception::propertyAccessError, "Forbidden access", engine->errorPos());
            break;
        case StaticMember::Variable:
        case StaticMember::HoistedVariable:
        case StaticMember::ConstructorMethod:
        case StaticMember::Accessor:
            *result = false;
            return true;
        }
        NOT_REACHED("Bad member kind");
        return false;
    }

    bool JS2Metadata::deleteInstanceMember(JS2Class *c, QualifiedName *qname, bool *result)
    {
        InstanceMember *m = findInstanceMember(c, qname, ReadAccess);
        if (m == NULL) return false;
        *result = false;
        return true;
    }


    // Find a binding that matches the multiname and access.
    // It's an error if more than one such binding exists.
    StaticMember *JS2Metadata::findFlatMember(Frame *container, Multiname *multiname, Access access, Phase /* phase */)
    {
        StaticMember *found = NULL;
        StaticBindingIterator b, end;
        if (access & ReadAccess) {
            b = container->staticReadBindings.lower_bound(*multiname->name);
            end = container->staticReadBindings.upper_bound(*multiname->name);
        }
        else {
            b = container->staticWriteBindings.lower_bound(*multiname->name);
            end = container->staticWriteBindings.upper_bound(*multiname->name);
        }
        while (true) {
            if (b == end) {
                if (access == ReadWriteAccess) {
                    access = WriteAccess;
                    b = container->staticWriteBindings.lower_bound(*multiname->name);
                    end = container->staticWriteBindings.upper_bound(*multiname->name);
                    continue;
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

    // Find the multiname in the class - either in the static bindings (first) or
    // in the instance bindings. If not there, look in the super class.
    bool JS2Metadata::findStaticMember(JS2Class *c, Multiname *multiname, Access access, Phase /* phase */, MemberDescriptor *result)
    {
        result->staticMember = NULL;
        result->qname = NULL;
        JS2Class *s = c;
        while (s) {
            StaticBindingIterator b, end;
            if (access & ReadAccess) {
                b = s->staticReadBindings.lower_bound(*multiname->name);
                end = s->staticReadBindings.upper_bound(*multiname->name);
            }
            else {
                b = s->staticWriteBindings.lower_bound(*multiname->name);
                end = s->staticWriteBindings.upper_bound(*multiname->name);
            }
            StaticMember *found = NULL;
            while (b != end) {
                if (multiname->matches(b->second->qname)) {
                    if (found && (b->second->content != found))
                        reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                    else
                        found = b->second->content;
                }
                b++;
            }
            if (found) {
                result->staticMember = found;
                result->qname = NULL;
                return true;
            }

            InstanceBinding *iFound = NULL;
            InstanceBindingIterator ib, iend;
            if (access & ReadAccess) {
                ib = s->instanceReadBindings.lower_bound(*multiname->name);
                iend = s->instanceReadBindings.upper_bound(*multiname->name);
            }
            else {
                ib = s->instanceWriteBindings.lower_bound(*multiname->name);
                iend = s->instanceWriteBindings.upper_bound(*multiname->name);
            }
            while (ib != iend) {
                if (multiname->matches(ib->second->qname)) {
                    if (iFound && (ib->second->content != iFound->content))
                        reportError(Exception::propertyAccessError, "Ambiguous reference to {0}", engine->errorPos(), multiname->name);
                    else
                        iFound = ib->second;
                }
                ib++;
            }
            if (iFound) {
                result->staticMember = NULL;
                result->qname = &iFound->qname;
                return true;
            }
            s = s->super;
        }
        return false;
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
            b = c->instanceReadBindings.lower_bound(*multiname->name);
            end = c->instanceReadBindings.upper_bound(*multiname->name);
        }
        else {
            b = c->instanceWriteBindings.lower_bound(*multiname->name);
            end = c->instanceWriteBindings.upper_bound(*multiname->name);
        }
        while (true) {
            if (b == end) {
                if (access == ReadWriteAccess) {
                    access = WriteAccess;
                    b = c->instanceWriteBindings.lower_bound(*multiname->name);
                    end = c->instanceWriteBindings.upper_bound(*multiname->name);
                    continue;
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

    // gc-mark all contained JS2Objects and their children 
    // and then invoke mark on all other structures that contain JS2Objects
    void JS2Metadata::mark()
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
        GCMARKOBJECT(prototypeClass);
        GCMARKOBJECT(packageClass);
        GCMARKOBJECT(dateClass);
        GCMARKOBJECT(regexpClass);
        GCMARKOBJECT(mathClass);

        for (BConListIterator i = bConList.begin(), end = bConList.end(); (i != end); i++)
            (*i)->mark();
        if (bCon)
            bCon->mark();
        if (engine)
            engine->mark();
        env.mark();

        GCMARKOBJECT(glob);
    
    }

    // x is not a String
    const String *JS2Metadata::convertValueToString(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return engine->undefined_StringAtom;
        if (JS2VAL_IS_NULL(x))
            return engine->null_StringAtom;
        if (JS2VAL_IS_BOOLEAN(x))
            return (JS2VAL_TO_BOOLEAN(x)) ? engine->true_StringAtom : engine->false_StringAtom;
        if (JS2VAL_IS_INT(x))
            return numberToString(JS2VAL_TO_INT(x));
        if (JS2VAL_IS_LONG(x)) {
            float64 d;
            JSLL_L2D(d, *JS2VAL_TO_LONG(x));
            return numberToString(&d);
        }
        if (JS2VAL_IS_ULONG(x)) {
            float64 d;
            JSLL_UL2D(d, *JS2VAL_TO_ULONG(x));
            return numberToString(&d);
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 d = *JS2VAL_TO_FLOAT(x);
            return numberToString(&d);
        }
        if (JS2VAL_IS_DOUBLE(x))
            return numberToString(JS2VAL_TO_DOUBLE(x));
        return toString(toPrimitive(x));
    }

    // x is not a primitive (it is an object and not null)
    js2val JS2Metadata::convertValueToPrimitive(js2val x)
    {
        // return [[DefaultValue]] --> get property 'toString' and invoke it, 
        // if not available or result is not primitive then try property 'valueOf'
        // if that's not available or returns a non primitive, throw a TypeError

        Multiname mn(engine->toString_StringAtom, publicNamespace);
        LookupKind lookup(false, JS2VAL_NULL);
        js2val result;
        if (readProperty(x, &mn, &lookup, RunPhase, &result)) {
            if (JS2VAL_IS_OBJECT(result)) {
                JS2Object *obj = JS2VAL_TO_OBJECT(result);
                if ((obj->kind == FixedInstanceKind) && (objectType(result) == functionClass)) {
                    FunctionWrapper *fWrap = (checked_cast<FixedInstance *>(obj))->fWrap;
                    if (fWrap->code) {
                        result = (fWrap->code)(this, result, NULL, 0);
                        return result;
                    }
                }
                else
                if (obj->kind == MethodClosureKind) {
                    MethodClosure *mc = checked_cast<MethodClosure *>(obj);
                    FixedInstance *fInst = mc->method->fInst;
                    FunctionWrapper *fWrap = fInst->fWrap;
                    if (fWrap->code) {
                        result = (fWrap->code)(this, mc->thisObject, NULL, 0);
                        return result;
                    }
                }
            }
        }

        return STRING_TO_JS2VAL(engine->object_StringAtom);
    }

    // x is not a number
    float64 JS2Metadata::convertValueToDouble(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return nan;
        if (JS2VAL_IS_NULL(x))
            return 0;
        if (JS2VAL_IS_BOOLEAN(x))
            return (JS2VAL_TO_BOOLEAN(x)) ? 1.0 : 0.0;
        if (JS2VAL_IS_STRING(x)) {
            String *str = JS2VAL_TO_STRING(x);
            const char16 *numEnd;
            return stringToDouble(str->data(), str->data() + str->length(), numEnd);
        }
        if (JS2VAL_IS_INACCESSIBLE(x))
            reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
        if (JS2VAL_IS_UNINITIALIZED(x))
            reportError(Exception::compileExpressionError, "Inappropriate compile time expression", engine->errorPos());
        return toFloat64(toPrimitive(x));
    }

    // x is not a number, convert it to one
    js2val JS2Metadata::convertValueToGeneralNumber(js2val x)
    {
        // XXX Assuming convert to float64, rather than long/ulong
        return engine->allocNumber(toFloat64(x));
    }

    // x is not an Object, it needs to be wrapped in one
    js2val JS2Metadata::convertValueToObject(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x) || JS2VAL_IS_NULL(x) || JS2VAL_IS_SPECIALREF(x))
            reportError(Exception::typeError, "Can't convert to Object", engine->errorPos());
        if (JS2VAL_IS_STRING(x))
            return String_Constructor(this, JS2VAL_NULL, &x, 1);
        // XXX need more
        return OBJECT_TO_JS2VAL(new PrototypeInstance(objectClass->prototype, objectClass));
    }
    
    // x is any js2val
    float64 JS2Metadata::toFloat64(js2val x)
    { 
        if (JS2VAL_IS_INT(x)) 
            return JS2VAL_TO_INT(x); 
        else
        if (JS2VAL_IS_DOUBLE(x)) 
            return *JS2VAL_TO_DOUBLE(x); 
        else
        if (JS2VAL_IS_LONG(x)) {
            float64 d;
            JSLL_L2D(d, *JS2VAL_TO_LONG(x));
            return d;
        }
        else
        if (JS2VAL_IS_ULONG(x)) {
            float64 d;
            JSLL_UL2D(d, *JS2VAL_TO_ULONG(x));
            return d; 
        }
        else
        if (JS2VAL_IS_FLOAT(x))
            return *JS2VAL_TO_FLOAT(x);
        else 
            return convertValueToDouble(x); 
    }

    // x is not a bool
    bool JS2Metadata::convertValueToBoolean(js2val x)
    {
        if (JS2VAL_IS_UNDEFINED(x))
            return false;
        if (JS2VAL_IS_NULL(x))
            return false;
        if (JS2VAL_IS_INT(x))
            return (JS2VAL_TO_INT(x) != 0);
        if (JS2VAL_IS_LONG(x) || JS2VAL_IS_ULONG(x))
            return (!JSLL_IS_ZERO(x));
        if (JS2VAL_IS_FLOAT(x)) {
            float64 xd = *JS2VAL_TO_FLOAT(x);
            return ! (JSDOUBLE_IS_POSZERO(xd) || JSDOUBLE_IS_NEGZERO(xd) || JSDOUBLE_IS_NaN(xd));
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 xd = *JS2VAL_TO_DOUBLE(x);
            return ! (JSDOUBLE_IS_POSZERO(xd) || JSDOUBLE_IS_NEGZERO(xd) || JSDOUBLE_IS_NaN(xd));
        }
        if (JS2VAL_IS_STRING(x)) {
            String *str = JS2VAL_TO_STRING(x);
            return (str->length() != 0);
        }
        return true;
    }

    // x is not an int
    int32 JS2Metadata::convertValueToInteger(js2val x)
    {
        int32 i;
        if (JS2VAL_IS_LONG(x)) {
            JSLL_L2I(i, *JS2VAL_TO_LONG(x));
            return i;
        }
        if (JS2VAL_IS_ULONG(x)) {
            JSLL_UL2I(i, *JS2VAL_TO_ULONG(x));
            return i;
        }
        if (JS2VAL_IS_FLOAT(x)) {
            float64 f = *JS2VAL_TO_FLOAT(x);
            return JS2Engine::float64toInt32(f);
        }
        if (JS2VAL_IS_DOUBLE(x)) {
            float64 d = *JS2VAL_TO_DOUBLE(x);
            return JS2Engine::float64toInt32(d);
        }
        float64 d = convertValueToDouble(x);
        return JS2Engine::float64toInt32(d);
    }

    // Save off info about the current compilation and begin a
    // new one - using the given parser.
    CompilationData *JS2Metadata::startCompilationUnit(BytecodeContainer *newBCon, const String &source, const String &sourceLocation)
    {
        CompilationData *result = new CompilationData();
        result->bCon = bCon;
        bConList.push_back(bCon);

        if (newBCon)
            bCon = newBCon;
        else
            bCon = new BytecodeContainer();
        bCon->mSource = source;
        bCon->mSourceLocation = sourceLocation;
        engine->bCon = bCon;

        return result;
    }

    // Restore the compilation data, and then delete the cached copy.
    void JS2Metadata::restoreCompilationUnit(CompilationData *oldData)
    {
        BytecodeContainer *xbCon = bConList.back();
        ASSERT(oldData->bCon == xbCon);
        bConList.pop_back();

        bCon = oldData->bCon;

        delete oldData;
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

 /************************************************************************************
 *
 *  JS2Class
 *
 ************************************************************************************/


    JS2Class::JS2Class(JS2Class *super, JS2Object *proto, Namespace *privateNamespace, bool dynamic, bool allowNull, bool final, const String *name) 
        : Frame(ClassKind), 
            instanceInitOrder(NULL), 
            complete(false), 
            super(super), 
            prototype(proto), 
            privateNamespace(privateNamespace), 
            dynamic(dynamic),
            allowNull(allowNull),
            final(final),
            call(NULL),
            construct(JS2Engine::defaultConstructor),
            slotCount(super ? super->slotCount : 0),
            name(name)
    {

    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void JS2Class::markChildren()
    {
        Frame::markChildren();
        GCMARKOBJECT(super)
        GCMARKOBJECT(prototype)
        GCMARKOBJECT(privateNamespace)
        InstanceBindingIterator ib, iend;
        for (ib = instanceReadBindings.begin(), iend = instanceReadBindings.end(); (ib != iend); ib++) {
            ib->second->content->mark();
        }        
        for (ib = instanceWriteBindings.begin(), iend = instanceWriteBindings.end(); (ib != iend); ib++) {
            ib->second->content->mark();
        }        
    }

 /************************************************************************************
 *
 *  DynamicInstance
 *
 ************************************************************************************/

    // Construct a dynamic instance of a class. Set the
    // initial value of all slots to uninitialized.
    DynamicInstance::DynamicInstance(JS2Class *type) 
        : JS2Object(DynamicInstanceKind), 
            type(type), 
            fWrap(NULL),
            call(NULL), 
            construct(NULL), 
            env(NULL), 
            typeofString(type->getName()),
            slots(new Slot[type->slotCount])
    {
        for (uint32 i = 0; i < type->slotCount; i++) {
            slots[i].value = JS2VAL_UNINITIALIZED;
        }
    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void DynamicInstance::markChildren()
    {
        GCMARKOBJECT(type)
        if (fWrap) {
            GCMARKOBJECT(fWrap->compileFrame);
            if (fWrap->bCon)
                fWrap->bCon->mark();
        }
        if (slots) {
            ASSERT(type);
            for (uint32 i = 0; (i < type->slotCount); i++) {
                GCMARKVALUE(slots[i].value);
            }
        }
        for (DynamicPropertyIterator i = dynamicProperties.begin(), end = dynamicProperties.end(); (i != end); i++) {
            GCMARKVALUE(i->second);
        }        
    }



/************************************************************************************
 *
 *  FixedInstance
 *
 ************************************************************************************/

    
    // Construct a fixed instance of a class. Set the
    // initial value of all slots to uninitialized.
    FixedInstance::FixedInstance(JS2Class *type) 
        : JS2Object(FixedInstanceKind), 
            type(type), 
            fWrap(NULL),
            call(NULL), 
            construct(NULL), 
            env(NULL), 
            typeofString(type->getName()),
            slots(new Slot[type->slotCount])
    {
        for (uint32 i = 0; i < type->slotCount; i++) {
            slots[i].value = JS2VAL_UNINITIALIZED;
        }

    }

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void FixedInstance::markChildren()
    {
        GCMARKOBJECT(type)
        if (fWrap) {
            GCMARKOBJECT(fWrap->compileFrame);
            if (fWrap->bCon)
                fWrap->bCon->mark();
        }
        if (slots) {
            ASSERT(type);
            for (uint32 i = 0; (i < type->slotCount); i++) {
                GCMARKVALUE(slots[i].value);
            }
        }
    }


 /************************************************************************************
 *
 *  PrototypeInstance
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void PrototypeInstance::markChildren()
    {
        GCMARKOBJECT(parent)
        for (DynamicPropertyIterator i = dynamicProperties.begin(), end = dynamicProperties.end(); (i != end); i++) {
            GCMARKVALUE(i->second);
        }        
    }


/************************************************************************************
 *
 *  Frame
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

    // Allocate a new temporary variable in this frame and stick it
    // on the list (which may need to be created) for gc tracking.
    uint16 Frame::allocateTemp()
    {
        if (temps == NULL)
            temps = new std::vector<js2val>;
        uint16 result = (uint16)(temps->size());
        temps->push_back(JS2VAL_VOID);
        return result;
    }


    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void Frame::markChildren()
    {
        GCMARKOBJECT(nextFrame)
        GCMARKOBJECT(pluralFrame)
        StaticBindingIterator sbi, end;
        for (sbi = staticReadBindings.begin(), end = staticReadBindings.end(); (sbi != end); sbi++) {
            sbi->second->content->mark();
        }
        for (sbi = staticWriteBindings.begin(), end = staticWriteBindings.end(); (sbi != end); sbi++) {
            sbi->second->content->mark();
        }
        if (temps) {
            for (std::vector<js2val>::iterator i = temps->begin(), end = temps->end(); (i != end); i++)
                GCMARKVALUE(*i);
        }
    }


 /************************************************************************************
 *
 *  GlobalObject
 *
 ************************************************************************************/

    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void GlobalObject::markChildren()
    {
        Frame::markChildren();
        GCMARKOBJECT(internalNamespace)
        for (DynamicPropertyIterator i = dynamicProperties.begin(), end = dynamicProperties.end(); (i != end); i++) {
            GCMARKVALUE(i->second);
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
    void ParameterFrame::assignArguments(js2val *argBase, uint32 argCount)
    {
        ASSERT(pluralFrame->kind == ParameterKind);
        ParameterFrame *plural = checked_cast<ParameterFrame *>(pluralFrame);
        ASSERT((plural->positionalCount == 0) || (plural->positional != NULL));
        for (uint32 i = 0; ((i < argCount) && (i < plural->positionalCount)); i++) {
            ASSERT(plural->positional[i]->cloneContent);
            ASSERT(plural->positional[i]->cloneContent->kind == Member::Variable);
            (checked_cast<Variable *>(plural->positional[i]->cloneContent))->value = argBase[i];
        }
    }


    // gc-mark all contained JS2Objects and visit contained structures to do likewise
    void ParameterFrame::markChildren()
    {
        Frame::markChildren();
        GCMARKVALUE(thisObject);
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
        GCMARKOBJECT(type);
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
    std::list<PondScum **> JS2Object::rootList;

    // Add a pointer to a gc-allocated object to the root list
    // (Note - we hand out an iterator, so it's essential to
    // use something like std::list that doesn't mess with locations)
    JS2Object::RootIterator JS2Object::addRoot(void *t)
    {
        PondScum **p = (PondScum **)t;
        ASSERT(p);
        return rootList.insert(rootList.end(), p);
    }

    // Remove a root pointer
    void JS2Object::removeRoot(RootIterator ri)
    {
        rootList.erase(ri);
    }

    // Mark all reachable objects and put the rest back on the freelist
    void JS2Object::gc(JS2Metadata *meta)
    {
        pond.resetMarks();
        // Anything on the root list is a pointer to a JS2Object.
        for (std::list<PondScum **>::iterator i = rootList.begin(), end = rootList.end(); (i != end); i++) {
            if (**i) {
                PondScum *p = (**i) - 1;
                ASSERT(p->owner && (p->getSize() >= sizeof(PondScum)) && (p->owner->sanity == POND_SANITY));
                JS2Object *obj = (JS2Object *)(p + 1);
                GCMARKOBJECT(obj)
            }
        }
        meta->mark();
        pond.moveUnmarkedToFreeList();
    }

    // Allocate a chunk of size s
    void *JS2Object::alloc(size_t s)
    {
        s += sizeof(PondScum);
        // make sure that the thing is 16-byte aligned
        if (s & 0xF) s += 16 - (s & 0xF);
        ASSERT(s <= 0x7FFFFFFF);
        void *p = pond.allocFromPond(s);
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
    void *Pond::allocFromPond(size_t sz)
    {
        // Try scannning the free list...
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
#ifdef DEBUG
                memset((p + 1), 0xB7, p->getSize() - sizeof(PondScum));
#endif
                return (p + 1);
            }
            pre = p;
            p = (PondScum *)(p->owner);
        }

        // See if there's room left...
        if (sz > pondSize) {
            if (nextPond == NULL)
                nextPond = new Pond(sz, nextPond);
            return nextPond->allocFromPond(sz);
        }
        p = (PondScum *)pondTop;
        p->owner = this;
        p->setSize(sz);
        pondTop += sz;
        pondSize -= sz;
#ifdef DEBUG
        memset((p + 1), 0xB7, sz - sizeof(PondScum));
#endif
        return (p + 1);
    }

    // Stick the chunk at the start of the free list
    void Pond::returnToPond(PondScum *p)
    {
        p->owner = (Pond *)freeHeader;
        uint8 *t = (uint8 *)(p + 1);
#ifdef DEBUG
        memset(t, 0xB3, p->getSize() - sizeof(PondScum));
#endif
        freeHeader = p;
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
    void Pond::moveUnmarkedToFreeList()
    {
        uint8 *t = pondBottom;
        while (t != pondTop) {
            PondScum *p = (PondScum *)t;
            if (!p->isMarked() && (p->owner == this))   // (owner != this) ==> already on free list
                returnToPond(p);
            t += p->getSize();
        }
        if (nextPond)
            nextPond->moveUnmarkedToFreeList();
    }



}; // namespace MetaData
}; // namespace Javascript
