
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

#ifndef js2metadata_h___
#define js2metadata_h___


namespace JavaScript {
namespace MetaData {


// forward declarations:
class JS2Object;
class JS2Metadata;
class JS2Class;
class StaticBinding;
class Environment;
class Context;
class CompoundAttribute;
class BytecodeContainer;
class Pond;
class FixedInstance;


typedef void (Invokable)();
typedef Invokable Callor;
typedef js2val (Constructor)(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);

extern void initDateObject(JS2Metadata *meta);
extern void initStringObject(JS2Metadata *meta);
extern void initMathObject(JS2Metadata *meta);
extern void initArrayObject(JS2Metadata *meta);

extern js2val String_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val RegExp_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val RegExp_exec(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);

extern uint32 getLength(JS2Metadata *meta, JS2Object *obj);
extern js2val setLength(JS2Metadata *meta, JS2Object *obj, uint32 length);

// OBJECT is the semantic domain of all possible objects and is defined as:
// OBJECT = UNDEFINED | NULL | BOOLEAN | FLOAT64 | LONG | ULONG | CHARACTER | STRING | NAMESPACE |
// COMPOUNDATTRIBUTE | CLASS | METHODCLOSURE | PROTOTYPE | INSTANCE | PACKAGE | GLOBAL
//
//  In this implementation, the primitive types are distinguished by the tag value
// of a JS2Value (see js2value.h). Non-primitive types are distinguished by calling
// 'kind()' on the object to recover one of the values below:
//
enum ObjectKind { 
    AttributeObjectKind,
    SystemKind,                 
    GlobalObjectKind, 
    PackageKind, 
    ParameterKind, 
    ClassKind, 
    BlockKind, 
    PrototypeInstanceKind,
    FixedInstanceKind,
    DynamicInstanceKind,
    MultinameKind,
    MethodClosureKind,

    ForIteratorKind
};

class PondScum {
public:    
    void resetMark()        { size &= 0x7FFFFFFF; }
    void mark()             { size |= 0x80000000; }
    bool isMarked()         { return ((size & 0x80000000) != 0); }
    uint32 getSize()        { return size & 0x7FFFFFFF; }
    void setSize(uint32 sz) { ASSERT((sz & 0x8000000) == 0); size = (sz & 0x7FFFFFFF); }

    Pond *owner;    // for a piece of scum in use, this points to it's own Pond
                    // otherwise it's a link to the next item on the free list
private:
    uint32 size;    // The high bit is used as the gc mark flag
};

// A pond is a place to get chunks of PondScum from and to return them to
#define POND_SIZE (8000)
#define POND_SANITY (0xFADE2BAD)
class Pond {
public:
    Pond(size_t sz, Pond *nextPond);
    
    void *allocFromPond(int32 sz);
    void returnToPond(PondScum *p);

    void resetMarks();
    void moveUnmarkedToFreeList();

    uint32 sanity;

    size_t pondSize;
    uint8 *pondBase;
    uint8 *pondBottom;
    uint8 *pondTop;

    PondScum *freeHeader;

    Pond *nextPond;
};

#define GCMARKOBJECT(n) if ((n) && !(n)->isMarked()) { (n)->mark(); (n)->markChildren(); }
#define GCMARKVALUE(v) JS2Object::markJS2Value(v)

class JS2Object {
// Every object is either undefined, null, a Boolean,
// a number, a string, a namespace, a compound attribute, a class, a method closure, 
// a prototype instance, a class instance, a package object, or the global object.
public:

    JS2Object(ObjectKind kind) : kind(kind) { }

    ObjectKind kind;

    static Pond pond;
    static std::list<PondScum **> rootList;
    typedef std::list<PondScum **>::iterator RootIterator;

    static void gc(JS2Metadata *meta);
    static RootIterator addRoot(void *t);   // pass the address of any JS2Object pointer
                                            // Note: Not the address of a JS2VAL!
    static void removeRoot(RootIterator ri);

    static void *alloc(size_t s);
    static void unalloc(void *p);

    void *operator new(size_t s)    { return alloc(s); }
    void operator delete(void *p)   { unalloc(p); }

    virtual void markChildren()     { }
    bool isMarked()                 { return ((PondScum *)this)[-1].isMarked(); }
    void mark()                     { ((PondScum *)this)[-1].mark(); }

    static void mark(const void *p)       { ((PondScum *)p)[-1].mark(); }
    static void markJS2Value(js2val v);

};

class Attribute : public JS2Object {
public:
    enum AttributeKind { TrueAttr, FalseAttr, NamespaceAttr, CompoundAttr };
    enum MemberModifier { NoModifier, Static, Constructor, Abstract, Virtual, Final};
    enum OverrideModifier { NoOverride, DoOverride, DontOverride, OverrideUndefined };


    Attribute(AttributeKind akind) : JS2Object(AttributeObjectKind), attrKind(akind) { }

    static Attribute *combineAttributes(Attribute *a, Attribute *b);
    static CompoundAttribute *toCompoundAttribute(Attribute *a);

