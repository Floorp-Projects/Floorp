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
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.

#include "interpreter.h"
#include "world.h"

#include <map>

namespace JavaScript {

using std::map;
using std::less;
using std::pair;

/**
 * Private representation of a JavaScript object.
 * This will change over time, so it is treated as an opaque
 * type everywhere else but here.
 */

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

class JSObject : public map<String, JSValue, less<String>, gc_map_allocator> {
public:
    void* operator new(size_t) { return alloc.allocate(1, 0); }
    void operator delete(void* /* ptr */) {}
private:
    static gc_allocator<JSObject> alloc;
};

/**
 * Private representation of a JavaScript array.
 */
class JSArray : public JSObject {
public:
    void* operator new(size_t) { return alloc.allocate(1, 0); }
    uint32 length() { return elements.size(); }
    JSValue& operator[](uint32 n)
    {
        // obviously, a sparse representation might be better.
        uint32 size = elements.size();
        if (n >= size) resize(n, size);
        return elements[n];
    }
private:
    void resize(uint32 n, uint32 size)
    {
        do {
            size *= 2;
        } while (n >= size);
        elements.resize(size);
    }
private:
    JSValues elements;
    static gc_allocator<JSArray> alloc;
};

// static allocator (required when gc_allocator<T> is allocator<T>.
gc_allocator<JSObject> JSObject::alloc;
gc_allocator<JSArray> JSArray::alloc;

// operand access macros.
#define op1(i) (i->itsOperand1)
#define op2(i) (i->itsOperand2)
#define op3(i) (i->itsOperand3)

// mnemonic names for operands.
#define dst(i) op1(i)
#define src1(i) op2(i)
#define src2(i) op3(i)

JSValue interpret(ICodeModule *iCode, const JSValues& args)
{
    // fake global variables object.
    static JSObject globals;

    JSValue result;
    JSValues frame(args);
    JSValues registers(iCode->itsMaxRegister + 1);

    // ensure that frame is large enough.
    uint32 frameSize = iCode->itsMaxVariable + 1;
    if (frameSize > frame.size())
        frame.resize(frameSize);
    
    InstructionIterator begin_pc = iCode->its_iCode->begin();
    InstructionIterator end_pc = iCode->its_iCode->end();
    InstructionIterator pc = begin_pc;
    while (pc != end_pc) {
        Instruction* instruction = *pc;
        switch (instruction->opcode()) {
        case MOVE_TO:
            {
                Move* mov = static_cast<Move*>(instruction);
                registers[dst(mov)] = registers[src1(mov)];
            }
            break;
        case LOAD_NAME:
            {
                LoadName* ln = static_cast<LoadName*>(instruction);
                registers[dst(ln)] = globals[*src1(ln)];
            }
            break;
        case SAVE_NAME:
            {
                SaveName* sn = static_cast<SaveName*>(instruction);
                globals[*dst(sn)] = registers[src1(sn)];
            }
            break;
        case NEW_OBJECT:
            {
                NewObject* no = static_cast<NewObject*>(instruction);
                registers[dst(no)].object = new JSObject();
            }
            break;
        case NEW_ARRAY:
            {
                NewArray* na = static_cast<NewArray*>(instruction);
                registers[dst(na)].array = new JSArray();
            }
            break;
        case GET_PROP:
            {
                GetProp* gp = static_cast<GetProp*>(instruction);
                JSObject* object = registers[src1(gp)].object;
                registers[dst(gp)] = (*object)[*src2(gp)];
            }
            break;
        case SET_PROP:
            {
                SetProp* sp = static_cast<SetProp*>(instruction);
                JSObject* object = registers[dst(sp)].object;
                (*object)[*src1(sp)] = registers[src2(sp)];
            }
            break;
        case GET_ELEMENT:
            {
                GetElement* ge = static_cast<GetElement*>(instruction);
                JSArray* array = registers[src1(ge)].array;
                registers[dst(ge)] = (*array)[src2(ge)];
            }
            break;
        case SET_ELEMENT:
            {
                SetElement* se = static_cast<SetElement*>(instruction);
                JSArray* array = registers[dst(se)].array;
                (*array)[src1(se)] = registers[src2(se)];
            }
            break;
        case LOAD_IMMEDIATE:
            {
                LoadImmediate* li = static_cast<LoadImmediate*>(instruction);
                registers[dst(li)] = JSValue(src1(li));
            }
            break;
        case LOAD_VAR:
            {
                LoadVar* lv = static_cast<LoadVar*>(instruction);
                registers[dst(lv)] = frame[src1(lv)];
            }
            break;
        case SAVE_VAR:
            {
                SaveVar* sv = static_cast<SaveVar*>(instruction);
                frame[dst(sv)] = registers[src1(sv)];
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
                if (registers[src1(bc)].i32 < 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_LE:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if (registers[src1(bc)].i32 <= 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_EQ:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if (registers[src1(bc)].i32 == 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_NE:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if (registers[src1(bc)].i32 != 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_GE:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if (registers[src1(bc)].i32 >= 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case BRANCH_GT:
            {
                ResolvedBranchCond* bc = static_cast<ResolvedBranchCond*>(instruction);
                if (registers[src1(bc)].i32 > 0) {
                    pc = begin_pc + dst(bc);
                    continue;
                }
            }
            break;
        case ADD:
            {
                // could get clever here with Functional forms.
                Arithmetic* add = static_cast<Arithmetic*>(instruction);
                registers[dst(add)] = JSValue(registers[src1(add)].f64 + registers[src2(add)].f64);
            }
            break;
        case SUBTRACT:
            {
                Arithmetic* sub = static_cast<Arithmetic*>(instruction);
                registers[dst(sub)] = JSValue(registers[src1(sub)].f64 - registers[src2(sub)].f64);
            }
            break;
        case MULTIPLY:
            {
                Arithmetic* mul = static_cast<Arithmetic*>(instruction);
                registers[dst(mul)] = JSValue(registers[src1(mul)].f64 * registers[src2(mul)].f64);
            }
            break;
        case DIVIDE:
            {
                Arithmetic* div = static_cast<Arithmetic*>(instruction);
                registers[dst(div)] = JSValue(registers[src1(div)].f64 / registers[src2(div)].f64);
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
                float64 diff = (registers[src1(cmp)].f64 - registers[src2(cmp)].f64);
                registers[dst(cmp)].i32 = (diff == 0.0 ? 0 : (diff > 0.0 ? 1 : -1));
            }
            break;
        case NOT:
            {
                Move* nt = static_cast<Move*>(instruction);
                registers[dst(nt)].i32 = !registers[src1(nt)].i32;
            }
            break;
        case RETURN:
            {
                Return* ret = static_cast<Return*>(instruction);
                result = registers[op1(ret)];
                return result;
            }
            break;
        default:
            break;
        }
        
        // increment the program counter.
        ++pc;
    }

    return result;
}

}
