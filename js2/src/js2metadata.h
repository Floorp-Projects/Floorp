
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
class LocalBinding;
class Environment;
class Context;
class CompoundAttribute;
class BytecodeContainer;
class Pond;
class SimpleInstance;
class BooleanInstance;
class DateInstance;
class StringInstance;
class FunctionInstance;
class ArrayInstance;
class RegExpInstance;
class Package;
class ArgumentsInstance;

typedef void (Invokable)();
typedef js2val (Callor)(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
typedef js2val (Constructor)(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
typedef js2val (NativeCode)(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc);
typedef js2val (AlienCode)(JS2Metadata *meta, FunctionInstance *fn, const js2val thisValue, js2val argv[], uint32 argc);

extern void initDateObject(JS2Metadata *meta);
extern void initStringObject(JS2Metadata *meta);
extern void initMathObject(JS2Metadata *meta, SimpleInstance *mathObject);
extern void initArrayObject(JS2Metadata *meta);
extern void initRegExpObject(JS2Metadata *meta);
extern void initNumberObject(JS2Metadata *meta);
extern void initErrorObject(JS2Metadata *meta);
extern void initBooleanObject(JS2Metadata *meta);
extern void initFunctionObject(JS2Metadata *meta);

extern js2val Error_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val EvalError_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val RangeError_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val ReferenceError_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val SyntaxError_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val TypeError_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val UriError_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val String_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val RegExp_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val RegExp_ConstructorOpt(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc, bool flat);
extern js2val RegExp_exec(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val Boolean_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
extern js2val Number_Constructor(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);


js2val Array_lengthGet(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);
js2val Array_lengthSet(JS2Metadata *meta, const js2val thisValue, js2val *argv, uint32 argc);

typedef struct {
    char *name;
    uint16 length;
    NativeCode *code;
} FunctionData;


extern uint32 getLength(JS2Metadata *meta, JS2Object *obj);
extern js2val setLength(JS2Metadata *meta, JS2Object *obj, uint32 length);

// OBJECT is the semantic domain of all possible objects and is defined as:
// OBJECT = UNDEFINED | NULL | BOOLEAN | FLOAT64 | LONG | ULONG | CHARACTER | STRING | NAMESPACE |
// COMPOUNDATTRIBUTE | CLASS | METHODCLOSURE | INSTANCE | PACKAGE
//
//  In this implementation, the primitive types are distinguished by the tag value
// of a JS2Value (see js2value.h). Non-primitive types are distinguished by calling
// 'kind()' on the object to recover one of the values below:
//
enum ObjectKind { 
    AttributeObjectKind,
    SystemKind,                 
    PackageKind, 
    ParameterFrameKind, 
    ClassKind, 
    BlockFrameKind, 
    SimpleInstanceKind,
    MultinameKind,
    AlienInstanceKind,
    ForIteratorKind,
    WithFrameKind,
    LimitedInstanceKind,

    EnvironmentKind,         // Not an available JS2 runtime kind
};

enum Plurality { Singular, Plural };

enum Hint { NoHint, NumberHint, StringHint };

typedef enum JS2Version {
    JS2VERSION_1_0     = 100,
    JS2VERSION_1_1     = 110,
    JS2VERSION_1_2     = 120,
    JS2VERSION_1_3     = 130,
    JS2VERSION_1_4     = 140,
    JS2VERSION_1_5     = 150,
    JS2VERSION_DEFAULT = 0,
    JS2VERSION_UNKNOWN = -1
} JS2Version;




class PondScum {
public:    

    typedef enum { JS2ObjectFlag, StringFlag, GenericFlag } ScumFlag;

    void resetMark()        { markFlag = 0; }
    void mark()             { markFlag = 1; }
    bool isMarked()         { return (markFlag != 0); }

    void setFlag(ScumFlag f){ flag = f; }

    void setIsJS2Object()   { flag = JS2ObjectFlag; }
    bool isJS2Object()      { return (flag == JS2ObjectFlag); }

    void setIsString()      { flag = StringFlag; }
    bool isString()         { return (flag == StringFlag); }

    void clearFlags()       { flag = GenericFlag; }

    uint32 getSize()        { return size; }
    void setSize(uint32 sz) { ASSERT(sz < JS_BIT(29)); size = sz; }

    Pond *owner;    // for a piece of scum in use, this points to it's own Pond
                    // otherwise it's a link to the next item on the free list
private:
    unsigned int markFlag:1;
    ScumFlag flag:2;
    unsigned int size:29;
};

// A pond is a place to get chunks of PondScum from and to return them to
#define POND_SIZE (64000)
#define POND_SANITY (0xFADE2BAD)
class Pond {
public:
    Pond(size_t sz, Pond *nextPond);
    ~Pond() { delete [] pondBase; if (nextPond) delete nextPond; }
    
    void *allocFromPond(JS2Metadata *meta, size_t sz, PondScum::ScumFlag flag);
    uint32 returnToPond(PondScum *p);

    void resetMarks();
    uint32 moveUnmarkedToFreeList(JS2Metadata *meta);

    uint32 sanity;

    size_t pondSize;
    uint8 *pondBase;
    uint8 *pondBottom;
    uint8 *pondTop;

    PondScum *freeHeader;

    Pond *nextPond;
};

#define GCMARKOBJECT(n) if ((n) && !(n)->isMarked()) { (n)->markObject(); (n)->markChildren(); }
#define GCMARKVALUE(v) JS2Object::markJS2Value(v)

class RootKeeper;
typedef std::list<RootKeeper *>::iterator RootIterator;


class JS2Object {
// Every object is either undefined, null, a Boolean,
// a number, a string, a namespace, a compound attribute, a class, a method closure, 
// a class instance, a package object, or the global object.
public:

    JS2Object(ObjectKind kind) : kind(kind) { }
    virtual ~JS2Object()    { }

    ObjectKind kind;

    void *operator new(size_t s) throw()    { ASSERT(false); return 0; }
    void operator delete(void *p)   { }

    void *operator new(size_t s, JS2Metadata *meta);
    void operator delete(void *p, JS2Metadata *meta)   { ASSERT(false); }   // only called if constructor fails

    virtual void markChildren()     { } // XXX !!!! XXXX these are supposed to not have vtables !!!!
    virtual void finalize()         { }

    bool isMarked()                 { return ((PondScum *)this)[-1].isMarked(); }
    void markObject()               { ((PondScum *)this)[-1].mark(); }

    static void mark(const void *p)       { ((PondScum *)p)[-1].mark(); }
    static void markJS2Value(js2val v);
};

class RootKeeper {
public:

#ifdef DEBUG

#define DECLARE_ROOTKEEPER_CONSTRUCTOR(type) RootKeeper(JS2Metadata *meta, type **p, int line, char *pfile);
#define DEFINE_ROOTKEEPER_CONSTRUCTOR(type) inline RootKeeper::RootKeeper(JS2Metadata *meta, type **p, int line, char *pfile) : is_js2val(false), p(p), meta(meta) { init(meta, line, pfile); }

#else

#define DECLARE_ROOTKEEPER_CONSTRUCTOR(type) RootKeeper(JS2Metadata *meta, type **p);
#define DEFINE_ROOTKEEPER_CONSTRUCTOR(type) inline RootKeeper::RootKeeper(JS2Metadata *meta, type **p) : is_js2val(false), p(p), meta(meta) { ri = meta->addRoot(this); }

#endif

    DECLARE_ROOTKEEPER_CONSTRUCTOR(JS2Object)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(RegExpInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(CompoundAttribute)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(const String)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(String)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(ArrayInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(BooleanInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(StringInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(JS2Metadata)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(Environment)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(Multiname)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(ParameterFrame)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(SimpleInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(FunctionInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(DateInstance)
    DECLARE_ROOTKEEPER_CONSTRUCTOR(ArgumentsInstance)

#ifdef DEBUG
    RootKeeper(JS2Metadata *meta, js2val *p, int line, char *pfile);
    RootKeeper(JS2Metadata *meta, js2val **p, uint32 count, int line, char *pfile);
    ~RootKeeper();
    void init(JS2Metadata *meta, int ln, char *pfile);
#else
    RootKeeper(JS2Metadata *meta, js2val *p);
    RootKeeper(JS2Metadata *meta, js2val **p, uint32 count);
    ~RootKeeper();
#endif

    RootIterator ri;
    bool is_js2val;
	uint32 js2val_count;
    void *p;
    JS2Metadata *meta;

#ifdef DEBUG
    int line;
    char *file;
#endif
};

#ifdef DEBUG

#define DEFINE_ROOTKEEPER(meta, rk_var, obj) RootKeeper rk_var(meta, &obj, __LINE__, __FILE__);
#define DEFINE_ARRAYROOTKEEPER(meta, rk_var, obj, count) RootKeeper rk_var(meta, &obj, count, __LINE__, __FILE__);

#else

#define DEFINE_ROOTKEEPER(meta, rk_var, obj) RootKeeper rk_var(meta, &obj);
#define DEFINE_ARRAYROOTKEEPER(meta, rk_var, obj, count) RootKeeper rk_var(meta, &obj, count);

#endif



class Attribute : public JS2Object {
public:
    enum AttributeKind { TrueAttr, FalseAttr, NamespaceAttr, CompoundAttr };
    enum MemberModifier { NoModifier, Static, Virtual, Final};
    enum OverrideModifier { NoOverride, DoOverride, DontOverride, OverrideUndefined };


    Attribute(AttributeKind akind) : JS2Object(AttributeObjectKind), attrKind(akind) { }
    virtual ~Attribute()            { }
    virtual void markChildren()     { }

    static Attribute *combineAttributes(JS2Metadata *meta, Attribute *a, Attribute *b);
    static CompoundAttribute *toCompoundAttribute(JS2Metadata *meta, Attribute *a);

    virtual CompoundAttribute *toCompoundAttribute()    { ASSERT(false); return NULL; }

    AttributeKind attrKind;
};

// A Namespace (is also an attribute)
class Namespace : public Attribute {
public:
    Namespace(const StringAtom &name) : Attribute(NamespaceAttr), name(name) { }

    virtual CompoundAttribute *toCompoundAttribute(JS2Metadata *meta);
    virtual void markChildren()     { /*if (name) JS2Object::mark(name); */}

    const StringAtom &name;       // The namespace's name (used by toString)
};

// A QualifiedName is the combination of an identifier and a namespace
class QualifiedName {
public:
    QualifiedName(Namespace *nameSpace, const StringAtom &name) : nameSpace(nameSpace), name(name) { }

    bool operator ==(const QualifiedName &b) { return (nameSpace == b.nameSpace) && (name == b.name); }

    Namespace *nameSpace;    // The namespace qualifier
    const StringAtom &name;  // The name
};

// A MULTINAME is the semantic domain of sets of qualified names. Multinames are used internally in property lookup.
// We keep Multinames as a basename and a list of namespace qualifiers (XXX is that right - would the basename 
// ever be different for the same multiname?)

typedef std::vector<Namespace *> NamespaceList;
typedef NamespaceList::iterator NamespaceListIterator;

class Multiname : public JS2Object {
public:    
//    Multiname() : JS2Object(MultinameKind), name(NULL), nsList(new NamespaceList()) { }
//    Multiname(Namespace *ns) : JS2Object(MultinameKind), name(NULL), nsList(new NamespaceList()) { nsList->push_back(ns); }
    Multiname(QualifiedName& q) : JS2Object(MultinameKind), name(q.name), nsList(new NamespaceList())    { nsList->push_back(q.nameSpace); }
    Multiname(const StringAtom &name) : JS2Object(MultinameKind), name(name), nsList(new NamespaceList()) { }
    Multiname(const StringAtom &name, Namespace *ns) : JS2Object(MultinameKind), name(name), nsList(new NamespaceList()) { addNamespace(ns); }
    Multiname(const StringAtom &name, NamespaceList *ns) : JS2Object(MultinameKind), name(name), nsList(new NamespaceList()) { addNamespace(ns); }
    Multiname(const StringAtom &name, NamespaceList &ns) : JS2Object(MultinameKind), name(name), nsList(new NamespaceList()) { addNamespace(ns); }
    Multiname(const StringAtom &name, Context *cxt) : JS2Object(MultinameKind), name(name), nsList(new NamespaceList()) { addNamespace(*cxt); }
    Multiname(Multiname *m) : JS2Object(MultinameKind), name(m->name), nsList(new NamespaceList())    { addNamespace(m->nsList); }

    Multiname(const Multiname& m) : JS2Object(MultinameKind), name(m.name), nsList(new NamespaceList())    { addNamespace(m.nsList); }
//    void operator =(const Multiname& m) { name = m.name; delete nsList; nsList = new NamespaceList(); addNamespace(m.nsList); }   

    void addNamespace(Namespace *ns)                { nsList->push_back(ns); }
    void addNamespace(NamespaceList &ns);
    void addNamespace(NamespaceList *ns)            { addNamespace(*ns); }
    void addNamespace(Context &cxt);

    bool matches(QualifiedName &q)                  { return (name == q.name) && listContains(q.nameSpace); }
    bool listContains(Namespace *nameSpace);

    QualifiedName *selectPrimaryName(JS2Metadata *meta);
    bool subsetOf(Multiname &mn);

    const StringAtom &name;
    NamespaceList *nsList;

    virtual void markChildren();
    virtual ~Multiname()                 { nsList->clear(); delete nsList; nsList = NULL; }

    virtual void finalize()              { }
};

class NamedParameter {
public:
    const String *name;      // This parameter's name
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

// A base class for Instance and Local members for convenience.
class Member {
public:
    enum MemberKind { 
            ForbiddenMember, 
            DynamicVariableMember, 
            FrameVariableMember, 
            VariableMember, 
            ConstructorMethodMember, 
            SetterMember, 
            GetterMember, 
            InstanceVariableMember, 
            InstanceMethodMember, 
            InstanceGetterMember, 
            InstanceSetterMember };
    
    Member(MemberKind kind) : memberKind(kind), count(0)    { }
    virtual ~Member()                       { }
    MemberKind memberKind;


    virtual void mark()                     { }

    void acquire()  { count++; }
    bool release()  { ASSERT(count > 0); return ((--count) == 0); }
    int count;
};

// A local member is either forbidden, a dynamic variable, a variable, a constructor method, a getter or a setter:
class LocalMember : public Member {
public:
    LocalMember(MemberKind kind) : Member(kind), forbidden(false) { }
    LocalMember(MemberKind kind, bool forbidden) : Member(kind), forbidden(forbidden) { }
    virtual ~LocalMember()                  { }

    LocalMember *cloneContent;  // Used during cloning operation to prevent cloning of duplicates (i.e. once
                                // a clone exists for this member it's recorded here and used for any other
                                // bindings that refer to this member.)
                                // Also used thereafter by 'assignArguments' to initialize the singular
                                // variable instantations in a parameter frame.

    virtual LocalMember *clone()        { if (forbidden) return this; ASSERT(false); return NULL; }
    virtual void mark()                 { }
    bool forbidden;
};

#define FUTURE_TYPE ((JS2Class *)(-1))

class Variable : public LocalMember {
public:
    Variable() : LocalMember(Member::VariableMember), type(NULL), value(JS2VAL_VOID), immutable(false), vb(NULL) { }
    Variable(JS2Class *type, js2val value, bool immutable) 
        : LocalMember(LocalMember::VariableMember), type(type), value(value), immutable(immutable), vb(NULL) { }

    virtual LocalMember *clone()   { return new Variable(type, value, immutable); }
    
    JS2Class *type;                 // Type of values that may be stored in this variable, NULL if INACCESSIBLE, FUTURE_TYPE if pending
    js2val value;                   // This variable's current value; future if the variable has not been declared yet;
                                    // uninitialised if the variable must be written before it can be read
    bool immutable;                 // true if this variable's value may not be changed once set

    // XXX union this with the type field later?
    VariableBinding *vb;            // The variable definition node, to resolve future types

    virtual void mark();
};

class DynamicVariable : public LocalMember {
public:
    DynamicVariable() : LocalMember(Member::DynamicVariableMember), value(JS2VAL_UNDEFINED), sealed(false) { }
    DynamicVariable(js2val value, bool sealed) : LocalMember(Member::DynamicVariableMember), value(value), sealed(sealed) { }

    js2val value;                   // This variable's current value
                                    // XXX may be an uninstantiated function at compile time
    bool sealed;                    // true if this variable cannot be deleted using the delete operator

    virtual LocalMember *clone()       { return new DynamicVariable(); }
    virtual void mark()                { GCMARKVALUE(value); }
};

class FrameVariable : public LocalMember {
public:

    typedef enum { Local, Package, Parameter } FrameVariableKind;

    FrameVariable(uint16 frameSlot, FrameVariableKind kind) 
        : LocalMember(Member::FrameVariableMember), 
            frameSlot(frameSlot), 
            kind(kind), 
            type(NULL), 
            sealed(false) 
        { } 

    uint16 frameSlot;
    FrameVariableKind kind;         // the kind of frame this variable is in

    JS2Class *type;

    bool sealed;                    // true if this variable cannot be deleted using the delete operator
    virtual LocalMember *clone()       { return new FrameVariable(frameSlot, kind); }
};

class ConstructorMethod : public LocalMember {
public:
    ConstructorMethod() : LocalMember(Member::ConstructorMethodMember), value(JS2VAL_VOID) { }
    ConstructorMethod(js2val value) : LocalMember(Member::ConstructorMethodMember), value(value) { }

    js2val value;           // This constructor itself (a callable object)

    virtual void mark()                 { GCMARKVALUE(value); }
};

class Getter : public LocalMember {
public:
    Getter(JS2Class *type, FunctionInstance *code) : LocalMember(Member::GetterMember), type(type), code(code) { }

    JS2Class *type;             // The type of the value read from this getter
    FunctionInstance *code;     // calling this object does the read

    virtual void mark();
};

class Setter : public LocalMember {
public:
    Setter(JS2Class *type, FunctionInstance *code) : LocalMember(Member::SetterMember), type(type), code(code) { }

    JS2Class *type;             // The type of the value written into the setter
    FunctionInstance *code;     // calling this object does the write

    virtual void mark();
};

// A LOCALBINDING describes the member to which one qualified name is bound in a frame. Multiple 
// qualified names may be bound to the same member in a frame, but a qualified name may not be 
// bound to multiple members in a frame (except when one binding is for reading only and 
// the other binding is for writing only).
class LocalBinding {
public:
    LocalBinding(AccessSet accesses, LocalMember *content, bool enumerable) 
        : accesses(accesses), content(content), xplicit(false), enumerable(enumerable) { content->acquire(); }

// The qualified name is to be inferred from the map where this binding is kept
//    QualifiedName qname;        // The qualified name bound by this binding

    virtual ~LocalBinding() { if (content->release()) delete content; }

    AccessSet accesses;
    LocalMember *content;       // The member to which this qualified name was bound
    bool xplicit;               // true if this binding should not be imported into the global scope by an import statement
    bool enumerable;
};

class InstanceMember : public Member {
public:
    InstanceMember(MemberKind kind, Multiname *multiname, bool final, bool enumerable) 
        : Member(kind), multiname(multiname), final(final), enumerable(enumerable) { }
    virtual ~InstanceMember()   { }


    Multiname *multiname;       // The set of qualified names for this instance method
    bool final;                 // true if this member may not be overridden in subclasses
    bool enumerable;            // true if this instance member's public name should be visible in a for-in statemen
    
    virtual Access instanceMemberAccess()   { return ReadWriteAccess; }
    virtual void mark();
};

class InstanceVariable : public InstanceMember {
public:
    InstanceVariable(Multiname *multiname, JS2Class *type, bool immutable, bool final, bool enumerable, uint32 slotIndex) 
        : InstanceMember(InstanceVariableMember, multiname, final, enumerable), 
                type(type), defaultValue(JS2VAL_UNDEFINED), immutable(immutable), slotIndex(slotIndex) { }
    JS2Class *type;                 // Type of values that may be stored in this variable
    js2val defaultValue;            // This variable's default value, if provided
    bool immutable;                 // true if this variable's value may not be changed once set
    uint32 slotIndex;               // The index into an instance's slot array in which this variable is stored

    virtual void mark();
};

class InstanceMethod : public InstanceMember {
public:
    InstanceMethod(Multiname *multiname, FunctionInstance *fInst, bool final, bool enumerable) 
        : InstanceMember(InstanceMethodMember, multiname, final, enumerable), fInst(fInst) { }
    Signature type;                 // This method's signature
//    Invokable *code;              // This method itself (a callable object); null if this method is abstract
    FunctionInstance *fInst;

    virtual ~InstanceMethod();
    virtual void mark();
};

class InstanceGetter : public InstanceMember {
public:
    InstanceGetter(Multiname *multiname, FunctionInstance *fInst, JS2Class *type, bool final, bool enumerable)
        : InstanceMember(InstanceGetterMember, multiname, final, enumerable), fInst(fInst), type(type) { }
    FunctionInstance *fInst;        // A callable object which does the read or write; null if this method is abstract
    JS2Class *type;                 // Type of values that may be stored in this variable

    virtual Access instanceMemberAccess()   { return ReadAccess; }
    virtual void mark();

};

class InstanceSetter : public InstanceMember {
public:
    InstanceSetter(Multiname *multiname, FunctionInstance *fInst, JS2Class *type, bool final, bool enumerable) 
        : InstanceMember(InstanceSetterMember, multiname, final, enumerable), fInst(fInst), type(type) { }
    FunctionInstance *fInst;        // A callable object which does the read or write; null if this method is abstract
    JS2Class *type;                 // Type of values that may be stored in this variable

    virtual Access instanceMemberAccess()   { return WriteAccess; }
    virtual void mark();
};

class InstanceBinding {
public:
    InstanceBinding(AccessSet accesses, InstanceMember *content) 
        : accesses(accesses), content(content) { content->acquire(); }
    virtual ~InstanceBinding() { if (content->release()) delete content; }

// The qualified name is to be inferred from the map where this binding is kept
//    QualifiedName qname;         // The qualified name bound by this binding
    AccessSet accesses;
    InstanceMember *content;     // The member to which this qualified name was bound
};


class LocalBindingEntry {
public:
    LocalBindingEntry(const StringAtom &s) : name(s) { }

    void clear();

    typedef std::pair<Namespace *, LocalBinding *> NamespaceBinding;
    typedef std::vector<NamespaceBinding> NamespaceBindingList;
    typedef NamespaceBindingList::iterator NS_Iterator;

    NS_Iterator begin() { return bindingList.begin(); }
    NS_Iterator end() { return bindingList.end(); }


    const StringAtom &name;
    NamespaceBindingList bindingList;

};


// A LocalBindingMap maps names to a list of LocalBindings. Each LocalBinding in the list
// will have the same QualifiedName.name, but (potentially) different QualifiedName.namespace values
typedef HashTable<LocalBindingEntry, const StringAtom &> LocalBindingMap;
typedef HashTable<LocalBindingEntry, const StringAtom &>::Iterator LocalBindingIterator;


class InstanceBindingEntry {
public:
    InstanceBindingEntry(const StringAtom &s) : name(s) { }

    typedef std::pair<Namespace *, InstanceBinding *> NamespaceBinding;
    typedef std::vector<NamespaceBinding> NamespaceBindingList;
    typedef NamespaceBindingList::iterator NS_Iterator;

    NS_Iterator begin() { return bindingList.begin(); }
    NS_Iterator end() { return bindingList.end(); }


    const StringAtom &name;
    NamespaceBindingList bindingList;

};




typedef HashTable<InstanceBindingEntry, const StringAtom &> InstanceBindingMap;
typedef HashTable<InstanceBindingEntry, const StringAtom &>::Iterator InstanceBindingIterator;


// A frame contains bindings defined at a particular scope in a program. A frame is either the top-level system frame, 
// a global object, a package, a function frame, a class, or a block frame
class Frame : public JS2Object {
public:

    Frame(ObjectKind kind) : JS2Object(kind) { }

    virtual void instantiate(Environment * /*env*/)  { ASSERT(false); }

    virtual void markChildren()     { }
    virtual ~Frame()                { }

};

class WithFrame : public Frame {
public:
    WithFrame(JS2Object *b) : Frame(WithFrameKind), obj(b) { }
    virtual ~WithFrame()    { }

    virtual void markChildren()     { GCMARKOBJECT(obj); }

    JS2Object *obj;
};

class NonWithFrame : public Frame {
public:

    NonWithFrame(ObjectKind kind) : Frame(kind), frameSlots(NULL), pluralFrame(NULL) { }
    NonWithFrame(ObjectKind kind, NonWithFrame *pluralFrame) : Frame(kind), frameSlots(NULL), pluralFrame(pluralFrame) { }

    LocalBindingMap localBindings;              // Map of qualified names to members defined in this frame

    ValueList *frameSlots;                      // temporaries or frame variables allocted in this frame
    uint16 allocateSlot();

    virtual void instantiate(Environment * /*env*/)  { ASSERT(false); }

    NonWithFrame *pluralFrame;                  // for a singular frame, this is the plural frame from which it will be instantiated

    virtual void markChildren();
    virtual ~NonWithFrame();
};

// The top-level frame containing predefined constants, functions, and classes.
class SystemFrame : public NonWithFrame {
public:
    SystemFrame() : NonWithFrame(SystemKind) { }
    virtual ~SystemFrame()            { }
};


// Environments contain the bindings that are visible from a given point in the source code. An ENVIRONMENT is 
// a list of two or more frames. Each frame corresponds to a scope. More specific frames are listed first
// -each frame's scope is directly contained in the following frame's scope. The last frame is always the
// SYSTEMFRAME. The next-to-last frame is always a PACKAGE or GLOBAL frame.
typedef std::deque<Frame *> FrameList;
typedef FrameList::iterator FrameListIterator;

// Deriving from JS2Object for gc sake only
class Environment : public JS2Object {
public:
    Environment(SystemFrame *systemFrame, Frame *nextToLast) : JS2Object(EnvironmentKind) 
        { 
            frameList.push_back(nextToLast); 
            frameList.push_back(systemFrame);  
        }
    virtual ~Environment()                  { }

    Environment(Environment *e) : JS2Object(EnvironmentKind), frameList(e->frameList) { }
    
    JS2Class *getEnclosingClass();
    ParameterFrame *Environment::getEnclosingParameterFrame(js2val *thisP);
    FrameListIterator getRegionalFrame();
    FrameListIterator getRegionalEnvironment();
    Frame *getTopFrame()                    { return frameList.front(); }
    FrameListIterator getBegin()            { return frameList.begin(); }
    FrameListIterator getEnd()              { return frameList.end(); }
    Package *getPackageFrame();
    SystemFrame *getSystemFrame()           { return checked_cast<SystemFrame *>(frameList.back()); }

    void setTopFrame(Frame *f)              { while (frameList.front() != f) frameList.pop_front(); }

    void addFrame(Frame *f)                 { frameList.push_front(f); }
    void removeTopFrame()                   { frameList.pop_front(); }

    js2val readImplicitThis(JS2Metadata *meta);
    void lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase, js2val *rval, js2val *base);
    void lexicalWrite(JS2Metadata *meta, Multiname *multiname, js2val newValue, bool createIfMissing);
    void lexicalInit(JS2Metadata *meta, Multiname *multiname, js2val newValue);
    bool lexicalDelete(JS2Metadata *meta, Multiname *multiname, Phase phase);

    void instantiateFrame(NonWithFrame *pluralFrame, NonWithFrame *singularFrame, bool buildSlots);

    void markChildren();

    uint32 getSize()            { return frameList.size(); }

private:
    FrameList frameList;
};


class JS2Class : public NonWithFrame {
public:
    JS2Class(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name);

    const StringAtom &getTypeofString()             { return typeofString; }
        
    JS2Class    *super;                         // This class's immediate superclass or null if none
    InstanceBindingMap instanceBindings;        // Map of qualified names to instance members defined in this class    
    InstanceVariable **instanceInitOrder;       // List of instance variables defined in this class in the order in which they are initialised
    bool complete;                              // true after all members of this class have been added to this CLASS record
    const StringAtom &name;                     // This class's name
    js2val prototype;                           // The default value of the super field of newly created simple instances of this class; <none> for most classes
    const StringAtom &typeofString;             // A strign to return if typeof is invoked on this class's instances
    Namespace *privateNamespace;                // This class's private namespace
    bool dynamic;                               // true if this class or any of its ancestors was defined with the dynamic attribute
    bool final;                                 // true if this class cannot be subclassed
    js2val  defaultValue;                       // An instance of this class assigned when a variable is not explicitly initialized

    Callor *call;                               // A procedure to call when this class is used in a call expression
    Constructor *construct;                     // A procedure to call when this class is used in a new expression

    FunctionInstance *init;                     // A procedure to call to initialize a newly created instance of this
                                                // class or null if no special initialization is needed. 'init' is 
                                                // called by 'construct'

    void emitDefaultValue(BytecodeContainer *bCon, size_t pos);

    bool ReadPublic(JS2Metadata *meta, js2val *base, const StringAtom &name, Phase phase, js2val *rval);
    bool WritePublic(JS2Metadata *meta, js2val base, const StringAtom &name, bool createIfMissing, js2val newValue);
    bool DeletePublic(JS2Metadata *meta, js2val base, const StringAtom &name, bool *result);

    virtual bool Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval);
    virtual bool Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag);
    virtual bool Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result);
    virtual bool BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval);
    virtual bool BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue);
    virtual bool BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result);
    virtual js2val ImplicitCoerce(JS2Metadata *meta, js2val newValue);
    virtual js2val Is(JS2Metadata *meta, js2val newValue);

    bool isAncestor(JS2Class *heir);

    uint32 slotCount;

    virtual void instantiate(Environment * /* env */)  { }      // nothing to do
    virtual void markChildren();
    virtual ~JS2Class();
};

class JS2ArrayClass : public JS2Class {
public:
    JS2ArrayClass(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name)
        : JS2Class(super, proto, privateNamespace, dynamic, final, name) { }

    virtual bool Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval);
    virtual bool Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag);
    virtual bool Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result);
    virtual bool BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval);
    virtual bool BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue);
    virtual bool BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result);
};

