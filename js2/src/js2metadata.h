
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

#include "jsapi.h"
#include "jsstr.h"

#include "world.h"
#include "utilities.h"
#include "parser.h"

#include <map>

namespace JavaScript {
namespace MetaData {


// forward definitions:
class JS2Metadata;
class JS2Class;
class StaticBinding;
class Environment;
class Context;
typedef jsval js2val;

typedef void (Invokable)();
typedef Invokable Callor;
typedef Invokable Constructor;


enum Phase  { CompilePhase, RunPhase };
enum Access { ReadAccess, WriteAccess, ReadWriteAccess };

class JS2Object {
// Every object is either undefined, null, a Boolean,
// a number, a string, a namespace, a compound attribute, a class, a method closure, 
// a prototype instance, a class instance, a package object, or the global object.
public:
    void *operator new(size_t s);
};

class Attribute : public JS2Object {
public:
    enum AttributeKind { TrueKind, FalseKind, NamespaceKind, CompoundKind } kind;
    enum MemberModifier { NoModifier, Static, Constructor, Operator, Abstract, Virtual, Final};
    enum OverrideModifier { NoOverride, DoOverride, DontOverride, OverrideUndefined };


    Attribute(AttributeKind kind) : kind(kind) { }

    static Attribute *combineAttributes(Attribute *a, Attribute *b);

#ifdef DEBUG
    virtual void uselessVirtual()   { } // want the checked_cast stuff to work, so need a virtual function
#endif
};

// A Namespace (is also an attribute)
class Namespace : public Attribute {
public:
    Namespace(StringAtom &name) : Attribute(NamespaceKind), name(name) { }

    StringAtom &name;       // The namespace's name used by toString
};


class QualifiedName {
public:
    QualifiedName(Namespace *nameSpace, const StringAtom &id) : nameSpace(nameSpace), id(id) { }

    bool operator ==(const QualifiedName &b) { return (nameSpace == b.nameSpace) && (id == b.id); }

    Namespace *nameSpace;    // The namespace qualifier
    const StringAtom &id;    // The name
};

// MULTINAME is the semantic domain of sets of qualified names. Multinames are used internally in property lookup.
// We keep Multinames as a basename and a list of namespace qualifiers
typedef std::vector<Namespace *> NamespaceList;
typedef NamespaceList::iterator NamespaceListIterator;
class Multiname {
public:
    
    Multiname(StringAtom &name) : name(name) { }
    void addNamespace(Namespace *ns)    { nsList.push_back(ns); }

    bool matches(QualifiedName &q)      { return (name == q.id) && onList(q.nameSpace); }
    bool onList(Namespace *nameSpace);

    NamespaceList nsList;
    StringAtom &name;
};


class Object_Uninit_Future {
public:
    enum { Object, Uninitialized, Future } state;
    js2val value;
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

// A static member is either forbidden, a variable, a hoisted variable, a constructor method, or an accessor:
class StaticMember {
public:
    enum StaticMemberKind { Forbidden, Variable, HoistedVariable, ConstructorMethod, Accessor } kind;
};

class Variable : public StaticMember {
public:
    JS2Class *type;                 // Type of values that may be stored in this variable
    js2val value;                   // This variable's current value; future if the variable has not been declared yet;
                                    // uninitialised if the variable must be written before it can be read
    bool immutable;                 // true if this variable's value may not be changed once set
};

class HoistedVar : public StaticMember {
public:
    HoistedVar() : value(JSVAL_VOID), hasFunctionInitializer(false) { }
    js2val value;                   // This variable's current value
    bool hasFunctionInitializer;    // true if this variable was created by a function statement
};

class StaticMethod : public StaticMember {
public:
    Signature type;         // This function's signature
    Invokable *code;        // This function itself (a callable object)
    enum { Static, Constructor }
        modifier;           // static if this is a function or a static method; constructor if this is a constructor for a class
};

class Accessor : public StaticMember {
public:
    JS2Class *type;         // The type of the value read from the getter or written into the setter
    Invokable *code;        // calling this object does the read or write
};


// DYNAMICPROPERTY record describes one dynamic property of one (prototype or class) instance.
typedef std::map<String, js2val> DynamicPropertyMap;
typedef DynamicPropertyMap::iterator DynamicPropertyIterator;

// A STATICBINDING describes the member to which one qualified name is bound in a frame. Multiple 
// qualified names may be bound to the same member in a frame, but a qualified name may not be 
// bound to multiple members in a frame (except when one binding is for reading only and 
// the other binding is for writing only).
class StaticBinding {
public:
    StaticBinding(QualifiedName &qname, StaticMember *content) : qname(qname), content(content), xplicit(false) { }

