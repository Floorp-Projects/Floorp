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

#ifndef bytecodegen_h___
#define bytecodegen_h___

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#endif


#include <vector>
#include <map>

#include "systemtypes.h"
#include "strings.h"

#include "tracer.h"

namespace JavaScript {
namespace JS2Runtime {

    typedef enum {
        // 1st 2 bits specify what kind of 'this' exists
        NoThis = 0x00,
        Inherent = 0x01,
        Explicit = 0x02,
        ThisFlags = 0x03,

        // bit #3 indicates presence of named arguments
        NamedArguments = 0x04,

        // but #4 is set for the invocation of the super constructor
        // from inside a constructor
        SuperInvoke = 0x08

    } CallFlag;

typedef enum {

    LoadConstantUndefinedOp,//                          --> <undefined value object>
    LoadConstantTrueOp,     //                          --> <true value object>
    LoadConstantFalseOp,    //                          --> <false value object>
    LoadConstantNullOp,     //                          --> <null value object>
    LoadConstantZeroOp,     //                          --> <+0.0 value object>
    LoadConstantNumberOp,   // <poolindex>              --> <Number value object>
    LoadConstantStringOp,   // <poolindex>              --> <String value object>
    LoadThisOp,             //                          --> <this object>      
    LoadFunctionOp,         // <pointer>        XXX !!! XXX
    LoadTypeOp,             // <pointer>        XXX !!! XXX
    InvokeOp,               // <argc> <thisflag>        <function> <args>  --> [<result>]
    GetTypeOp,              //                          <object> --> <type of object>
    CastOp,                 //                          <object> <type> --> <object>
    DoUnaryOp,              // <operation>              <object> --> <result>
    DoOperatorOp,           // <operation>              <object> <object> --> <result>
    PushNullOp,             //                          --> <Object(null)>
    PushIntOp,              // <int>                    --> <Object(int)>
    PushNumOp,              // <num>                    --> <Object(num)>
    PushStringOp,           // <poolindex>              --> <Object(index)>
    PushTypeOp,             // <poolindex>
    ReturnOp,               //                          <function> <args> <result> --> <result>
    ReturnVoidOp,           //                          <function> <args> -->
    GetConstructorOp,       //                          <type> --> <function> 
    NewObjectOp,            //                          --> <object>
    NewThisOp,              //                          <type> -->
    NewInstanceOp,          //  <argc>                  <type> <args> --> <object>
    DeleteOp,               //  <poolindex>             <object> --> <boolean>
    DeleteNameOp,           //  <poolindex>              --> <boolean>
    TypeOfOp,               //                          <object> --> <string>
    InstanceOfOp,           //                          <object> <object> --> <boolean>
    AsOp,                   //                          <object> <type> --> <object>
    IsOp,                   //                          <object> <object> --> <boolean>
    ToBooleanOp,            //                          <object> --> <boolean>
    JumpFalseOp,            // <target>                 <object> -->
    JumpTrueOp,             // <target>                 <object> -->
    JumpOp,                 // <target>            
    TryOp,                  // <handler> <handler>
    JsrOp,                  // <target>
    RtsOp,
    WithinOp,               //                          <object> -->
    WithoutOp,              //
    ThrowOp,                //                          <whatever> <object> --> <object>
    HandlerOp,
    LogicalXorOp,           //                          <object> <object> <boolean> <boolean> --> <object> 
    LogicalNotOp,           //                          <object> --> <object>
    SwapOp,                 //                          <object1> <object2> --> <object2> <object1>
    DupOp,                  //                          <object> --> <object> <object>
    DupInsertOp,            //                          <object1> <object2> --> <object2> <object1> <object2>
    DupNOp,                 // <N>                      <N things> --> <N things> <N things>
    DupInsertNOp,           // <N>                      <N things> <object2> --> <object2> <N things> <object2>
    PopOp,                  //                          <object> -->   
    PopNOp,                 // <N>                      <N things> -->   
    VoidPopOp,              //                          <object>-->     (doesn't cache result value)
    // for instance members
    GetFieldOp,             // <slot>                   <base> --> <object>
    SetFieldOp,             // <slot>                   <base> <object> --> <object>
    // for instance methods
    GetMethodOp,            // <slot>                   <base> --> <base> <function>
    GetMethodRefOp,         // <slot>                   <base> --> <bound function> 
    // for argumentz
    GetArgOp,               // <index>                  --> <object>
    SetArgOp,               // <index>                  <object> --> <object>
    // for local variables in the immediate scope
    GetLocalVarOp,          // <index>                  --> <object>
    SetLocalVarOp,          // <index>                  <object> --> <object>
    // for local variables in the nth closure scope
    GetClosureVarOp,        // <depth>, <index>         --> <object>
    SetClosureVarOp,        // <depth>, <index>         <object> --> <object>
    // for array elements
    GetElementOp,           // <dimcount>               <base> <index> ... <index> --> <object>
    SetElementOp,           // <dimcount>               <base> <index> ...<index> <object> --> <object>
    DeleteElementOp,        // <dimcount>               <base> <index> ... <index> --> <boolean>
    // for properties
    GetPropertyOp,          // <poolindex>              <base> --> <object>
    GetInvokePropertyOp,    // <poolindex>              <base> --> <base> <object> 
    SetPropertyOp,          // <poolindex>              <base> <object> --> <object>
    // for all generic names 
    GetNameOp,              // <poolindex>              --> <object>
    GetTypeOfNameOp,        // <poolindex>              --> <object>
    SetNameOp,              // <poolindex>              <object> --> <object>
    LoadGlobalObjectOp,     //                          --> <object>
    PushScopeOp,            // <pointer>        XXX !!! XXX
    PopScopeOp,             // <pointer>        XXX !!! XXX
    NewClosureOp,           //                          <function> --> <function>
    ClassOp,                //                          <object> --> <type>
    JuxtaposeOp,            //                          <attribute> <attribute> --> <attribute>
    NamedArgOp,             //                          <object> <string> --> <named arg object>

    OpCodeCount

} ByteCodeOp;

struct ByteCodeData {
    int8 stackImpact;
    char *opName;
};
extern ByteCodeData gByteCodeData[OpCodeCount];