    virtual CompoundAttribute *toCompoundAttribute()    { ASSERT(false); return NULL; }

    AttributeKind attrKind;
};

// A Namespace (is also an attribute)
class Namespace : public Attribute {
public:
    Namespace(const String *name) : Attribute(NamespaceAttr), name(name) { }

    virtual CompoundAttribute *toCompoundAttribute();

    const String *name;       // The namespace's name used by toString
};

// A QualifiedName is the combination of an identifier and a namespace
class QualifiedName {
public:
    QualifiedName(Namespace *nameSpace, const StringAtom &id) : nameSpace(nameSpace), id(id) { }

    bool operator ==(const QualifiedName &b) { return (nameSpace == b.nameSpace) && (id == b.id); }

    Namespace *nameSpace;    // The namespace qualifier
    const StringAtom &id;    // The name
};

// A MULTINAME is the semantic domain of sets of qualified names. Multinames are used internally in property lookup.
// We keep Multinames as a basename and a list of namespace qualifiers (XXX is that right - would the basename 
// ever be different for the same multiname?)
typedef std::vector<Namespace *> NamespaceList;
typedef NamespaceList::iterator NamespaceListIterator;
class Multiname : public JS2Object {
public:    
    Multiname(const String *name) : JS2Object(MultinameKind), name(name) { }
    Multiname(const String *name, Namespace *ns) : JS2Object(MultinameKind), name(name) { addNamespace(ns); }

    void addNamespace(Namespace *ns)                { nsList.push_back(ns); }
    void addNamespace(NamespaceList *ns);
    void addNamespace(Context &cxt);

    bool matches(QualifiedName &q)                  { return (*name == q.id) && onList(q.nameSpace); }
    bool onList(Namespace *nameSpace);

    NamespaceList nsList;
    const String *name;

    virtual void markChildren();
};


class NamedParameter {
public:
    StringAtom &name;      // This parameter's name
    JS2Class *type;        // This parameter's type
};

class Signature {
    JS2Class **requiredPositional;      // List of the types of the required positional parameters
    JS2Class **optionalPositional;      // List of the types of the optional positional parameters, which follow the
                                        // required positional parameters
    NamedParameter **optionalNamed;     // Set of the types and names of the optional named parameters
    JS2Class *rest;                     // The type of any extra arguments that may be passed or null if no extra
                                        // arguments are allowed
    bool restAllowsNames;               // true if the extra arguments may be named
    bool returnType;                    // The type of this function's result
};

// A base class for Instance and Static members for convenience.
class Member {
public:
    enum MemberKind { Forbidden, Variable, HoistedVariable, ConstructorMethod, Accessor, InstanceVariableKind, InstanceMethodKind, InstanceAccessorKind };
    
    Member(MemberKind kind) : kind(kind) { }

    MemberKind kind;

    virtual bool isMarked()             { return true; }
    virtual void mark()                 { }
    virtual void markChildren()         { }
};

// A static member is either forbidden, a variable, a hoisted variable, a constructor method, or an accessor:
class StaticMember : public Member {
public:
    StaticMember(MemberKind kind) : Member(kind) { }

    StaticMember *cloneContent; // Used during cloning operation to prevent cloning of duplicates (i.e. once
                                // a clone exists for this member it's recorded here and used for any other
                                // bindings that refer to this member.)

    virtual StaticMember *clone()       { ASSERT(false); return NULL; }
};

#define FUTURE_TYPE ((JS2Class *)(-1))

class Variable : public StaticMember {
public:
    Variable() : StaticMember(Member::Variable), type(NULL), vb(NULL), value(JS2VAL_VOID), immutable(false) { }
    Variable(JS2Class *type, js2val value, bool immutable) : StaticMember(StaticMember::Variable), type(type), vb(NULL), value(value), immutable(immutable) { }

    virtual StaticMember *clone()   { return new Variable(type, value, immutable); }
    
    JS2Class *type;                 // Type of values that may be stored in this variable, NULL if INACCESSIBLE, FUTURE_TYPE if pending
    VariableBinding *vb;            // The variable definition node, to resolve future types
    js2val value;                   // This variable's current value; future if the variable has not been declared yet;
                                    // uninitialised if the variable must be written before it can be read
    bool immutable;                 // true if this variable's value may not be changed once set

    virtual bool isMarked()             { return (JS2VAL_IS_OBJECT(value) && JS2VAL_TO_OBJECT(value)->isMarked()); }
    virtual void mark()                 { if (JS2VAL_IS_OBJECT(value)) JS2VAL_TO_OBJECT(value)->mark(); }
    virtual void markChildren()         { if (JS2VAL_IS_OBJECT(value)) JS2VAL_TO_OBJECT(value)->markChildren(); }
};

class HoistedVar : public StaticMember {
public:
    HoistedVar() : StaticMember(Member::HoistedVariable), value(JS2VAL_VOID), hasFunctionInitializer(false) { }

    js2val value;                   // This variable's current value
    bool hasFunctionInitializer;    // true if this variable was created by a function statement