class JS2IntegerClass : public JS2Class {
public:
    JS2IntegerClass(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name)
        : JS2Class(super, proto, privateNamespace, dynamic, final, name) { }

    virtual js2val ImplicitCoerce(JS2Metadata *meta, js2val newValue);
    virtual js2val Is(JS2Metadata *meta, js2val newValue);
};

class JS2StringClass : public JS2Class {
public:
    JS2StringClass(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name)
        : JS2Class(super, proto, privateNamespace, dynamic, final, name) { }

    virtual bool BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval);
};

class JS2NullClass : public JS2Class {
public:
    JS2NullClass(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name)
        : JS2Class(super, proto, privateNamespace, dynamic, final, name) { }
    
    virtual bool Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval)           { return false; }
    virtual bool BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval)                           { return false; }
    virtual bool Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag) { return false; }
    virtual bool BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue)                                     { return false; }
    virtual bool Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result)                       { return false; }
    virtual bool BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result)                                       { return false; }
};

class JS2ArgumentsClass : public JS2Class {
public:
    JS2ArgumentsClass(JS2Class *super, js2val proto, Namespace *privateNamespace, bool dynamic, bool final, const StringAtom &name)
        : JS2Class(super, proto, privateNamespace, dynamic, final, name) { }
    
    virtual bool Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval);
    virtual bool Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag);
    virtual bool Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result);
};

