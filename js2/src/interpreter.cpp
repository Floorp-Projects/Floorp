// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 2000 Netscape Communications Corporation. All
// Rights Reserved.

#include "interpreter.h"
#include "world.h"

#include <map>
#include <stack>

namespace JavaScript {

using std::map;
using std::less;
using std::pair;

#if defined(XP_MAC)
    // copied from default template parameters in map.
    typedef gc_allocator<pair<const String, JSValue> > gc_map_allocator;
#elif defined(XP_UNIX)
    // FIXME: in libg++, they assume the map's allocator is a byte allocator,
    // which is wrapped in a simple_allocator. this is crap.
    typedef char _Char[1];
    typedef gc_allocator<_Char> gc_map_allocator;
#elif defined(_WIN32)
    // FIXME: MSVC++'s notion. this is why we had to add _Charalloc().
    typedef gc_allocator<JSValue> gc_map_allocator;
#endif

class JSMap {
    map<String, JSValue, less<String>, gc_map_allocator> properties;
public:
     JSValue& operator[](const String& name)
    {
        return properties[name];
    }
};
 
/**
 * Private representation of a JavaScript object.
 * This will change over time, so it is treated as an opaque
 * type everywhere else but here.
 */
class JSObject : public JSMap, public gc_object<JSObject> {};

/**
 * Private representation of a JavaScript array.
 */
class JSArray : public JSMap, public gc_object<JSArray> {
    JSValues elements;
public:
    JSArray() : elements(1) {}
    JSArray(uint32 size) : elements(size) {}
    JSArray(const JSValues &v) : elements(v) {}

    uint32 length()
    {
        return elements.size();
    }
    
    JSValue& operator[](const JSValue& index)
    {
        // for now, we can only handle f64 index values.
        uint32 n = (uint32)index.f64;
        // obviously, a sparse representation might be better.
        uint32 size = elements.size();
        if (n >= size) expand(n, size);
        return elements[n];
    }
    
    JSValue& operator[](uint32 n)
    {
        // obviously, a sparse representation might be better.
        uint32 size = elements.size();
        if (n >= size) expand(n, size);
        return elements[n];
    }
    
    void resize(uint32 size)
    {
        elements.resize(size);
    }

private:
    void expand(uint32 n, uint32 size)
    {
        do {
            size *= 2;
        } while (n >= size);
        elements.resize(size);
    }
};

/**
 * Stores saved state from the *previous* activation, the current activation is alive
 * and well in locals of the interpreter loop.
 */
struct JSFrame : public gc_object<JSFrame> {
    JSFrame(InstructionIterator returnPC, InstructionIterator basePC, JSArray *registers, JSArray *variables, Register result) 
            : itsReturnPC(returnPC), itsBasePC(basePC), itsRegisters(registers), itsVariables(variables), itsResult(result) { }

    InstructionIterator itsReturnPC;
    InstructionIterator itsBasePC;