    virtual bool isMarked()             { return (JS2VAL_IS_OBJECT(value) && JS2VAL_TO_OBJECT(value)->isMarked()); }
    virtual void mark()                 { if (JS2VAL_IS_OBJECT(value)) JS2VAL_TO_OBJECT(value)->mark(); }
    virtual void markChildren()         { if (JS2VAL_IS_OBJECT(value)) JS2VAL_TO_OBJECT(value)->markChildren(); }
};

class ConstructorMethod : public StaticMember {
public:
    ConstructorMethod() : StaticMember(Member::ConstructorMethod), code(NULL) { }

    Invokable *code;        // This function itself (a callable object)
};

class Accessor : public StaticMember {
public:
    Accessor() : StaticMember(Member::Accessor), type(NULL), code(NULL) { }

    JS2Class *type;         // The type of the value read from the getter or written into the setter
    Invokable *code;        // calling this object does the read or write
};


// A DYNAMICPROPERTY record describes one dynamic property of one (prototype or class) instance.
typedef std::map<String, js2val> DynamicPropertyMap;
typedef DynamicPropertyMap::iterator DynamicPropertyIterator;


// A STATICBINDING describes the member to which one qualified name is bound in a frame. Multiple 
// qualified names may be bound to the same member in a frame, but a qualified name may not be 
// bound to multiple members in a frame (except when one binding is for reading only and 
// the other binding is for writing only).
class StaticBinding {
public:
    StaticBinding(QualifiedName &qname, StaticMember *content) : qname(qname), xplicit(false), content(content) { }

    QualifiedName qname;        // The qualified name bound by this binding
    bool xplicit;               // true if this binding should not be imported into the global scope by an import statement
    StaticMember *content;      // The member to which this qualified name was bound
};

class InstanceMember : public Member {
public:
    InstanceMember(MemberKind kind, JS2Class *type, bool final) : Member(kind), type(type), final(final) { }

    JS2Class *type;             // Type of values that may be stored in this variable
    bool final;                 // true if this member may not be overridden in subclasses

    virtual bool isMarked();
    virtual void mark();
    virtual void markChildren();
};

class InstanceVariable : public InstanceMember {
public:
    InstanceVariable(JS2Class *type, bool immutable, bool final, uint32 slotIndex) : InstanceMember(InstanceVariableKind, type, final), immutable(immutable), slotIndex(slotIndex) { }
    Invokable *evalInitialValue;    // A function that computes this variable's initial value
    bool immutable;                 // true if this variable's value may not be changed once set
    uint32 slotIndex;               // The index into an instance's slot array in which this variable is stored

    virtual bool isMarked();
    virtual void mark();
    virtual void markChildren();
};

class InstanceMethod : public InstanceMember {
public:
    InstanceMethod(FixedInstance *fInst) : InstanceMember(InstanceMethodKind, NULL, false), fInst(fInst) { }
    Signature type;         // This method's signature
    Invokable *code;        // This method itself (a callable object); null if this method is abstract
    FixedInstance *fInst;

    virtual bool isMarked();
    virtual void mark();
    virtual void markChildren();
};

class InstanceAccessor : public InstanceMember {
public:
    InstanceAccessor(Invokable *code, JS2Class *type, bool final) : InstanceMember(InstanceAccessorKind, type, final), code(code) { }
    Invokable *code;        // A callable object which does the read or write; null if this method is abstract
};

class InstanceBinding {
public:
    InstanceBinding(QualifiedName &qname, InstanceMember *content) : qname(qname), content(content) { }

    QualifiedName qname;         // The qualified name bound by this binding
    InstanceMember *content;     // The member to which this qualified name was bound
};

// Override status is used to resolve overriden definitions for instance members
#define POTENTIAL_CONFLICT ((InstanceMember *)(-1))
class OverrideStatus {
public:
    OverrideStatus(InstanceMember *overriddenMember, const StringAtom *name)
        : overriddenMember(overriddenMember), multiname(name) { }
    
    InstanceMember *overriddenMember;   // NULL for none
    Multiname multiname;
};
typedef std::pair<OverrideStatus *, OverrideStatus *> OverrideStatusPair;




// A StaticBindingMap maps names to a list of StaticBindings. Each StaticBinding in the list
// will have the same QualifiedName.name, but (potentially) different QualifiedName.namespace values
typedef std::multimap<String, StaticBinding *> StaticBindingMap;
typedef StaticBindingMap::iterator StaticBindingIterator;

typedef std::multimap<String, InstanceBinding *> InstanceBindingMap;
typedef InstanceBindingMap::iterator InstanceBindingIterator;


// A frame contains bindings defined at a particular scope in a program. A frame is either the top-level system frame, 
// a global object, a package, a function frame, a class, or a block frame
class Frame : public JS2Object {
public:
    enum Plurality { Singular, Plural };

    Frame(ObjectKind kind) : JS2Object(kind), nextFrame(NULL), pluralFrame(NULL) { }
    Frame(ObjectKind kind, Frame *pluralFrame) : JS2Object(kind), nextFrame(NULL), pluralFrame(pluralFrame) { }