    QualifiedName qname;        // The qualified name bound by this binding
    StaticMember *content;      // The member to which this qualified name was bound
    bool xplicit;               // true if this binding should not be imported into the global scope by an import statement
};


class InstanceMember {
public:
    bool final;             // true if this member may not be overridden in subclasses
};

class InstanceVariable : public InstanceMember {
public:
    JS2Class *type;                 // Type of values that may be stored in this variable
    Invokable *evalInitialValue;    // A function that computes this variable's initial value
    bool immutable;                 // true if this variable's value may not be changed once set
    bool final;
};

class InstanceMethod : public InstanceMember {
public:
    Signature type;         // This method's signature
    Invokable *code;        // This method itself (a callable object); null if this method is abstract
};

class InstanceAccessor : public InstanceMember {
public:
    JS2Class *type;         // The type of the value read from the getter or written into the setter
    Invokable *code;        // A callable object which does the read or write; null if this method is abstract
};

class InstanceBinding {
public:
    QualifiedName qname;         // The qualified name bound by this binding
    InstanceMember *content;     // The member to which this qualified name was bound
};


// A StaticBindingMap maps names to a list of StaticBindings. Each StaticBinding in the list
// will have the same QualifiedName.name, but (potentially) different QualifiedName.namespace values
typedef std::multimap<String, StaticBinding *> StaticBindingMap;
typedef StaticBindingMap::iterator StaticBindingIterator;

class InstanceBindingMap {
public:
};

// A frame contains bindings defined at a particular scope in a program. A frame is either the top-level system frame, 
// a global object, a package, a function frame, a class, or a block frame
class Frame {
public:
    enum Plurality { Singular, Plural };
    enum FrameKind { System, GlobalObject, Package, Function, Class, Block };

    Frame(FrameKind kind) : kind(kind), nextFrame(NULL) { }

    StaticBindingMap staticReadBindings;        // Map of qualified names to readable static members defined in this frame
    StaticBindingMap staticWriteBindings;       // Map of qualified names to writable static members defined in this frame

    FrameKind kind;             // [rather than use RTTI (in general)]
    Frame *nextFrame;
#ifdef DEBUG
    virtual void uselessVirtual()   { } // want the checked_cast stuff to work, so need a virtual function
#endif
};


class JS2Class : public Frame {
public:
    JS2Class() : Frame(Class) { }
        
    InstanceBindingMap instanceReadBindings;    // Map of qualified names to readable instance members defined in this class    
    InstanceBindingMap instanceWriteBindings;   // Map of qualified names to writable instance members defined in this class    

    InstanceVariable **instanceInitOrder;       // List of instance variables defined in this class in the order in which they are initialised

    bool complete;                              // true after all members of this class have been added to this CLASS record

    JS2Class    *super;                         // This class's immediate superclass or null if none
    JS2Object   *prototype;                     // An object that serves as this class's prototype for compatibility with ECMAScript 3; may be null

    Namespace *privateNamespace;                // This class's private namespace

    bool dynamic;                               // true if this class or any of its ancestors was defined with the dynamic attribute
    bool primitive;                             // true if this class was defined with the primitive attribute
    bool final;                                 // true if this class cannot be subclassed

    Callor call;                                // A procedure to call when this class is used in a call expression
    Constructor construct;                      // A procedure to call when this class is used in a new expression

};

class GlobalObject : public Frame {
public:
    GlobalObject(World &world) : Frame(Frame::GlobalObject), internalNamespace(new Namespace(world.identifiers["internal"])) { }