    JSArray *itsRegisters;             // rather not save these BUT:
                                       // - switch temps (and others eventually?) are live across statments
                                       // and need to be preserved.
                                       // - better debugging if intermediate state can be recovered (?)
    JSArray *itsVariables;            
    Register itsResult;                // the desired target register for the return value

};

// a stack of JSFrames.
typedef std::stack<JSFrame*, std::vector<JSFrame*, gc_allocator<JSFrame*> > > JSFrameStack;

// operand access macros.
#define op1(i) (i->itsOperand1)
#define op2(i) (i->itsOperand2)
#define op3(i) (i->itsOperand3)

// mnemonic names for operands.
#define dst(i) op1(i)
#define src1(i) op2(i)
#define src2(i) op3(i)

static JSObject globals;

JSValue& defineGlobalProperty(const String& name, const JSValue& value)
{
    return (globals[name] = value);
}

JSValue interpret(ICodeModule* iCode, const JSValues& args)
{
    // stack of JSFrames.
    JSFrameStack frames;

    JSArray *locals = new JSArray(args);
    // ensure that locals array is large enough.
    uint32 localsSize = iCode->itsMaxVariable + 1;
    if (localsSize > locals->length())
        locals->resize(localsSize);

    JSArray *registers = new JSArray(iCode->itsMaxRegister + 1);

    InstructionIterator begin_pc = iCode->its_iCode->begin();
    InstructionIterator pc = begin_pc;

    while (true) {
        Instruction* instruction = *pc;
        switch (instruction->opcode()) {
        case CALL:
            {
                Call* call = static_cast<Call*>(instruction);
                JSArray *previousRegisters = registers;
                frames.push(new JSFrame(++pc, begin_pc, registers, locals, op1(call)));
                ICodeModule *tgt = (*registers)[op2(call)].icm;
                locals = new JSArray(tgt->itsMaxVariable + 1);
                registers = new JSArray(tgt->itsMaxRegister + 1);
                RegisterList args = op3(call);
                for (RegisterList::iterator r = args.begin(); r != args.end(); ++r)
                    (*locals)[r - args.begin()] = (*previousRegisters)[(*r)];
                begin_pc = pc = tgt->its_iCode->begin();
            }
            continue;
        case RETURN:
            {
                Return* ret = static_cast<Return*>(instruction);
                JSValue result;
                if (op1(ret) != NotARegister) 
                    result = (*registers)[op1(ret)];
                if (frames.empty())
                    return result;
                JSFrame *fr = frames.top();
                frames.pop();
                registers = fr->itsRegisters;
                locals = fr->itsVariables;
                (*registers)[fr->itsResult] = result;
                pc = fr->itsReturnPC;
                begin_pc = fr->itsBasePC;
            }
            continue;
        case MOVE_TO:
            {
                Move* mov = static_cast<Move*>(instruction);
                (*registers)[dst(mov)] = (*registers)[src1(mov)];
            }
            break;
        case LOAD_NAME:
            {
                LoadName* ln = static_cast<LoadName*>(instruction);
                (*registers)[dst(ln)] = globals[*src1(ln)];
            }
            break;
        case SAVE_NAME:
            {
                SaveName* sn = static_cast<SaveName*>(instruction);
                globals[*dst(sn)] = (*registers)[src1(sn)];
            }
            break;
        case NEW_OBJECT:
            {
                NewObject* no = static_cast<NewObject*>(instruction);
                (*registers)[dst(no)].object = new JSObject();
            }
            break;
        case NEW_ARRAY:
            {
                NewArray* na = static_cast<NewArray*>(instruction);
                (*registers)[dst(na)].array = new JSArray();
            }
            break;
        case GET_PROP:
            {
                GetProp* gp = static_cast<GetProp*>(instruction);
                JSObject* object = (*registers)[src1(gp)].object;
                (*registers)[dst(gp)] = (*object)[*src2(gp)];
            }
            break;
        case SET_PROP:
            {
                SetProp* sp = static_cast<SetProp*>(instruction);
                JSObject* object = (*registers)[dst(sp)].object;
                (*object)[*src1(sp)] = (*registers)[src2(sp)];
            }
            break;
        case GET_ELEMENT:
            {
                GetElement* ge = static_cast<GetElement*>(instruction);
                JSArray* array = (*registers)[src1(ge)].array;
                (*registers)[dst(ge)] = (*array)[(*registers)[src2(ge)]];
            }
            break;
        case SET_ELEMENT:
            {
                SetElement* se = static_cast<SetElement*>(instruction);
                JSArray* array = (*registers)[dst(se)].array;
                (*array)[(*registers)[src1(se)]] = (*registers)[src2(se)];
            }
            break;
        case LOAD_IMMEDIATE:
            {
                LoadImmediate* li = static_cast<LoadImmediate*>(instruction);
                (*registers)[dst(li)] = JSValue(src1(li));
            }
            break;
        case LOAD_VAR:
            {
                LoadVar* lv = static_cast<LoadVar*>(instruction);
                (*registers)[dst(lv)] = (*locals)[src1(lv)];
            }
            break;
        case SAVE_VAR:
            {
                SaveVar* sv = static_cast<SaveVar*>(instruction);
                (*locals)[dst(sv)] = (*registers)[src1(sv)];
            }
            break;
        case BRANCH:
            {
                ResolvedBranch* bra = static_cast<ResolvedBranch*>(instruction);
                pc = begin_pc + dst(bra);
                continue;
            }
            break;
        case BRANCH_LT:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if ((*registers)[src1(bc)].i32 < 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_LE:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if ((*registers)[src1(bc)].i32 <= 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_EQ:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if ((*registers)[src1(bc)].i32 == 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_NE:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if ((*registers)[src1(bc)].i32 != 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_GE:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if ((*registers)[src1(bc)].i32 >= 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_GT:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if ((*registers)[src1(bc)].i32 > 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case ADD:
            {
                // could get clever here with Functional forms.
                Arithmetic* add = static_cast<Arithmetic*>(instruction);
                (*registers)[dst(add)] = JSValue((*registers)[src1(add)].f64 + (*registers)[src2(add)].f64);
            }
            break;
        case SUBTRACT:
            {
                Arithmetic* sub = static_cast<Arithmetic*>(instruction);
                (*registers)[dst(sub)] = JSValue((*registers)[src1(sub)].f64 - (*registers)[src2(sub)].f64);
            }
            break;
        case MULTIPLY:
            {
                Arithmetic* mul = static_cast<Arithmetic*>(instruction);
                (*registers)[dst(mul)] = JSValue((*registers)[src1(mul)].f64 * (*registers)[src2(mul)].f64);
            }
            break;
        case DIVIDE:
            {
                Arithmetic* div = static_cast<Arithmetic*>(instruction);
                (*registers)[dst(div)] = JSValue((*registers)[src1(div)].f64 / (*registers)[src2(div)].f64);
            }
            break;
        case COMPARE_LT:
        case COMPARE_LE:
        case COMPARE_EQ:
        case COMPARE_NE:
        case COMPARE_GT:
        case COMPARE_GE:
            {
                Arithmetic* cmp = static_cast<Arithmetic*>(instruction);
                float64 diff = ((*registers)[src1(cmp)].f64 - (*registers)[src2(cmp)].f64);
                (*registers)[dst(cmp)].i32 = (diff == 0.0 ? 0 : (diff > 0.0 ? 1 : -1));
            }
            break;
        case NOT:
            {
                Move* nt = static_cast<Move*>(instruction);
                (*registers)[dst(nt)].i32 = !(*registers)[src1(nt)].i32;
            }
            break;
        default:
            NOT_REACHED("bad opcode");
            break;
        }
        
        // increment the program counter.
        ++pc;
    }

}

}