    typedef std::pair<uint32, size_t> PC_Position;


    class ByteCodeModule {
    public:
            
        ByteCodeModule(ByteCodeGen *bcg, JSFunction *f);
        ~ByteCodeModule();

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("ByteCodeModule", s, t); return t; }
        void operator delete(void* t)   { trace_release("ByteCodeModule", t); STD::free(t); }
#endif

        uint32 getLong(uint32 index) const          { return *((uint32 *)&mCodeBase[index]); }
        uint16 getShort(uint32 index) const         { return *((uint16 *)&mCodeBase[index]); }
        int32 getOffset(uint32 index) const         { return *((int32 *)&mCodeBase[index]); }
        const String *getString(uint32 index) const { return (const String *)(index); }
        float64 getNumber(uint8 *p) const       { return *((float64 *)p); }

        void setSource(const String &source, const String &sourceLocation)
        {
            mSource = source;
            mSourceLocation = sourceLocation;
        }

        JSFunction *mFunction;

        String mSource;
        String mSourceLocation;

        uint32 mLocalsCount;        // number of local vars to allocate space for
        uint32 mStackDepth;         // max. depth of execution stack
        
        uint8 *mCodeBase;
        uint32 mLength;

        PC_Position *mCodeMap;
        uint32 mCodeMapLength;

        size_t getPositionForPC(uint32 pc);
            
    };
    Formatter& operator<<(Formatter& f, const ByteCodeModule& bcm);

    #define BufferIncrement (32)

#define NotALabel ((uint32)(-1))

    class Label {
    public:
        
        typedef enum { InternalLabel, NamedLabel, BreakLabel, ContinueLabel } LabelKind;

        Label() : mKind(InternalLabel), mHasLocation(false) { }
        Label(LabelStmtNode *lbl) : mKind(NamedLabel), mHasLocation(false), mLabelStmt(lbl) { }
        Label(LabelKind kind) : mKind(kind), mHasLocation(false) { }

        bool matches(const StringAtom *name)
        {
            return ((mKind == NamedLabel) && (mLabelStmt->name.compare(*name) == 0));
        }

        bool matches(LabelKind kind)
        {
            return (mKind == kind);
        }

        void addFixup(ByteCodeGen *bcg, uint32 branchLocation);        
        void setLocation(ByteCodeGen *bcg, uint32 location);

        std::vector<uint32> mFixupList;

        LabelKind mKind;
        bool mHasLocation;
        LabelStmtNode *mLabelStmt;

        uint32 mLocation;
    };

    class ByteCodeGen {
    public:

        ByteCodeGen(Context *cx, ScopeChain *scopeChain) 
            :   mBuffer(new CodeBuffer), 
                mScopeChain(scopeChain), 
                mPC_Map(new CodeMap),
                m_cx(cx), 
                mNamespaceList(NULL) ,
                mStackTop(0),
                mStackMax(0)
        { }

        ~ByteCodeGen()
        {
            delete mBuffer;
            delete mPC_Map;
        }

#ifdef DEBUG
        void* operator new(size_t s)    { void *t = STD::malloc(s); trace_alloc("ByteCodeGen", s, t); return t; }
        void operator delete(void* t)   { trace_release("ByteCodeGen", t); STD::free(t); }
#endif