    StaticBindingMap staticReadBindings;        // Map of qualified names to readable static members defined in this frame
    StaticBindingMap staticWriteBindings;       // Map of qualified names to writable static members defined in this frame

    virtual void instantiate(Environment *env)  { ASSERT(false); }

    Frame *nextFrame;
    Frame *pluralFrame;                         // for a singular frame, this the plural frame from which it will be instantiated

    virtual void markChildren();

};


class JS2Class : public Frame {
public:
    JS2Class(JS2Class *super, JS2Object *proto, Namespace *privateNamespace, bool dynamic, bool allowNull, bool final, const String *name);

    const String *getName()                 { return name; }
        
    InstanceBindingMap instanceReadBindings;    // Map of qualified names to readable instance members defined in this class    
    InstanceBindingMap instanceWriteBindings;   // Map of qualified names to writable instance members defined in this class    

    InstanceVariable **instanceInitOrder;       // List of instance variables defined in this class in the order in which they are initialised

    bool complete;                              // true after all members of this class have been added to this CLASS record

    JS2Class    *super;                         // This class's immediate superclass or null if none
    JS2Object   *prototype;                     // An object that serves as this class's prototype for compatibility with ECMAScript 3; may be null

    Namespace *privateNamespace;                // This class's private namespace

    bool dynamic;                               // true if this class or any of its ancestors was defined with the dynamic attribute
    bool allowNull;                             // true if null is considered to be an instance of this class
    bool final;                                 // true if this class cannot be subclassed

    Callor *call;                               // A procedure to call when this class is used in a call expression
    Constructor *construct;                     // A procedure to call when this class is used in a new expression

    uint32 slotCount;

    const String *name;

    virtual void instantiate(Environment * /* env */)  { }      // nothing to do
    virtual void markChildren();

};

class GlobalObject : public Frame {
public:
    GlobalObject(World &world) : Frame(GlobalObjectKind), internalNamespace(new Namespace(&world.identifiers["internal"])) { }

    Namespace *internalNamespace;               // This global object's internal namespace
    DynamicPropertyMap dynamicProperties;       // A set of this global object's dynamic properties
    virtual void markChildren();
};


// A SLOT record describes the value of one fixed property of one instance.
class Slot {
public:
// We keep the slotIndex in the InstanceVariable rather than go looking for a specific id
//    InstanceVariable *id;        // The instance variable whose value this slot carries
    js2val value;                // This fixed property's current value; uninitialised if the fixed property is an uninitialised constant
};

// Instances of non-dynamic classes are represented as FIXEDINSTANCE records. These instances can contain only fixed properties.
class FixedInstance : public JS2Object {
public:
    FixedInstance(JS2Class *type);

    JS2Class    *type;          // This instance's type
    FunctionWrapper *fWrap;
    Invokable   *call;          // A procedure to call when this instance is used in a call expression
    Invokable   *construct;     // A procedure to call when this instance is used in a new expression
    Environment *env;           // The environment to pass to the call or construct procedure
    const String  *typeofString;  // A string to return if typeof is invoked on this instance
    Slot        *slots;         // A set of slots that hold this instance's fixed property values
    virtual void markChildren();
};

// Instances of dynamic classes are represented as DYNAMICINSTANCE records. These instances can contain fixed and dynamic properties.
class DynamicInstance : public JS2Object {
public:
    DynamicInstance(JS2Class *type);

    JS2Class    *type;          // This instance's type
    FunctionWrapper *fWrap;
    Invokable   *call;          // A procedure to call when this instance is used in a call expression
    Invokable   *construct;     // A procedure to call when this instance is used in a new expression
    Environment *env;           // The environment to pass to the call or construct procedure
    const String  *typeofString;  // A string to return if typeof is invoked on this instance
    Slot        *slots;         // A set of slots that hold this instance's fixed property values
    DynamicPropertyMap dynamicProperties; // A set of this instance's dynamic properties
    virtual void markChildren();

    virtual void writeProperty(JS2Metadata *meta, const String *name, js2val newValue);
};

// Prototype instances are represented as PROTOTYPE records. Prototype instances
// contain no fixed properties.

class PrototypeInstance : public JS2Object {
public:
    PrototypeInstance(JS2Object *parent, JS2Class *type) : JS2Object(PrototypeInstanceKind), parent(parent), type(type) { }


    JS2Object   *parent;        // If this instance was created by calling new on a prototype function,
                                // the value of the function’s prototype property at the time of the call;
                                // none otherwise.
    JS2Class    *type;          // XXX used to determine [[class]] value 
    DynamicPropertyMap dynamicProperties; // A set of this instance's dynamic properties
    virtual void markChildren();
};

// Date instances are fixed (not dynamic? XXX) instances created by the Date class, they have an extra field 
// that contains the millisecond count
class DateInstance : public FixedInstance {
public:
    DateInstance(JS2Class *type) : FixedInstance(type) { }