class Package : public NonWithFrame {
public:
    typedef enum { InTransit, InHand } PackageStatus;

    Package(const String &name, Namespace *internal) : NonWithFrame(PackageKind), super(JS2VAL_VOID), sealed(false), internalNamespace(internal), name(name) { }
    js2val super;
    bool sealed;
    Namespace *internalNamespace;               // This Package's internal namespace
    PackageStatus status;
    String name;
    
    virtual void markChildren();
    virtual ~Package()            { }
};


// A SLOT record describes the value of one fixed property of one instance.
class Slot {
public:
// We keep the slotIndex in the InstanceVariable rather than go looking for a specific id
//    InstanceVariable *id;        // The instance variable whose value this slot carries
    js2val value;                // This fixed property's current value; uninitialised if the fixed property is an uninitialised constant
};

class ParameterFrame;

class FunctionWrapper {
public:
    FunctionWrapper(JS2Metadata *meta, bool unchecked, ParameterFrame *compileFrame, Environment *env) 
        : bCon(new BytecodeContainer()), 
            code(NULL), 
            alien(NULL), 
            unchecked(unchecked), 
            compileFrame(compileFrame), 
            env(new (meta) Environment(env)), 
            length(0),
            resultType(NULL) 
        { }

    FunctionWrapper(JS2Metadata *meta, bool unchecked, ParameterFrame *compileFrame, NativeCode *code, Environment *env) 
        : bCon(NULL), 
            code(code), 
            alien(NULL), 
            unchecked(unchecked), 
            compileFrame(compileFrame), 
            env(new (meta) Environment(env)), 
            length(0),
            resultType(NULL)
        { }

