
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


#include "world.h"
#include "utilities.h"


namespace JavaScript {
namespace MetaData {


// forward definitions:
class JS2Class;
typedef uint32 js2val;

typedef void (Invokable)();
typedef Invokable Callor;
typedef Invokable Constructor;

class JS2Object {
// Every object is either undefined, null, a Boolean,
// a number, a string, a namespace, a compound attribute, a class, a method closure, 
// a prototype instance, a class instance, a package object, or the global object.
public:
};

class Namespace {
public:
    StringAtom &name;       // The namespace's name used by toString
};


class QualifiedName {
public:
    Namespace nameSpace;    // The namespace qualifier
    StringAtom &id;         // The name
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

class StaticMember {
public:
};

class Variable : public StaticMember {
public:
    JS2Class *type;                 // Type of values that may be stored in this variable
    Object_Uninit_Future value;     // This variable's current value; future if the variable has not been declared yet;
                                    // uninitialised if the variable must be written before it can be read
    bool immutable;                 // true if this variable's value may not be changed once set
};

class HoistedVar : public StaticMember {
public:
    js2val value;                   // This variable's current value
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




class StaticBinding {
public:
    QualifiedName qname;        // The qualified name bound by this binding
    StaticMember content;       // The member to which this qualified name was bound
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
    QualifiedName qname;        // The qualified name bound by this binding
    InstanceMember *content;     // The member to which this qualified name was bound
};

class StaticBindingMap {
public:
};

class InstanceBindingMap {
public:
};

class JS2Class {
public:
        
    StaticBindingMap staticReadBindings;        // Map of qualified names to readable static members defined in this class
    StaticBindingMap staticWriteBindings;       // Map of qualified names to writable static members defined in this class

    InstanceBindingMap instanceReadBindings;    // Map of qualified names to readable instance members defined in this class    
    InstanceBindingMap instanceWriteBindings;   // Map of qualified names to writable instance members defined in this class    

    InstanceVariable **instanceInitOrder;       // List of instance variables defined in this class in the order in which they are initialised

    bool complete;                              // true after all members of this class have been added to this CLASS record

    JS2Class    *super;                         // This class's immediate superclass or null if none
    JS2Object   *prototype;                     // An object that serves as this class's prototype for compatibility with ECMAScript 3; may be null

    Namespace privateNamespace;                 // This class's private namespace

    bool dynamic;                               // true if this class or any of its ancestors was defined with the dynamic attribute
    bool primitive;                             // true if this class was defined with the primitive attribute
    bool final;                                 // true if this class cannot be subclassed

    Callor call;                                // A procedure to call when this class is used in a call expression
    Constructor construct;                      // A procedure to call when this class is used in a new expression

};




class LexicalReference {
// A LEXICALREFERENCE tuple has the fields below and represents an lvalue that refers to a variable with one
// of a given set of qualified names. LEXICALREFERENCE tuples arise from evaluating identifiers a and qualified identifiers
// q::a.
public:
    Environment *env;               // The environment in which the reference was created.
    Multiname variableMultiname;    // A nonempty set of qualified names to which this reference can refer
    Context *cxt;                   // The context in effect at the point where the reference was created
};

class DotReference {
// A DOTREFERENCE tuple has the fields below and represents an lvalue that refers to a property of the base
// object with one of a given set of qualified names. DOTREFERENCE tuples arise from evaluating subexpressions such as a.b or
// a.q::b.
public:
    JS2Object *base;                // The object whose property was referenced (a in the examples above). The
                                    // object may be a LIMITEDINSTANCE if a is a super expression, in which case
                                    // the property lookup will be restricted to members defined in proper ancestors
                                    // of base.limit.
    Multiname propertyMultiname;    // A nonempty set of qualified names to which this reference can refer (b
                                    // qualified with the namespace q or all currently open namespaces in the
                                    // example above)
};

class BracketReference {
// A BRACKETREFERENCE tuple has the fields below and represents an lvalue that refers to the result of
// applying the [] operator to the base object with the given arguments. BRACKETREFERENCE tuples arise from evaluating
// subexpressions such as a[x] or a[x,y].
public:
    JS2Object *base;                // The object whose property was referenced (a in the examples above). The object may be a
                                    // LIMITEDINSTANCE if a is a super expression, in which case the property lookup will be
                                    // restricted to definitions of the [] operator defined in proper ancestors of base.limit.
    ArgumentList args;              // The list of arguments between the brackets (x or x,y in the examples above)
};




}; // namespace MetaData
}; // namespace Javascript