    Namespace *internalNamespace;               // This global object's internal namespace
    DynamicPropertyMap dynamicProperties;       // A set of this global object's dynamic properties
};


// A SLOT record describes the value of one fixed property of one instance.
class Slot {
public:
    InstanceVariable *id;        // The instance variable whose value this slot carries
    js2val value;                // This fixed property's current value; uninitialised if the fixed property is an uninitialised constant
};

// Instances of non-dynamic classes are represented as FIXEDINSTANCE records. These instances can contain only fixed properties.
class FixedInstance {
public:
    JS2Class    *type;          // This instance's type
    Invokable   *call;          // A procedure to call when this instance is used in a call expression
    Invokable   *construct;     // A procedure to call when this instance is used in a new expression
    Environment *env;           // The environment to pass to the call or construct procedure
    StringAtom  &typeofString;  // A string to return if typeof is invoked on this instance
    Slot        *slots;         // A set of slots that hold this instance's fixed property values
};

// Instances of dynamic classes are represented as DYNAMICINSTANCE records. These instances can contain fixed and dynamic properties.
class DynamicInstance {
public:
    JS2Class    *type;          // This instance's type
    Invokable   *call;          // A procedure to call when this instance is used in a call expression
    Invokable   *construct;     // A procedure to call when this instance is used in a new expression
    Environment *env;           // The environment to pass to the call or construct procedure
    StringAtom  &typeofString;  // A string to return if typeof is invoked on this instance
    Slot        *slots;         // A set of slots that hold this instance's fixed property values
    DynamicPropertyMap dynamicProperties; // A set of this instance's dynamic properties
};

class LexicalReference : public JS2Object {
// A LEXICALREFERENCE tuple has the fields below and represents an lvalue that refers to a variable with one
// of a given set of qualified names. LEXICALREFERENCE tuples arise from evaluating identifiers a and qualified identifiers
// q::a.
public:
    LexicalReference(Multiname *vm, Environment *env, bool strict) : variableMultiname(vm), env(env), strict(strict) { }
    Multiname *variableMultiname;   // A nonempty set of qualified names to which this reference can refer
    Environment *env;               // The environment in which the reference was created.
    bool strict;                    // The strict setting from the context in effect at the point where the reference was created
    
};

class DotReference : public JS2Object {
// A DOTREFERENCE tuple has the fields below and represents an lvalue that refers to a property of the base
// object with one of a given set of qualified names. DOTREFERENCE tuples arise from evaluating subexpressions such as a.b or
// a.q::b.
public:
    jsval base;                     // The object whose property was referenced (a in the examples above). The
                                    // object may be a LIMITEDINSTANCE if a is a super expression, in which case
                                    // the property lookup will be restricted to members defined in proper ancestors
                                    // of base.limit.
    Multiname propertyMultiname;    // A nonempty set of qualified names to which this reference can refer (b
                                    // qualified with the namespace q or all currently open namespaces in the
                                    // example above)
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


class BracketReference : public JS2Object {
// A BRACKETREFERENCE tuple has the fields below and represents an lvalue that refers to the result of
// applying the [] operator to the base object with the given arguments. BRACKETREFERENCE tuples arise from evaluating
// subexpressions such as a[x] or a[x,y].
public:
    jsval base;                     // The object whose property was referenced (a in the examples above). The object may be a
                                    // LIMITEDINSTANCE if a is a super expression, in which case the property lookup will be
                                    // restricted to definitions of the [] operator defined in proper ancestors of base.limit.
    ArgumentList args;              // The list of arguments between the brackets (x or x,y in the examples above)
};


// The top-level frame containing predefined constants, functions, and classes.
class SystemFrame : public Frame {
public:
    SystemFrame() : Frame(System) { }
};

// Frames holding bindings for invoked functions
class FunctionFrame : public Frame {
public:
    FunctionFrame() : Frame(Function) { }

    Plurality plurality;
    jsval thisObject;               // The value of this; none if this function doesn't define this;
                                    // inaccessible if this function defines this but the value is not 
                                    // available because this function hasn't been called yet.

                                    // Here we use NULL as no this and VOID as inaccessible

    bool prototype;                 // true if this function is not an instance method but defines this anyway
};

class BlockFrame : public Frame {
public:
    BlockFrame() : Frame(Block) { }

    Plurality plurality;
};


class LookupKind {
public:
    LookupKind(bool isLexical, jsval thisObject) : isLexical(isLexical), thisObject(thisObject) { }
    bool isLexical;         // if isLexical, use the 'this' below. Otherwise it's a propertyLookup
    jsval thisObject;
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
    Frame *getTopFrame()            { return firstFrame; }