    FunctionWrapper(JS2Metadata *meta, bool unchecked, ParameterFrame *compileFrame, AlienCode *code, Environment *env) 
        : bCon(NULL), 
            code(NULL),
            alien(code), 
            unchecked(unchecked), 
            compileFrame(compileFrame), 
            env(new (meta) Environment(env)), 
            length(0),
            resultType(NULL)
        { }

    virtual ~FunctionWrapper()  { if (bCon) delete bCon; }

    BytecodeContainer   *bCon;
    NativeCode          *code;
    AlienCode           *alien;
    bool                unchecked;          // true if the function is untyped, non-method, normal
    ParameterFrame      *compileFrame;
    Environment         *env;
    uint32              length;
    JS2Class            *resultType;
};


// Instances which do not respond to the function call or new operators are represented as SIMPLEINSTANCE records
class SimpleInstance : public JS2Object {
public:
    SimpleInstance(JS2Metadata *meta, js2val parent, JS2Class *type);

    LocalBindingMap     localBindings;      // Map of qualified names to local properties (including dynamic properties, if any)
    js2val              super;              // Optional link to the next object in this instance's prototype chain
    bool                sealed;             // If true, no more local properties may be added to this instance
    JS2Class            *type;              // This instance's type
    Slot                *fixedSlots;        // A set of slots that hold this instance's fixed property values