    float64     ms;
};

// String instances are fixed (not dynamic? XXX) instances created by the String class, they have an extra field 
// that contains the string data
class StringInstance : public FixedInstance {
public:
    StringInstance(JS2Class *type) : FixedInstance(type), mValue(NULL) { }

    String     *mValue;             // has been allocated by engine in the GC'able Pond

    virtual void markChildren()     { if (mValue) JS2Object::mark(mValue); }
};

// Array instances are dynamic instances created by the Array class, they 
// maintain the value of the 'length' property when 'indexable' elements
// are added.
class ArrayInstance : public DynamicInstance {
public:
    ArrayInstance(JS2Class *type) : DynamicInstance(type) { }

    virtual void writeProperty(JS2Metadata *meta, const String *name, js2val newValue);
};

// RegExp instances are dynamic instances created by the RegExp class, they have an extra field 
// that contains the RegExp object
class RegExpInstance : public DynamicInstance {
public:
    RegExpInstance(JS2Class *type) : DynamicInstance(type) { }

    void setLastIndex(JS2Metadata *meta, js2val a);
    void setGlobal(JS2Metadata *meta, js2val a);
    void setMultiline(JS2Metadata *meta, js2val a);
    void setIgnoreCase(JS2Metadata *meta, js2val a);
    void setSource(JS2Metadata *meta, js2val a);

    js2val getLastIndex(JS2Metadata *meta);
    js2val getGlobal(JS2Metadata *meta);
    js2val getMultiline(JS2Metadata *meta);
    js2val getIgnoreCase(JS2Metadata *meta);
    js2val getSource(JS2Metadata *meta);

    REState     *mRegExp;
};

// A helper class for 'for..in' statements
class ForIteratorObject : public JS2Object {
public:
    ForIteratorObject(JS2Object *obj) : JS2Object(ForIteratorKind), obj(obj), nameList(NULL) { }

    bool first();
    bool next(JS2Engine *engine);

    js2val value(JS2Engine *engine);

    JS2Object *obj;
    JS2Object *originalObj;

    virtual void markChildren()     { GCMARKOBJECT(obj); GCMARKOBJECT(originalObj); }

private:

    bool buildNameList();

    const String **nameList;
    int32 it;
    int32 length;
};

// A METHODCLOSURE tuple describes an instance method with a bound this value.
class MethodClosure : public JS2Object {
public:
    MethodClosure(js2val thisObject, InstanceMethod *method) : JS2Object(MethodClosureKind), thisObject(thisObject), method(method) { }
    js2val              thisObject;     // The bound this value
    InstanceMethod      *method;        // The bound method

    virtual void markChildren();
};


// Base class for all references (lvalues)
// References are generated during the eval stage (bytecode generation), but shouldn't live beyond that
class Reference {
public:
    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)              { ASSERT(false); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)             { ASSERT(false); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { ASSERT(false); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { ASSERT(false); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { ASSERT(false); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)           { ASSERT(false); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)           { ASSERT(false); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)            { ASSERT(false); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)            { ASSERT(false); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)            { ASSERT(false); }    
};

class LexicalReference : public Reference {
// A LEXICALREFERENCE tuple has the fields below and represents an lvalue that refers to a variable with one
// of a given set of qualified names. LEXICALREFERENCE tuples arise from evaluating identifiers a and qualified identifiers
// q::a.
public:
    LexicalReference(const StringAtom *name, bool strict) : variableMultiname(new Multiname(name)), env(NULL), strict(strict) { }
    LexicalReference(const StringAtom *name, Namespace *nameSpace, bool strict) : variableMultiname(new Multiname(name, nameSpace)), env(NULL), strict(strict) { }

    
    Multiname *variableMultiname;   // A nonempty set of qualified names to which this reference can refer
    Environment *env;               // The environment in which the reference was created.
    bool strict;                    // The strict setting from the context in effect at the point where the reference was created
    
    
    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eLexicalRead, pos); bCon->addMultiname(variableMultiname); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eLexicalWrite, pos); bCon->addMultiname(variableMultiname); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eLexicalRef, pos); bCon->addMultiname(variableMultiname); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eLexicalPostInc, pos); bCon->addMultiname(variableMultiname); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eLexicalPostDec, pos); bCon->addMultiname(variableMultiname); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eLexicalPreInc, pos); bCon->addMultiname(variableMultiname); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eLexicalPreDec, pos); bCon->addMultiname(variableMultiname); }
    
    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eLexicalDelete, pos); bCon->addMultiname(variableMultiname); }
};

class DotReference : public Reference {
// A DOTREFERENCE tuple has the fields below and represents an lvalue that refers to a property of the base
// object with one of a given set of qualified names. DOTREFERENCE tuples arise from evaluating subexpressions such as a.b or
// a.q::b.
public:
    DotReference(const StringAtom *name) : propertyMultiname(new Multiname(name)) { }
    DotReference(Multiname *mn) : propertyMultiname(mn) { }