        ByteCodeModule *genCodeForScript(StmtNode *p);
        bool genCodeForStatement(StmtNode *p, ByteCodeGen *static_cg, uint32 finallyLabel);
        void genCodeForFunction(FunctionDefinition &f, 
                                    size_t pos,
                                    JSFunction *fnc, 
                                    bool isConstructor, 
                                    JSType *topClass);
        ByteCodeModule *genCodeForExpression(ExprNode *p);

        JSType *genExpr(ExprNode *p);
        Reference *genReference(ExprNode *p, Access acc);
        void genReferencePair(ExprNode *p, Reference *&readRef, Reference *&writeRef);

        typedef std::vector<uint8> CodeBuffer;

        typedef std::vector<PC_Position> CodeMap;

        // this is the current code buffer
        CodeBuffer *mBuffer;
        ScopeChain *mScopeChain;
        CodeMap *mPC_Map;

        Context *m_cx;
        
        std::vector<Label> mLabelList;
        std::vector<uint32> mLabelStack;

        NamespaceList *mNamespaceList;

        int32 mStackTop;        // keep these as signed so as to
        int32 mStackMax;        // track if they go negative.

        bool hasContent()
        {
            return (mBuffer->size() > 0);
        }
       
        void addOp(uint8 op);       // XXX move more outline if it helps to reduce overall .exe size

        void addPosition(size_t pos)    { mPC_Map->push_back(PC_Position(mBuffer->size(), pos)); }

        // Add in the opcode effect as usual, but also stretch the
        // execution stack by N, as the opcode has that effect during
        // execution.
        void addOpStretchStack(uint8 op, int32 n)        
        {
            addByte(op);
            mStackTop += gByteCodeData[op].stackImpact;
            if ((mStackTop + n) > mStackMax)
                mStackMax = mStackTop + n;
            ASSERT(mStackTop >= 0);
        }

        void adjustStack(int32 n)
        {
            mStackTop += n;
            if ((mStackTop + n) > mStackMax)
                mStackMax = mStackTop + n;
            ASSERT(mStackTop >= 0);
        }

        // Make sure there's room for n more operands on the stack
        void stretchStack(int32 n)
        {
            if ((mStackTop + n) > mStackMax)
                mStackMax = mStackTop + n;
        }

        // these routines assume the depth is being reduced
        // i.e. they don't reset mStackMax
        void addOpAdjustDepth(uint8 op, int32 depth)        
                                    { addByte(op); mStackTop += depth; ASSERT(mStackTop >= 0); }
        void addOpSetDepth(uint8 op, int32 depth)        
                                    { addByte(op); mStackTop = depth; ASSERT(mStackTop >= 0); }

        void addByte(uint8 v)       { mBuffer->push_back(v); }
        void addShort(uint16 v)     { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint16)); }

        void addPointer(void *v)    { ASSERT(sizeof(void *) == sizeof(uint32)); addLong((uint32)(v)); }   // XXX Pointer size dependant !!!
        
        void addFloat64(float64 v)  { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(float64)); }
        void addLong(uint32 v)      { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(uint32)); }
        void addOffset(int32 v)     { mBuffer->insert(mBuffer->end(), (uint8 *)&v, (uint8 *)(&v) + sizeof(int32)); }
        void setOffset(uint32 index, int32 v) { *((int32 *)(mBuffer->begin() + index)) = v; }   // XXX dubious pointer usage

        void addFixup(uint32 label) 
        { 
            mLabelList[label].addFixup(this, mBuffer->size()); 
        }

        uint32 getLabel();

        uint32 getLabel(Label::LabelKind kind);

        uint32 getLabel(LabelStmtNode *lbl);

        uint32 getTopLabel(Label::LabelKind kind, const StringAtom *name, StmtNode *p);

        uint32 getTopLabel(Label::LabelKind kind, StmtNode *p);

        void setLabel(uint32 label)
        {
            mLabelList[label].setLocation(this, mBuffer->size()); 
        }

        uint32 currentOffset()
        {
            return mBuffer->size();
        }

        std::vector<String> mStringPoolContents;
        typedef std::map<String, uint32, std::less<String> > StringPool;
        StringPool mStringPool;

        std::vector<float64> mNumberPoolContents;
        typedef std::map<float64, uint32, std::less<double> > NumberPool;
        NumberPool mNumberPool;


        void addNumberRef(float64 f);
        
        void addStringRef(const String &str);


    };


    uint32 printInstruction(Formatter &f, uint32 i, const ByteCodeModule& bcm);
}
}
#endif /* bytecodegen_h___ */