    void initializeSlots(JS2Class *type);

    virtual void markChildren();
    virtual ~SimpleInstance();
};

// A LimitedInstance represents an intermediate result of a 'super' or 'super(expr)' subexpression
class LimitedInstance : public JS2Object {
public:
    LimitedInstance(JS2Object *instance, JS2Class *limit) : JS2Object(LimitedInstanceKind), instance(instance), limit(limit) { }

    JS2Object   *instance;          // The value of 'expr' to whoch the super subexpression was applied; if 'expr'
                                    // wasn't given, defaults to the value of 'this'. The value of instance is
                                    // always an instance of one of the limit class's descendants.
    JS2Class    *limit;             // The immediate superclass of the class inside which the 'super' subexpression
                                    // was applied.
};

// Date instances are simple instances created by the Date class, they have an extra field 
// that contains the millisecond count
class DateInstance : public SimpleInstance {
public:
    DateInstance(JS2Metadata *meta, js2val parent, JS2Class *type) : SimpleInstance(meta, parent, type) { }

    float64     ms;
    virtual ~DateInstance()              { }
};

// String instances are simple instances created by the String class, they have an extra field 
// that contains the string data
class StringInstance : public SimpleInstance {
public:
    StringInstance(JS2Metadata *meta, js2val parent, JS2Class *type) : SimpleInstance(meta, parent, type), mValue(NULL) { }

    String     *mValue;             // has been allocated by engine in the GC'able Pond

    virtual void markChildren()     { SimpleInstance::markChildren(); if (mValue) JS2Object::mark(mValue); }
    virtual ~StringInstance()            { }
};

// Number instances are simple instances created by the Number class, they have an extra field 
// that contains the float64 data
class NumberInstance : public SimpleInstance {
public:
    NumberInstance(JS2Metadata *meta, js2val parent, JS2Class *type) : SimpleInstance(meta, parent, type), mValue(0.0) { }

    float64     mValue;
    virtual ~NumberInstance()            { }
};

// Boolean instances are simple instances created by the Boolean class, they have an extra field 
// that contains the bool data
class BooleanInstance : public SimpleInstance {
public:
    BooleanInstance(JS2Metadata *meta, js2val parent, JS2Class *type) : SimpleInstance(meta, parent, type), mValue(false) { }

    bool     mValue;
    virtual ~BooleanInstance()           { }
};

// Function instances are SimpleInstances created by the Function class, they have an extra field 
// that contains a pointer to the function implementation
class FunctionInstance : public SimpleInstance {
public:
    FunctionInstance(JS2Metadata *meta, js2val parent, JS2Class *type);

    FunctionWrapper     *fWrap;

    bool                isMethodClosure;        // if true, use the thisObject from below
    js2val              thisObject;
    const String *sourceText;

    virtual void markChildren();
    virtual ~FunctionInstance();
};

// Array instances are SimpleInstances created by the Array class, they 
// maintain the value of the 'length' property when 'indexable' elements
// are added.
class ArrayInstance : public SimpleInstance {
public:
    ArrayInstance(JS2Metadata *meta, js2val parent, JS2Class *type);
    virtual ~ArrayInstance()             { }

    uint32 length;
};

// RegExp instances are simple instances created by the RegExp class, they have an extra field 
// that contains the RegExp object
class RegExpInstance : public SimpleInstance {
public:
    RegExpInstance(JS2Metadata *meta, js2val parent, JS2Class *type) : SimpleInstance(meta, parent, type), mRegExp(NULL) { }

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

    JS2RegExp  *mRegExp;
    virtual ~RegExpInstance();
};


// Instances of Arguments class:
// The n arguments supplied to a function are also stored in a variable 'arguments' with properties [0] ..[n]
// FrameVariable references from the ParameterFrame are directed to this array because the FrameParameter's
// slot list points here instead.
// When an element in arguments[] is deleted, that splits the alias. New values added afterwards are kept in a 
// separate value list instead.
class ArgumentsInstance : public SimpleInstance {
public:
    ArgumentsInstance(JS2Metadata *meta, js2val parent, JS2Class *type) 
                        : SimpleInstance(meta, parent, type), mSlots(NULL), count(0), mSplit(NULL), mSplitValue(NULL) { }

    js2val *mSlots;
    uint32 count;
    bool *mSplit;
    js2val *mSplitValue;

    virtual void markChildren();
    virtual ~ArgumentsInstance();
};




// A helper class for 'for..in' statements
class ForIteratorObject : public JS2Object {
public:
    ForIteratorObject(JS2Object *obj) : JS2Object(ForIteratorKind), obj(obj), nameList(NULL) { }

    bool first(JS2Engine *engine);
    bool next(JS2Engine *engine);

    js2val value(JS2Engine *engine);

    JS2Object *obj;
    JS2Object *originalObj;

    virtual void markChildren()     { GCMARKOBJECT(obj); GCMARKOBJECT(originalObj); }
    virtual ~ForIteratorObject()    { }

private:

    bool buildNameList(JS2Metadata *meta);

    const String **nameList;
    uint32 it;
    uint32 length;
};

// Base class for all references (lvalues)
// References are generated during the eval stage (bytecode generation), but shouldn't live beyond that
// so they're allocated from an arena carried in the metadata class and cleared after each bytecode gen pass.
class Reference : public ArenaObject {
public:
//    virtual ~Reference() { }
    virtual void emitReadBytecode(BytecodeContainer *, size_t)              { ASSERT(false); }
    virtual void emitWriteBytecode(BytecodeContainer *, size_t)             { ASSERT(false); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *, size_t)     { ASSERT(false); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *, size_t)  { ASSERT(false); }
    virtual void emitWriteBackBytecode(BytecodeContainer *, size_t)         { ASSERT(false); }

    virtual void emitPostIncBytecode(BytecodeContainer *, size_t)           { ASSERT(false); }
    virtual void emitPostDecBytecode(BytecodeContainer *, size_t)           { ASSERT(false); }
    virtual void emitPreIncBytecode(BytecodeContainer *, size_t)            { ASSERT(false); }
    virtual void emitPreDecBytecode(BytecodeContainer *, size_t)            { ASSERT(false); }

    virtual void emitDeleteBytecode(BytecodeContainer *, size_t)            { ASSERT(false); }   
    
    // indicate whether building the reference generates any stack deposits
    virtual int hasStackEffect()                                            { ASSERT(false); return 0; }

};