    // In this implementation, the base is established by the execution of the preceding expression and
    // is available on the execution stack, not in the reference object (which is a codegen-time only)
    // js2val base;                 // The object whose property was referenced (a in the examples above). The
                                    // object may be a LIMITEDINSTANCE if a is a super expression, in which case
                                    // the property lookup will be restricted to members defined in proper ancestors
                                    // of base.limit.
    Multiname *propertyMultiname;   // A nonempty set of qualified names to which this reference can refer (b
                                    // qualified with the namespace q or all currently open namespaces in the
                                    // example above)
    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eDotRead, pos); bCon->addMultiname(propertyMultiname); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eDotWrite, pos); bCon->addMultiname(propertyMultiname); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eDotRef, pos); bCon->addMultiname(propertyMultiname); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadForInvokeBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eDotPostInc, pos); bCon->addMultiname(propertyMultiname); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eDotPostDec, pos); bCon->addMultiname(propertyMultiname); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eDotPreInc, pos); bCon->addMultiname(propertyMultiname); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eDotPreDec, pos); bCon->addMultiname(propertyMultiname); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eDotDelete, pos); bCon->addMultiname(propertyMultiname); }
};


class NamedArgument {
public:
    StringAtom &name;               // This argument's name
    js2val value;                   // This argument's value
};

// An ARGUMENTLIST describes the arguments (other than this) passed to a function.
class ArgumentList {
public:
    JS2Object *positional;          // Ordered list of positional arguments
    NamedArgument *named;           // Set of named arguments
};


class BracketReference : public Reference {
// A BRACKETREFERENCE tuple has the fields below and represents an lvalue that refers to the result of
// applying the [] operator to the base object with the given arguments. BRACKETREFERENCE tuples arise from evaluating
// subexpressions such as a[x] or a[x,y].
public:
    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eBracketRead, pos); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eBracketWrite, pos); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eBracketRef, pos); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { bCon->emitOp(eBracketReadForRef, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { bCon->emitOp(eBracketWriteRef, pos); }
/*
    js2val base;                    // The object whose property was referenced (a in the examples above). The object may be a
                                    // LIMITEDINSTANCE if a is a super expression, in which case the property lookup will be
                                    // restricted to definitions of the [] operator defined in proper ancestors of base.limit.
    ArgumentList args;              // The list of arguments between the brackets (x or x,y in the examples above)
*/

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eBracketPostInc, pos); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eBracketPostDec, pos); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eBracketPreInc, pos); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eBracketPreDec, pos); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eBracketDelete, pos); }
};


// The top-level frame containing predefined constants, functions, and classes.
class SystemFrame : public Frame {
public:
    SystemFrame() : Frame(SystemKind) { }
};

// Frames holding bindings for invoked functions
class ParameterFrame : public Frame {
public:
    ParameterFrame(js2val thisObject, bool prototype) : Frame(ParameterKind), thisObject(thisObject), prototype(prototype) { }    
    ParameterFrame(ParameterFrame *pluralFrame) : Frame(ParameterKind, pluralFrame), thisObject(JS2VAL_UNDEFINED), prototype(pluralFrame->prototype) { }

    Plurality plurality;
    js2val thisObject;              // The value of this; none if this function doesn't define this;
                                    // inaccessible if this function defines this but the value is not 
                                    // available because this function hasn't been called yet.

    bool prototype;                 // true if this function is not an instance method but defines this anyway

    virtual void instantiate(Environment *env);
    virtual void markChildren();
};

class BlockFrame : public Frame {
public:
    BlockFrame() : Frame(BlockKind) { }
    BlockFrame(BlockFrame *pluralFrame) : Frame(BlockKind, pluralFrame) { }

    Plurality plurality;

    virtual void instantiate(Environment *env);
};


class LookupKind {
public:
    LookupKind(bool isLexical, js2val thisObject) : isLexical(isLexical), thisObject(thisObject) { }
    
    bool isPropertyLookup() { return !isLexical; }

    bool isLexical;         // if isLexical, use the 'this' below. Otherwise it's a propertyLookup
    js2val thisObject;
};

// Environments contain the bindings that are visible from a given point in the source code. An ENVIRONMENT is 
// a list of two or more frames. Each frame corresponds to a scope. More specific frames are listed first
// -each frame's scope is directly contained in the following frame's scope. The last frame is always the
// SYSTEMFRAME. The next-to-last frame is always a PACKAGE or GLOBAL frame.
class Environment {
public:
    Environment(SystemFrame *systemFrame, Frame *nextToLast) : firstFrame(nextToLast) { nextToLast->nextFrame = systemFrame; }

    JS2Class *getEnclosingClass();
    Frame *getRegionalFrame();
    Frame *getTopFrame()                { return firstFrame; }
    Frame *getPackageOrGlobalFrame();

    void addFrame(Frame *f)             { f->nextFrame = firstFrame; firstFrame = f; }
    void removeTopFrame()               { firstFrame = firstFrame->nextFrame; }

    js2val findThis(bool allowPrototypeThis);
    js2val lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase);
    void lexicalWrite(JS2Metadata *meta, Multiname *multiname, js2val newValue, bool createIfMissing, Phase phase);
    bool lexicalDelete(JS2Metadata *meta, Multiname *multiname, Phase phase);

    void instantiateFrame(Frame *pluralFrame, Frame *singularFrame);

    void mark()                         { GCMARKOBJECT(firstFrame) }