    jsval findThis(bool allowPrototypeThis);
    jsval lexicalRead(JS2Metadata *meta, Multiname *multiname, Phase phase);


private:
    Frame *firstFrame;
};

typedef std::vector<Namespace *> NamespaceList;
typedef NamespaceList::iterator NamespaceListIterator;

// A CONTEXT carries static information about a particular point in the source program.
class Context {
public:
    Context() : strict(true) { }
    Context(Context *cxt);
    bool strict;                    // true if strict mode is in effect
    NamespaceList openNamespaces;   // The set of namespaces that are open at this point. 
                                    // The public namespace is always a member of this set.
};

// The 'true' attribute
class TrueAttribute : public Attribute {
public:
    TrueAttribute() : Attribute(TrueKind) { }
};

// The 'false' attribute
class FalseAttribute : public Attribute {
public:
    FalseAttribute() : Attribute(FalseKind) { }
};

// Compound attribute objects are all values obtained from combining zero or more syntactic attributes 
// that are not Booleans or single namespaces. 
class CompoundAttribute : public Attribute {
public:
    CompoundAttribute();
    void addNamespace(Namespace *n);

    NamespaceList *namespaces;      // The set of namespaces contained in this attribute
    bool xplicit;                   // true if the explicit attribute has been given
    bool dynamic;                   // true if the dynamic attribute has been given
    MemberModifier memberMod;       // if one of these attributes has been given; none if not.
    OverrideModifier overrideMod;   // if the override attribute  with one of these arguments was given; 
                                    // true if the attribute override without arguments was given; none if the override attribute was not given.
    bool prototype;                 // true if the prototype attribute has been given
    bool unused;                    // true if the unused attribute has been given
};

class JS2Metadata {
public:
    
    JS2Metadata(World &world);

    void setCurrentParser(Parser *parser) { mParser = parser; }

    void ValidateStmtList(StmtNode *p);
    jsval EvalStmtList(Phase phase, StmtNode *p);


    void ValidateStmtList(Context *cxt, Environment *env, StmtNode *p);
    void ValidateTypeExpression(ExprNode *e);
    void ValidateStmt(Context *cxt, Environment *env, StmtNode *p);
    void defineHoistedVar(Environment *env, const StringAtom &id, StmtNode *p);
    void ValidateExpression(Context *cxt, Environment *env, ExprNode *p);
    void ValidateAttributeExpression(Context *cxt, Environment *env, ExprNode *p);

    jsval EvalStmtList(Environment *env, Phase phase, StmtNode *p);
    jsval EvalExpression(Environment *env, Phase phase, ExprNode *p);
    bool EvalExprNode(Environment *env, Phase phase, ExprNode *p, String &s);
    Attribute *EvalAttributeExpression(Environment *env, Phase phase, ExprNode *p);
    jsval EvalStmt(Environment *env, Phase phase, StmtNode *p);


    JS2Class *objectType(jsval obj);

    bool readProperty(jsval container, Multiname *multiname, LookupKind *lookupKind, Phase phase, jsval *rval);
    bool readProperty(Frame *pf, Multiname *multiname, LookupKind *lookupKind, Phase phase, jsval *rval);
    StaticMember *findFlatMember(Frame *container, Multiname *multiname, Access access, Phase phase);

    bool readDynamicProperty(Frame *container, Multiname *multiname, LookupKind *lookupKind, Phase phase);
    bool readStaticMember(StaticMember *m, Phase phase);



    void reportError(Exception::Kind kind, const char *message, size_t pos, const char *arg = NULL);
    void reportError(Exception::Kind kind, const char *message, size_t pos, const String& name);


    void initializeMonkey();
    jsval execute(String *str, size_t pos);

    // Used for interning strings
    World &world;

    // The one and only 'public' namespace
    Namespace *publicNamespace;

    // The base classes:
    JS2Class *undefinedClass;
    JS2Class *nullClass;
    JS2Class *booleanClass;
    JS2Class *numberClass;
    JS2Class *characterClass;
    JS2Class *stringClass;

    
    Parser *mParser;                // used for error reporting
    size_t errorPos;


    GlobalObject glob;
    Environment env;
    Context cxt;

    // SpiderMonkey execution data:
    JSRuntime *monkeyRuntime;
    JSContext *monkeyContext;
    JSObject *monkeyGlobalObject;

};


}; // namespace MetaData
}; // namespace Javascript