class LexicalReference : public Reference {
// A LEXICALREFERENCE tuple has the fields below and represents an lvalue that refers to a variable with one
// of a given set of qualified names. LEXICALREFERENCE tuples arise from evaluating identifiers a and qualified identifiers
// q::a.
public:
    LexicalReference(Multiname *mname, bool strict, BytecodeContainer *bCon) 
        : variableMultiname(mname), env(NULL), strict(strict), mn_index(bCon->saveMultiname(mname)) { }
//    LexicalReference(const String *name, bool strict) : variableMultiname(name), env(NULL), strict(strict) { }
//    LexicalReference(const String *name, Namespace *nameSpace, bool strict) : variableMultiname(name, nameSpace), env(NULL), strict(strict) { }
//    virtual ~LexicalReference() { }
    
    Multiname *variableMultiname;   // A nonempty set of qualified names to which this reference can refer
    Environment *env;               // The environment in which the reference was created.
    bool strict;                    // The strict setting from the context in effect at the point where the reference was created

    uint16 mn_index;
    
    void emitInitBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eLexicalInit, pos); bCon->addShort(mn_index); }
    
    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eLexicalRead, pos); bCon->addShort(mn_index); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eLexicalWrite, pos); bCon->addShort(mn_index); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eLexicalRef, pos); bCon->addShort(mn_index); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eLexicalPostInc, pos); bCon->addShort(mn_index); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eLexicalPostDec, pos); bCon->addShort(mn_index); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eLexicalPreInc, pos); bCon->addShort(mn_index); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eLexicalPreDec, pos); bCon->addShort(mn_index); }
    
    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eLexicalDelete, pos); bCon->addShort(mn_index); }

    virtual int hasStackEffect()                                           { return 0; }
};

class DotReference : public Reference {
// A DOTREFERENCE tuple has the fields below and represents an lvalue that refers to a property of the base
// object with one of a given set of qualified names. DOTREFERENCE tuples arise from evaluating subexpressions such as a.b or
// a.q::b.
public:
//    DotReference(const String *name) : propertyMultiname(name) { }
    DotReference(Multiname *mn, BytecodeContainer *bCon) : propertyMultiname(mn), mn_index(bCon->saveMultiname(propertyMultiname)) { }
//    virtual ~DotReference() { }

    // In this implementation, the base is established by the execution of the preceding expression and
    // is available on the execution stack, not in the reference object (which is a codegen-time only)
    // js2val base;                 // The object whose property was referenced (a in the examples above). The
                                    // object may be a LIMITEDINSTANCE if a is a super expression, in which case
                                    // the property lookup will be restricted to members defined in proper ancestors
                                    // of base.limit.
    Multiname *propertyMultiname;   // A nonempty set of qualified names to which this reference can refer (b
                                    // qualified with the namespace q or all currently open namespaces in the
                                    // example above)
    uint16 mn_index;

    
    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eDotRead, pos); bCon->addShort(mn_index); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eDotWrite, pos); bCon->addShort(mn_index); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eDotRef, pos); bCon->addShort(mn_index); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadForInvokeBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eDotPostInc, pos); bCon->addShort(mn_index); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eDotPostDec, pos); bCon->addShort(mn_index); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eDotPreInc, pos); bCon->addShort(mn_index); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eDotPreDec, pos); bCon->addShort(mn_index); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eDotDelete, pos); bCon->addShort(mn_index); }

    virtual int hasStackEffect()                                            { return 1; }
};

class SlotReference : public Reference {
// A special case of a DotReference with an Sl instead of a D
public:
    SlotReference(uint32 slotIndex) : slotIndex(slotIndex) { }
//    virtual ~SlotReference()    { }

    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eSlotRead, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eSlotWrite, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eSlotRef, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eSlotPostInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eSlotPostDec, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eSlotPreInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eSlotPreDec, pos); bCon->addShort((uint16)slotIndex); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eFalse, pos); /* bCon->emitOp(eSlotDelete, pos); bCon->addShort((uint16)slotIndex); */ }

    uint32 slotIndex;
    virtual int hasStackEffect()                                            { return 1; }
};

class FrameSlotReference : public Reference {
// A special case of a DotReference with an FrameSl instead of a D
public:
    FrameSlotReference(uint32 slotIndex) : slotIndex(slotIndex) { }
//    virtual ~FrameSlotReference()	{ }

    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eFrameSlotRead, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eFrameSlotWrite, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eFrameSlotRef, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eFrameSlotPostInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eFrameSlotPostDec, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eFrameSlotPreInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eFrameSlotPreDec, pos); bCon->addShort((uint16)slotIndex); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eFalse, pos); /* bCon->emitOp(eFrameSlotDelete, pos); bCon->addShort((uint16)slotIndex); */ }

    uint32 slotIndex;
    virtual int hasStackEffect()                                            { return 0; }
};

class ParameterSlotReference : public Reference {
// A special case of a DotReference with an ParameterSl instead of a D
public:
    ParameterSlotReference(uint32 slotIndex) : slotIndex(slotIndex) { }
//    virtual ~ParameterSlotReference()	{ }

    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(eParameterSlotRead, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eParameterSlotWrite, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(eParameterSlotRef, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eParameterSlotPostInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(eParameterSlotPostDec, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eParameterSlotPreInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eParameterSlotPreDec, pos); bCon->addShort((uint16)slotIndex); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eFalse, pos); /* bCon->emitOp(ePackageSlotDelete, pos); bCon->addShort((uint16)slotIndex); */ }

    uint32 slotIndex;
    virtual int hasStackEffect()                                            { return 0; }
};

class PackageSlotReference : public Reference {
// A special case of a DotReference with an PackageSl instead of a D
public:
    PackageSlotReference(uint32 slotIndex) : slotIndex(slotIndex) { }
//    virtual ~PackageSlotReference()	{ }