private:
    Frame *firstFrame;
};


typedef js2val (NativeCode)(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc);

class FunctionWrapper {
public:
    FunctionWrapper(bool unchecked, ParameterFrame *compileFrame) 
        : bCon(new BytecodeContainer), code(NULL), unchecked(unchecked), compileFrame(compileFrame) { }
    FunctionWrapper(bool unchecked, ParameterFrame *compileFrame, NativeCode *code) 
        : bCon(NULL), code(code), unchecked(unchecked), compileFrame(compileFrame) { }

    BytecodeContainer   *bCon;
    NativeCode          *code;
    bool                unchecked;      // true if the function is untyped, non-method, normal
    ParameterFrame      *compileFrame;
};



typedef std::vector<Namespace *> NamespaceList;
typedef NamespaceList::iterator NamespaceListIterator;

// A CONTEXT carries static information about a particular point in the source program.
class Context {
public:
    Context() : strict(false) { }
    Context(Context *cxt);
    bool strict;                    // true if strict mode is in effect
    NamespaceList openNamespaces;   // The set of namespaces that are open at this point. 
                                    // The public namespace is always a member of this set.
};

// The 'true' attribute
class TrueAttribute : public Attribute {
public:
    TrueAttribute() : Attribute(TrueAttr) { }
    virtual CompoundAttribute *toCompoundAttribute();
};

// The 'false' attribute
class FalseAttribute : public Attribute {
public:
    FalseAttribute() : Attribute(FalseAttr) { }
};

// Compound attribute objects are all values obtained from combining zero or more syntactic attributes 
// that are not Booleans or single namespaces. 
class CompoundAttribute : public Attribute {
public:
    CompoundAttribute();
    void addNamespace(Namespace *n);

    virtual CompoundAttribute *toCompoundAttribute()    { return this; }

    NamespaceList *namespaces;      // The set of namespaces contained in this attribute
    bool xplicit;                   // true if the explicit attribute has been given
    bool dynamic;                   // true if the dynamic attribute has been given
    MemberModifier memberMod;       // if one of these attributes has been given; none if not.
    OverrideModifier overrideMod;   // if the override attribute  with one of these arguments was given; 
                                    // true if the attribute override without arguments was given; none if the override attribute was not given.
    bool prototype;                 // true if the prototype attribute has been given
    bool unused;                    // true if the unused attribute has been given

    virtual void markChildren();
};


typedef std::vector<StmtNode *> TargetList;
typedef std::vector<StmtNode *>::iterator TargetListIterator;
typedef std::vector<StmtNode *>::reverse_iterator TargetListReverseIterator;

struct MemberDescriptor {
    StaticMember *staticMember;
    QualifiedName *qname;
};

class JS2Metadata {
public:
    
    JS2Metadata(World &world);

    void setCurrentParser(Parser *parser) { mParser = parser; }

    void ValidateStmtList(StmtNode *p);
    js2val EvalStmtList(Phase phase, StmtNode *p);

    js2val readEvalString(const String &str, const String& fileName);
    js2val readEvalFile(const String& fileName);


    void ValidateStmtList(Context *cxt, Environment *env, StmtNode *p);
    void ValidateTypeExpression(Context *cxt, Environment *env, ExprNode *e)    { ValidateExpression(cxt, env, e); } 
    void ValidateStmt(Context *cxt, Environment *env, StmtNode *p);
    void ValidateExpression(Context *cxt, Environment *env, ExprNode *p);
    void ValidateAttributeExpression(Context *cxt, Environment *env, ExprNode *p);

    js2val ExecuteStmtList(Phase phase, StmtNode *p);
    js2val EvalExpression(Environment *env, Phase phase, ExprNode *p);
    JS2Class *EvalTypeExpression(Environment *env, Phase phase, ExprNode *p);
    Reference *EvalExprNode(Environment *env, Phase phase, ExprNode *p);
    Attribute *EvalAttributeExpression(Environment *env, Phase phase, ExprNode *p);
    void EvalStmt(Environment *env, Phase phase, StmtNode *p);


    JS2Class *objectType(js2val obj);

    StaticMember *findFlatMember(Frame *container, Multiname *multiname, Access access, Phase phase);
    InstanceBinding *resolveInstanceMemberName(JS2Class *js2class, Multiname *multiname, Access access, Phase phase);

    HoistedVar *defineHoistedVar(Environment *env, const StringAtom *id, StmtNode *p);
    Multiname *defineStaticMember(Environment *env, const StringAtom *id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, StaticMember *m, size_t pos);
    OverrideStatusPair *defineInstanceMember(JS2Class *c, Context *cxt, const StringAtom *id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, InstanceMember *m, size_t pos);
    OverrideStatus *resolveOverrides(JS2Class *c, Context *cxt, const StringAtom *id, NamespaceList *namespaces, Access access, bool expectMethod, size_t pos);
    OverrideStatus *searchForOverrides(JS2Class *c, const StringAtom *id, NamespaceList *namespaces, Access access, size_t pos);
    InstanceMember *findInstanceMember(JS2Class *c, QualifiedName *qname, Access access);
    Slot *findSlot(js2val thisObjVal, InstanceVariable *id);
    bool findStaticMember(JS2Class *c, Multiname *multiname, Access access, Phase phase, MemberDescriptor *result);
    JS2Class *getVariableType(Variable *v, Phase phase, size_t pos);


    bool readProperty(js2val container, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval);
    bool readProperty(Frame *pf, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval);
    bool readDynamicProperty(JS2Object *container, Multiname *multiname, LookupKind *lookupKind, Phase phase, js2val *rval);
    bool readStaticMember(StaticMember *m, Phase phase, js2val *rval);
    bool readInstanceMember(js2val containerVal, JS2Class *c, QualifiedName *qname, Phase phase, js2val *rval);
    JS2Object *lookupDynamicProperty(JS2Object *obj, const String *name);
    bool JS2Metadata::hasOwnProperty(JS2Object *obj, const String *name);

    bool writeProperty(js2val container, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue, Phase phase);
    bool writeProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, bool createIfMissing, js2val newValue, Phase phase);
    bool writeDynamicProperty(JS2Object *container, Multiname *multiname, bool createIfMissing, js2val newValue, Phase phase);
    bool writeStaticMember(StaticMember *m, js2val newValue, Phase phase);
    bool writeInstanceMember(js2val containerVal, JS2Class *c, QualifiedName *qname, js2val newValue, Phase phase);

    bool deleteProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, Phase phase, bool *result);
    bool deleteProperty(js2val container, Multiname *multiname, LookupKind *lookupKind, Phase phase, bool *result);
    bool deleteDynamicProperty(JS2Object *container, Multiname *multiname, LookupKind *lookupKind, bool *result);
    bool deleteStaticMember(StaticMember *m, bool *result);
    bool deleteInstanceMember(JS2Class *c, QualifiedName *qname, bool *result);

    void addGlobalObjectFunction(char *name, NativeCode *code);

    void reportError(Exception::Kind kind, const char *message, size_t pos, const char *arg = NULL);
    void reportError(Exception::Kind kind, const char *message, size_t pos, const String &name);
    void reportError(Exception::Kind kind, const char *message, size_t pos, const String *name);

    const String *convertValueToString(js2val x);
    js2val convertValueToPrimitive(js2val x);
    float64 convertValueToDouble(js2val x);
    bool convertValueToBoolean(js2val x);
    int32 convertValueToInteger(js2val x);
    js2val convertValueToGeneralNumber(js2val x);
    js2val convertValueToObject(js2val x);

    const String *toString(js2val x)    { if (JS2VAL_IS_STRING(x)) return JS2VAL_TO_STRING(x); else return convertValueToString(x); }
    js2val toPrimitive(js2val x)        { if (JS2VAL_IS_PRIMITIVE(x)) return x; else return convertValueToPrimitive(x); }
    float64 toFloat64(js2val x);
    js2val toGeneralNumber(js2val x)    { if (JS2VAL_IS_NUMBER(x)) return x; else return convertValueToGeneralNumber(x); }
    bool toBoolean(js2val x)            { if (JS2VAL_IS_BOOLEAN(x)) return JS2VAL_TO_BOOLEAN(x); else return convertValueToBoolean(x); }
    int32 toInteger(js2val x)           { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else return convertValueToInteger(x); }
    js2val toObject(js2val x)           { if (JS2VAL_IS_OBJECT(x)) return x; else return convertValueToObject(x); }

    // Used for interning strings
    World &world;

    // The execution engine
    JS2Engine *engine;

    // Random number generator state
    bool      rngInitialized;
    int64     rngMultiplier;
    int64     rngAddend;
    int64     rngMask;
    int64     rngSeed;
    float64   rngDscale;

    
    // The one and only 'public' namespace
    Namespace *publicNamespace;

    StaticMember *forbiddenMember;  // just need one of these hanging around

    // The base classes:
    JS2Class *objectClass;
    JS2Class *undefinedClass;
    JS2Class *nullClass;
    JS2Class *booleanClass;
    JS2Class *generalNumberClass;
    JS2Class *numberClass;
    JS2Class *characterClass;
    JS2Class *stringClass;
    JS2Class *namespaceClass;
    JS2Class *attributeClass;
    JS2Class *classClass;
    JS2Class *functionClass;
    JS2Class *prototypeClass;
    JS2Class *packageClass;
    JS2Class *dateClass;
    JS2Class *regexpClass;
    JS2Class *mathClass;
    JS2Class *arrayClass;

    Parser *mParser;                // used for error reporting

    BytecodeContainer *bCon;        // the current output container

    GlobalObject *glob;
    Environment env;
    Context cxt;

    TargetList targetList;

    void mark();

};

}; // namespace MetaData
}; // namespace Javascript

#endif