    virtual void emitReadBytecode(BytecodeContainer *bCon, size_t pos)      { bCon->emitOp(ePackageSlotRead, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitWriteBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(ePackageSlotWrite, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForInvokeBytecode(BytecodeContainer *bCon, size_t pos)     { bCon->emitOp(ePackageSlotRef, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitReadForWriteBackBytecode(BytecodeContainer *bCon, size_t pos)  { emitReadBytecode(bCon, pos); }
    virtual void emitWriteBackBytecode(BytecodeContainer *bCon, size_t pos)         { emitWriteBytecode(bCon, pos); }

    virtual void emitPostIncBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(ePackageSlotPostInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPostDecBytecode(BytecodeContainer *bCon, size_t pos)   { bCon->emitOp(ePackageSlotPostDec, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreIncBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(ePackageSlotPreInc, pos); bCon->addShort((uint16)slotIndex); }
    virtual void emitPreDecBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(ePackageSlotPreDec, pos); bCon->addShort((uint16)slotIndex); }

    virtual void emitDeleteBytecode(BytecodeContainer *bCon, size_t pos)    { bCon->emitOp(eFalse, pos); /* bCon->emitOp(ePackageSlotDelete, pos); bCon->addShort((uint16)slotIndex); */ }

    uint32 slotIndex;
    virtual int hasStackEffect()                                            { return 0; }
};


class NamedArgument {
public:
    const String *name;               // This argument's name
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
    virtual int hasStackEffect()                                            { return 2; }
//    virtual ~BracketReference()     { }
};


// Frames holding bindings for invoked functions
class ParameterFrame : public NonWithFrame {
public:
    ParameterFrame(js2val thisObject, bool prototype) 
        : NonWithFrame(ParameterFrameKind), 
                thisObject(thisObject), 
                prototype(prototype), 
                buildArguments(false),
                isConstructor(false),
                isInstance(false),
                callsSuperConstructor(false),
                superConstructorCalled(true),
                argSlots(NULL),
                argSlotsOwner(false)
                    { }
/*
    ParameterFrame(ParameterFrame *pluralFrame) 
        : NonWithFrame(ParameterFrameKind, pluralFrame), 
                thisObject(JS2VAL_UNDEFINED), 
                prototype(pluralFrame->prototype), 
                buildArguments(pluralFrame->buildArguments),
                isConstructor(pluralFrame->isConstructor),
                isInstance(pluralFrame->isInstance),
                callsSuperConstructor(pluralFrame->callsSuperConstructor),
                superConstructorCalled(false), // initialized to false for each construction of a singular frame
                                               // and then set true when/if the call occurs
                argSlots(NULL)
                    { }
*/

//    Plurality plurality;
    js2val thisObject;              // The value of this; none if this function doesn't define this;
                                    // inaccessible if this function defines this but the value is not 
                                    // available because this function hasn't been called yet.

    bool prototype;                 // true if this function is not an instance method but defines this anyway
    bool buildArguments;            // true if the 'arguments' variable is referenced, it will be constructed
                                    // by 'assignArguments' and contain the slots for this frame.
    bool isConstructor;
    bool isInstance;
    bool callsSuperConstructor;
    bool superConstructorCalled;

    // these relate to the 'most active' invocation
    js2val *argSlots;               // if the corresponding function uses 'arguments', this pointer is borrowed
                                    // from the arguments instance.
    uint32 argCount;                // may be more than frameSlots->size() if more arguments are passed
    bool argSlotsOwner;




//    Variable **positional;          // list of positional parameters, in order
//    uint32 positionalCount;

    virtual void instantiate(Environment *env);
    void assignArguments(JS2Metadata *meta, JS2Object *fnObj, js2val *argBase, uint32 argCount);
    void releaseArgs();
    virtual void markChildren();
    virtual ~ParameterFrame();
};

class BlockFrame : public NonWithFrame {
public:
    BlockFrame() : NonWithFrame(BlockFrameKind), isFunctionFrame(false) { }
    BlockFrame(BlockFrame *pluralFrame) : NonWithFrame(BlockFrameKind, pluralFrame), isFunctionFrame(false) { }

    Plurality plurality;
    bool isFunctionFrame;

    virtual void instantiate(Environment *env);
    virtual ~BlockFrame()           { }
};


typedef std::vector<Namespace *> NamespaceList;
typedef NamespaceList::iterator NamespaceListIterator;

// A CONTEXT carries static information about a particular point in the source program.
class Context {
public:
    Context() : strict(false), E3compatibility(true) { }
    bool strict;                    // true if strict mode is in effect
    bool E3compatibility;
    NamespaceList openNamespaces;   // The set of namespaces that are open at this point. 
                                    // The public namespace is always a member of this set.
};

// The 'true' attribute
class TrueAttribute : public Attribute {
public:
    TrueAttribute() : Attribute(TrueAttr) { }
    virtual CompoundAttribute *toCompoundAttribute(JS2Metadata *meta);
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

    NamespaceList namespaces;       // The set of namespaces contained in this attribute
    bool xplicit;                   // true if the explicit attribute has been given
    bool enumerable;                // true if the enumerable attribute has been given
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
    LocalMember *localMember;
    Namespace *ns;
};

class CompilationData {
public:
    BytecodeContainer *compilation_bCon;
    BytecodeContainer *execution_bCon;
};

class JS2Metadata {
public:
    
    JS2Metadata(World &world);
    virtual ~JS2Metadata();

    CompilationData *startCompilationUnit(BytecodeContainer *newBCon, const String &source, const String &sourceLocation);
    void restoreCompilationUnit(CompilationData *oldData);


    void ValidateStmtList(StmtNode *p);
    js2val EvalStmtList(Phase phase, StmtNode *p);

    js2val readEvalString(const String &str, const String& fileName);
    js2val readEvalFile(const String& fileName);
    js2val readEvalFile(const char *fileName);

// XXX - passing (Context *cxt, Environment *env) throughout - but do these really change?

    void ValidateStmtList(Context *cxt, Environment *env, Plurality pl, StmtNode *p);
    void ValidateTypeExpression(Context *cxt, Environment *env, ExprNode *e)    { ValidateExpression(cxt, env, e); } 
    void ValidateStmt(Context *cxt, Environment *env, Plurality pl, StmtNode *p);
    void ValidateExpression(Context *cxt, Environment *env, ExprNode *p);
    void ValidateAttributeExpression(Context *cxt, Environment *env, ExprNode *p);
    FunctionInstance *validateStaticFunction(Context *cxt, Environment *env, FunctionDefinition *fnDef, bool prototype, bool unchecked, bool isConstructor, size_t pos);

    void validateStatic(Context *cxt, Environment *env, FunctionDefinition *fnDef, CompoundAttribute *a, bool unchecked, bool hoisted, size_t pos);
    void validateConstructor(Context *cxt, Environment *env, FunctionDefinition *fnDef, JS2Class *c, CompoundAttribute *a, size_t pos);
    void validateInstance(Context *cxt, Environment *env, FunctionDefinition *fnDef, JS2Class *c, CompoundAttribute *a, bool final, size_t pos);

    js2val ExecuteStmtList(Phase phase, StmtNode *p);
    js2val EvalExpression(Environment *env, Phase phase, ExprNode *p);
    JS2Class *EvalTypeExpression(Environment *env, Phase phase, ExprNode *p);
    Reference *SetupExprNode(Environment *env, Phase phase, ExprNode *p, JS2Class **exprType);
    Attribute *EvalAttributeExpression(Environment *env, Phase phase, ExprNode *p);
    void SetupStmt(Environment *env, Phase phase, StmtNode *p);


    JS2Class *objectType(js2val obj);
    JS2Class *objectType(JS2Object *obj);
    bool hasType(js2val objVal, JS2Class *c);

    LocalMember *findFlatMember(NonWithFrame *container, Multiname *multiname, Access access, Phase phase);
    InstanceBinding *resolveInstanceMemberName(JS2Class *js2class, Multiname *multiname, Access access, Phase phase, QualifiedName *qname);

    FrameVariable *makeFrameVariable(NonWithFrame *regionalFrame);
    LocalMember *defineHoistedVar(Environment *env, const StringAtom &id, js2val initVal, bool isVar, size_t pos);
    Multiname *defineLocalMember(Environment *env, const StringAtom &id, NamespaceList *namespaces, Attribute::OverrideModifier overrideMod, bool xplicit, Access access, LocalMember *m, size_t pos, bool enumerable);
    InstanceMember *defineInstanceMember(JS2Class *c, Context *cxt, const StringAtom &idAtom, NamespaceList &namespaces, 
                                                                    Attribute::OverrideModifier overrideMod, bool xplicit,
                                                                    InstanceMember *m, size_t pos);
    InstanceMember *searchForOverrides(JS2Class *c, Multiname *multiname, Access access, size_t pos);
    InstanceMember *findInstanceMember(JS2Class *c, QualifiedName *qname, Access access);
    Slot *findSlot(js2val thisObjVal, InstanceVariable *id);
    LocalMember *findLocalMember(JS2Object *container, Multiname *multiname, Access access, bool &enumerable);
    js2val getSuperObject(JS2Object *obj);
    JS2Class *getVariableType(Variable *v, Phase phase, size_t pos);
    InstanceMember *getDerivedInstanceMember(JS2Class *c, InstanceMember *mBase, Access access);
    InstanceMember *findBaseInstanceMember(JS2Class *limit, Multiname *multiname, Access access);
    InstanceMember *findLocalInstanceMember(JS2Class *limit, Multiname *multiname, Access access);
    Member *findCommonMember(js2val *base, Multiname *multiname, Access access, bool flat);

    js2val invokeFunction(const char *fname);
    bool invokeFunctionOnObject(js2val thisValue, const StringAtom &fnName, js2val &result);
    js2val invokeFunction(JS2Object *fnObj, js2val thisValue, js2val *argv, uint32 argc, ParameterFrame *runtimeFrame);
    void invokeInit(JS2Class *c, js2val thisValue, js2val* argv, uint32 argc);

    DynamicVariable *createDynamicProperty(JS2Object *obj, const char *name, js2val initVal, Access access, bool sealed, bool enumerable)
    {
        return createDynamicProperty(obj, world.identifiers[widenCString(name)], initVal, access, sealed, enumerable);
    }
    DynamicVariable *createDynamicProperty(JS2Object *obj, const StringAtom &name, js2val initVal, Access access, bool sealed, bool enumerable);
    void addPublicVariableToLocalMap(LocalBindingMap *lMap, const StringAtom &name, LocalMember *v, Access access, bool enumerable);

    FunctionInstance *createFunctionInstance(Environment *env, bool prototype, bool unchecked, NativeCode *code, uint32 length, DynamicVariable **lengthProperty);


    bool readLocalMember(LocalMember *m, Phase phase, js2val *rval, Frame *container);
    bool readInstanceMember(js2val containerVal, JS2Class *c, InstanceMember *mBase, Phase phase, js2val *rval);
    bool JS2Metadata::hasOwnProperty(JS2Object *obj, const StringAtom &name);

    bool writeLocalMember(LocalMember *m, js2val newValue, bool initFlag, Frame *container);
    bool writeInstanceMember(js2val containerVal, JS2Class *c, InstanceMember *mBase, js2val newValue);

    bool deleteLocalMember(LocalMember *m, bool *result);
    bool deleteInstanceMember(JS2Class *c, QualifiedName *qname, bool *result);

    void addGlobalObjectFunction(char *name, NativeCode *code, uint32 length);
    void initBuiltinClass(JS2Class *builtinClass, FunctionData *staticFunctions, NativeCode *construct, NativeCode *call);
    void initBuiltinClassPrototype(JS2Class *builtinClass, FunctionData *protoFunctions);

    void reportError(Exception::Kind kind, const char *message, size_t pos, const char *arg = NULL);
    void reportError(Exception::Kind kind, const char *message, size_t pos, const String &name);
    void reportError(Exception::Kind kind, const char *message, size_t pos, const String *name);

    const String *convertValueToString(js2val x);
    js2val convertValueToPrimitive(js2val x, Hint hint);
    float64 convertValueToDouble(js2val x);
    float64 convertStringToDouble(const String *str);
    bool convertValueToBoolean(js2val x);
    int32 convertValueToInt32(js2val x);
    uint32 convertValueToUInt32(js2val x);
    float64 convertValueToInteger(js2val x);
    js2val convertValueToGeneralNumber(js2val x);
    js2val convertValueToObject(js2val x);
    float64 valToFloat64(js2val x);

    const String *toString(js2val x)    { if (JS2VAL_IS_STRING(x)) return JS2VAL_TO_STRING(x); else return convertValueToString(x); }
    js2val toPrimitive(js2val x, Hint hint)        { if (JS2VAL_IS_PRIMITIVE(x)) return x; else return convertValueToPrimitive(x, hint); }
    float64 toFloat64(js2val x)         { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else return valToFloat64(x); }
    js2val toGeneralNumber(js2val x)    { if (JS2VAL_IS_NUMBER(x)) return x; else return convertValueToGeneralNumber(x); }
    bool toBoolean(js2val x)            { if (JS2VAL_IS_BOOLEAN(x)) return JS2VAL_TO_BOOLEAN(x); else return convertValueToBoolean(x); }
    float64 toInteger(js2val x)         { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else return convertValueToInteger(x); }
    int32 valToInt32(js2val x)          { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else return convertValueToInt32(x); }
    uint32 valToUInt32(js2val x)        { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else return convertValueToUInt32(x); }
    js2val toObject(js2val x)           { if (JS2VAL_IS_OBJECT(x)) return x; else return convertValueToObject(x); }
    // x is a General Number
    int64 truncateToInteger(js2val x)   { if (JS2VAL_IS_INT(x)) return JS2VAL_TO_INT(x); else return JS2Engine::float64toInt64(toFloat64(x)); }

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

    LocalMember *forbiddenMember;  // just need one of these hanging around

    // The base classes:
    JS2Class *objectClass;
    JS2Class *undefinedClass;
    JS2Class *nullClass;
    JS2Class *booleanClass;
    JS2Class *generalNumberClass;
    JS2Class *numberClass;
    JS2Class *mathClass;
    JS2Class *integerClass;
    JS2Class *characterClass;
    JS2Class *stringClass;
    JS2Class *namespaceClass;
    JS2Class *attributeClass;
    JS2Class *classClass;
    JS2Class *functionClass;
    JS2Class *packageClass;
    JS2Class *dateClass;
    JS2Class *regexpClass;
    JS2Class *arrayClass;
    JS2Class *errorClass;
    JS2Class *evalErrorClass;
    JS2Class *rangeErrorClass;
    JS2Class *referenceErrorClass;
    JS2Class *syntaxErrorClass;
    JS2Class *typeErrorClass;
    JS2Class *uriErrorClass;
    JS2Class *argumentsClass;

    BytecodeContainer *bCon;        // the current output container

    typedef std::vector<BytecodeContainer *> BConList;
    typedef std::vector<BytecodeContainer *>::iterator BConListIterator;
    BConList bConList;

    Package *glob;
    Environment *env;
    Context cxt;

    enum Flag { JS1, JS2 };
    Flag flags;
    int version;

    TargetList targetList;          // stack of potential break/continue targets

    bool showTrees;                 // debug only, causes parse tree dump 

    Arena *referenceArena;          // allocation arena for all reference objects

    typedef std::vector<Package *> PackageList;
    PackageList packages;           // the currently loaded packages, packages.back() is the current package

    bool checkForPackage(const String &packageName, size_t pos);    // return true if loaded, throw exception if loading
    void loadPackage(const String &packageName, const String &filename);  // load package from file
    String getPackageName(IdentifierList *packageIdList);

    // iterates over contained gc allocated objects
    void markChildren();


    Pond pond;
    std::list<RootKeeper *> rootList;

    RootIterator addRoot(RootKeeper *t);
    uint32 gc();
    void clear();
    void removeRoot(RootIterator ri);

    void *alloc(size_t s, PondScum::ScumFlag flag);
    void unalloc(void *p);


};

    DEFINE_ROOTKEEPER_CONSTRUCTOR(JS2Object)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(RegExpInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(CompoundAttribute)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(const String)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(String)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(ArrayInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(BooleanInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(StringInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(JS2Metadata)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(Environment)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(Multiname)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(ParameterFrame)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(SimpleInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(FunctionInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(DateInstance)
    DEFINE_ROOTKEEPER_CONSTRUCTOR(ArgumentsInstance)
#ifdef DEBUG
    inline RootKeeper::RootKeeper(JS2Metadata *meta, js2val *p, int line, char *pfile) : is_js2val(true), js2val_count(0), p(p), meta(meta)    { init(meta, line, pfile); }
    inline RootKeeper::RootKeeper(JS2Metadata *meta, js2val **p, uint32 count, int line, char *pfile) : is_js2val(true), js2val_count(count), p(p), meta(meta)    { init(meta, line, pfile); }
    inline RootKeeper::~RootKeeper() { meta->removeRoot(ri); delete file; }
    inline void RootKeeper::init(JS2Metadata *meta, int ln, char *pfile)
    {
        line = ln;
        file = new char[strlen(pfile) + 1];
        strcpy(file, pfile);
        ri = meta->addRoot(this);
    }
#else
    inline RootKeeper::RootKeeper(JS2Metadata *meta, js2val *p) : is_js2val(true), js2val_count(0), p(p), meta(meta)    { ri = meta->addRoot(this); }
    inline RootKeeper::RootKeeper(JS2Metadata *meta, js2val **p, uint32 count) : is_js2val(true), js2val_count(count), p(p), meta(meta)    { ri = meta->addRoot(this); }
    inline RootKeeper::~RootKeeper() { meta->removeRoot(ri); }
#endif

    inline void *JS2Object::operator new(size_t s, JS2Metadata *meta)    { return meta->alloc(s, PondScum::JS2ObjectFlag); }



    inline char narrow(char16 ch) { return char(ch); }

    inline void Setter::mark()              { GCMARKOBJECT(type); GCMARKOBJECT(code); }
    inline void Getter::mark()              { GCMARKOBJECT(type); GCMARKOBJECT(code); }

    inline void InstanceGetter::mark()      { InstanceMember::mark(); GCMARKOBJECT(type); GCMARKOBJECT(fInst); }
    inline void InstanceSetter::mark()      { InstanceMember::mark(); GCMARKOBJECT(type); GCMARKOBJECT(fInst); }

    inline void InstanceMember::mark()      { GCMARKOBJECT(multiname); }
    
}; // namespace MetaData

inline bool operator==(MetaData::LocalBindingEntry &s1, const StringAtom &s2) { return s1.name == s2;}
inline bool operator!=(MetaData::LocalBindingEntry &s1, const StringAtom &s2) { return s1.name != s2;}

inline bool operator==(MetaData::InstanceBindingEntry &s1, const StringAtom &s2) { return s1.name == s2;}
inline bool operator!=(MetaData::InstanceBindingEntry &s1, const StringAtom &s2) { return s1.name != s2;}

inline HashNumber hashString(const StringAtom &s) { return s.hash; }


}; // namespace Javascript

using namespace JavaScript;
using namespace MetaData;

#endif
