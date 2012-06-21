/*
Distributed under both the W3C Test Suite License [1] and the W3C
3-clause BSD License [2]. To contribute to a W3C Test Suite, see the
policies and contribution forms [3].

[1] http://www.w3.org/Consortium/Legal/2008/04-testsuite-license
[2] http://www.w3.org/Consortium/Legal/2008/03-bsd-license
[3] http://www.w3.org/2004/10/27-testcases
*/

/*
 * This file automatically generates browser tests for WebIDL interfaces, using
 * the testharness.js framework.  To use, first include the following:
 *
 *   <script src=/resources/testharness.js></script>
 *   <script src=/resources/testharnessreport.js></script>
 *   <script src=/resources/WebIDLParser.js></script>
 *   <script src=/resources/idlharness.js></script>
 *
 * Then you'll need some type of IDLs.  Here's some script that can be run on a
 * spec written in HTML, which will grab all the elements with class="idl",
 * concatenate them, and replace the body so you can copy-paste:
 *
     var s = "";
     [].forEach.call(document.getElementsByClassName("idl"), function(idl) {
       //https://www.w3.org/Bugs/Public/show_bug.cgi?id=14914
       if (!idl.classList.contains("extract"))
       {
         s += idl.textContent + "\n\n";
       }
     });
     document.body.innerHTML = '<pre></pre>';
     document.body.firstChild.textContent = s;
 *
 * (TODO: write this in Python or something so that it can be done from the
 * command line instead.)
 *
 * Once you have that, put it in your script somehow.  The easiest way is to
 * embed it literally in an HTML file with <script type=text/plain> or similar,
 * so that you don't have to do any escaping.  Another possibility is to put it
 * in a separate .idl file that's fetched via XHR or similar.  Sample usage:
 *
 *   var idl_array = new IdlArray();
 *   idl_array.add_untested_idls("interface Node { readonly attribute DOMString nodeName; };");
 *   idl_array.add_idls("interface Document : Node { readonly attribute DOMString URL; };");
 *   idl_array.add_objects({Document: ["document"]});
 *   idl_array.test();
 *
 * This tests that window.Document exists and meets all the requirements of
 * WebIDL.  It also tests that window.document (the result of evaluating the
 * string "document") has URL and nodeName properties that behave as they
 * should, and otherwise meets WebIDL's requirements for an object whose
 * primary interface is Document.  It does not test that window.Node exists,
 * which is what you want if the Node interface is already tested in some other
 * specification's suite and your specification only extends or refers to it.
 * Of course, each IDL string can define many different things, and calls to
 * add_objects() can register many different objects for different interfaces:
 * this is a very simple example.
 *
 * TODO: Write assert_writable, assert_enumerable, assert_configurable and
 * their inverses, and use those instead of just checking
 * getOwnPropertyDescriptor.
 *
 * == Public methods of IdlArray ==
 *
 * IdlArray objects can be obtained with new IdlArray().  Anything not
 * documented in this section should be considered an implementation detail,
 * and outside callers should not use it.
 *
 * add_idls(idl_string):
 *   Parses idl_string (throwing on parse error) and adds the results to the
 *   IdlArray.  All the definitions will be tested when you run test().  If
 *   some of the definitions refer to other definitions, those must be present
 *   too.  For instance, if idl_string says that Document inherits from Node,
 *   the Node interface must also have been provided in some call to add_idls()
 *   or add_untested_idls().
 *
 * add_untested_idls(idl_string):
 *   Like add_idls(), but the definitions will not be tested.  If an untested
 *   interface is added and then extended with a tested partial interface, the
 *   members of the partial interface will still be tested.  Also, all the
 *   members will still be tested for objects added with add_objects(), because
 *   you probably want to test that (for instance) window.document has all the
 *   properties from Node, not just Document, even if the Node interface itself
 *   is tested in a different test suite.
 *
 * add_objects(dict):
 *   dict should be an object whose keys are the names of interfaces or
 *   exceptions, and whose values are arrays of strings.  When an interface or
 *   exception is tested, every string registered for it with add_objects()
 *   will be evaluated, and tests will be run on the result to verify that it
 *   correctly implements that interface or exception.  This is the only way to
 *   test anything about [NoInterfaceObject] interfaces, and there are many
 *   tests that can't be run on any interface without an object to fiddle with.
 *
 *   The interface has to be the *primary* interface of all the objects
 *   provided.  For example, don't pass {Node: ["document"]}, but rather
 *   {Document: ["document"]}.  Assuming the Document interface was declared to
 *   inherit from Node, this will automatically test that document implements
 *   the Node interface too.
 *
 *   Warning: methods will be called on any provided objects, in a manner that
 *   WebIDL requires be safe.  For instance, if a method has mandatory
 *   arguments, the test suite will try calling it with too few arguments to
 *   see if it throws an exception.  If an implementation incorrectly runs the
 *   function instead of throwing, this might have side effects, possibly even
 *   preventing the test suite from running correctly.
 *
 * prevent_multiple_testing(name):
 *   This is a niche method for use in case you're testing many objects that
 *   implement the same interfaces, and don't want to retest the same
 *   interfaces every single time.  For instance, HTML defines many interfaces
 *   that all inherit from HTMLElement, so the HTML test suite has something
 *   like
 *     .add_objects({
 *         HTMLHtmlElement: ['document.documentElement'],
 *         HTMLHeadElement: ['document.head'],
 *         HTMLBodyElement: ['document.body'],
 *         ...
 *     })
 *   and so on for dozens of element types.  This would mean that it would
 *   retest that each and every one of those elements implements HTMLElement,
 *   Element, and Node, which would be thousands of basically redundant tests.
 *   The test suite therefore calls prevent_multiple_testing("HTMLElement").
 *   This means that once one object has been tested to implement HTMLElement
 *   and its ancestors, no other object will be.  Thus in the example code
 *   above, the harness would test that document.documentElement correctly
 *   implements HTMLHtmlElement, HTMLElement, Element, and Node; but
 *   document.head would only be tested for HTMLHeadElement, and so on for
 *   further objects.
 *
 * test():
 *   Run all tests.  This should be called after you've called all other
 *   methods to add IDLs and objects.
 */
"use strict";
(function(){
var interfaces = {};

/// IdlArray ///
//Entry point
window.IdlArray = function()
//@{
{
    this.members = {};
    this.objects = {};
    // When adding multiple collections of IDLs one at a time, an earlier one
    // might contain a partial interface or implements statement that depends
    // on a later one.  Save these up and handle them right before we run
    // tests.
    this.partials = [];
    this.implements = {};
}

//@}
IdlArray.prototype.add_idls = function(raw_idls)
//@{
{
    this.internal_add_idls(WebIDLParser.parse(raw_idls));
};

//@}
IdlArray.prototype.add_untested_idls = function(raw_idls)
//@{
{
    var parsed_idls = WebIDLParser.parse(raw_idls);
    for (var i = 0; i < parsed_idls.length; i++)
    {
        parsed_idls[i].untested = true;
        if ("members" in parsed_idls[i])
        {
            for (var j = 0; j < parsed_idls[i].members.length; j++)
            {
                parsed_idls[i].members[j].untested = true;
            }
        }
    }
    this.internal_add_idls(parsed_idls);
}

//@}
IdlArray.prototype.internal_add_idls = function(parsed_idls)
//@{
{
    parsed_idls.forEach(function(parsed_idl)
    {
        if (parsed_idl.type == "partialinterface")
        {
            this.partials.push(parsed_idl);
            return;
        }

        if (parsed_idl.type == "implements")
        {
            if (!(parsed_idl.target in this.implements))
            {
                this.implements[parsed_idl.target] = [];
            }
            this.implements[parsed_idl.target].push(parsed_idl.implements);
            return;
        }

        parsed_idl.array = this;
        if (parsed_idl.name in this.members)
        {
            throw "Duplicate identifier " + parsed_idl.name;
        }
        switch(parsed_idl.type)
        {
        case "interface":
            this.members[parsed_idl.name] = new IdlInterface(parsed_idl);
            break;

        case "exception":
            this.members[parsed_idl.name] = new IdlException(parsed_idl);
            break;

        case "dictionary":
            //Nothing to test, but we need the dictionary info around for type
            //checks
            this.members[parsed_idl.name] = new IdlDictionary(parsed_idl);
            break;

        case "typedef":
            //TODO
            break;

        case "enum":
            //TODO
            break;

        default:
            throw parsed_idl.name + ": " + parsed_idl.type + " not yet supported";
        }
    }.bind(this));
}

//@}
IdlArray.prototype.add_objects = function(dict)
//@{
{
    for (var k in dict)
    {
        if (k in this.objects)
        {
            this.objects[k] = this.objects[k].concat(dict[k]);
        }
        else
        {
            this.objects[k] = dict[k];
        }
    }
}

//@}
IdlArray.prototype.prevent_multiple_testing = function(name)
//@{
{
    this.members[name].prevent_multiple_testing = true;
}

//@}
IdlArray.prototype.recursively_get_implements = function(interface_name)
//@{
{
    var ret = this.implements[interface_name];
    if (ret === undefined)
    {
        return [];
    }
    for (var i = 0; i < this.implements[interface_name].length; i++)
    {
        ret = ret.concat(this.recursively_get_implements(ret[i]));
        if (ret.indexOf(ret[i]) != ret.lastIndexOf(ret[i]))
        {
            throw "Circular implements statements involving " + ret[i];
        }
    }
    return ret;
}

//@}
IdlArray.prototype.test = function()
//@{
{
    this.partials.forEach(function(parsed_idl)
    {
        if (!(parsed_idl.name in this.members)
        || !(this.members[parsed_idl.name] instanceof IdlInterface))
        {
            throw "Partial interface " + parsed_idl.name + " with no original interface";
        }
        if (parsed_idl.extAttrs)
        {
            parsed_idl.extAttrs.forEach(function(extAttr)
            {
                this.members[parsed_idl.name].extAttrs.push(extAttr);
            }.bind(this));
        }
        parsed_idl.members.forEach(function(member)
        {
            this.members[parsed_idl.name].members.push(new IdlInterfaceMember(member));
        }.bind(this));
    }.bind(this));
    this.partials = [];

    for (var lhs in this.implements)
    {
        this.recursively_get_implements(lhs).forEach(function(rhs)
        {
            if (!(lhs in this.members)
            || !(this.members[lhs] instanceof IdlInterface)
            || !(rhs in this.members)
            || !(this.members[rhs] instanceof IdlInterface))
            {
                throw lhs + " implements " + rhs + ", but one is undefined or not an interface";
            }
            this.members[rhs].members.forEach(function(member)
            {
                this.members[lhs].members.push(new IdlInterfaceMember(member));
            }.bind(this));
        }.bind(this));
    }
    this.implements = {};

    for (var name in this.members)
    {
        this.members[name].test();
        if (name in this.objects)
        {
            this.objects[name].forEach(function(str)
            {
                this.members[name].test_object(str);
            }.bind(this));
        }
    }
};

//@}
IdlArray.prototype.assert_type_is = function(value, type)
//@{
{
    if (type.idlType == "any")
    {
        //No assertions to make
        return;
    }

    if (type.nullable && value === null)
    {
        //This is fine
        return;
    }

    if (type.array)
    {
        //TODO: not supported yet
        return;
    }

    if (type.sequence)
    {
        assert_true(Array.isArray(value), "is not array");
        if (!type.idlType.length)
        {
            return;
        }
        type = type.idlType[0];
    }
    else
    {
        type = type.idlType;
    }

    switch(type)
    {
        case "void":
            assert_equals(value, undefined);
            return;

        case "boolean":
            assert_equals(typeof value, "boolean");
            return;

        case "byte":
            assert_equals(typeof value, "number");
            assert_equals(value, Math.floor(value), "not an integer");
            assert_true(-128 <= value && value <= 127, "byte " + value + " not in range [-128, 127]");
            return;

        case "octet":
            assert_equals(typeof value, "number");
            assert_equals(value, Math.floor(value), "not an integer");
            assert_true(0 <= value && value <= 255, "octet " + value + " not in range [0, 255]");
            return;

        case "short":
            assert_equals(typeof value, "number");
            assert_equals(value, Math.floor(value), "not an integer");
            assert_true(-32768 <= value && value <= 32767, "short " + value + " not in range [-32768, 32767]");
            return;

        case "unsigned short":
            assert_equals(typeof value, "number");
            assert_equals(value, Math.floor(value), "not an integer");
            assert_true(0 <= value && value <= 65535, "unsigned short " + value + " not in range [0, 65535]");
            return;

        case "long":
            assert_equals(typeof value, "number");
            assert_equals(value, Math.floor(value), "not an integer");
            assert_true(-2147483648 <= value && value <= 2147483647, "long " + value + " not in range [-2147483648, 2147483647]");
            return;

        case "unsigned long":
            assert_equals(typeof value, "number");
            assert_equals(value, Math.floor(value), "not an integer");
            assert_true(0 <= value && value <= 4294967295, "unsigned long " + value + " not in range [0, 4294967295]");
            return;

        case "long long":
            assert_equals(typeof value, "number");
            return;

        case "unsigned long long":
            assert_equals(typeof value, "number");
            assert_true(0 <= value, "unsigned long long is negative");
            return;

        case "float":
        case "double":
            //TODO: distinguish these cases
            assert_equals(typeof value, "number");
            return;

        case "DOMString":
            assert_equals(typeof value, "string");
            return;

        case "object":
            assert_true(typeof value == "object" || typeof value == "function", "wrong type: not object or function");
            return;
    }

    if (!(type in this.members))
    {
        throw "Unrecognized type " + type;
    }

    if (this.members[type] instanceof IdlInterface)
    {
        //We don't want to run the full
        //IdlInterface.prototype.test_instance_of, because that could result in
        //an infinite loop.  TODO: This means we don't have tests for
        //NoInterfaceObject interfaces, and we also can't test objects that
        //come from another window.
        assert_true(typeof value == "object" || typeof value == "function", "wrong type: not object or function");
        if (value instanceof Object
        && !this.members[type].has_extended_attribute("NoInterfaceObject")
        && type in window)
        {
            assert_true(value instanceof window[type], "not instanceof " + type);
        }
    }
    else if (this.members[type] instanceof IdlDictionary)
    {
        //TODO: Test when we actually have something to test this on
    }
    else
    {
        throw "Type " + type + " isn't an interface or dictionary";
    }
};
//@}

/// IdlObject ///
function IdlObject() {}
IdlObject.prototype.has_extended_attribute = function(name)
//@{
{
    return this.extAttrs.some(function(o)
    {
        return o.name == name;
    });
};

//@}
IdlObject.prototype.test = function() {};

/// IdlDictionary ///
//Used for IdlArray.prototype.assert_type_is
function IdlDictionary(obj)
//@{
{
    this.name = obj.name;
    this.members = obj.members ? obj.members : [];
    this.inheritance = obj.inheritance ? obj.inheritance: [];
}

//@}
IdlDictionary.prototype = Object.create(IdlObject.prototype);

/// IdlException ///
function IdlException(obj)
//@{
{
    this.name = obj.name;
    this.array = obj.array;
    this.untested = obj.untested;
    this.extAttrs = obj.extAttrs ? obj.extAttrs : [];
    this.members = obj.members ? obj.members.map(function(m){return new IdlInterfaceMember(m)}) : [];
    this.inheritance = obj.inheritance ? obj.inheritance : [];
}

//@}
IdlException.prototype = Object.create(IdlObject.prototype);
IdlException.prototype.test = function()
//@{
{
    // Note: largely copy-pasted from IdlInterface, but then, so is the spec
    // text.
    if (this.has_extended_attribute("NoInterfaceObject"))
    {
        //No tests to do without an instance
        return;
    }

    if (!this.untested)
    {
        this.test_self();
    }
    this.test_members();
}

//@}
IdlException.prototype.test_self = function()
//@{
{
    test(function()
    {
        //"For every exception that is not declared with the
        //[NoInterfaceObject] extended attribute, a corresponding property must
        //exist on the exception’s relevant namespace object. The name of the
        //property is the identifier of the exception, and its value is an
        //object called the exception interface object, which provides access
        //to any constants that have been associated with the exception. The
        //property has the attributes { [[Writable]]: true, [[Enumerable]]:
        //false, [[Configurable]]: true }."
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));
        var desc = Object.getOwnPropertyDescriptor(window, this.name);
        assert_false("get" in desc, "window's property " + format_value(this.name) + " has getter");
        assert_false("set" in desc, "window's property " + format_value(this.name) + " has setter");
        assert_true(desc.writable, "window's property " + format_value(this.name) + " is not writable");
        assert_false(desc.enumerable, "window's property " + format_value(this.name) + " is enumerable");
        assert_true(desc.configurable, "window's property " + format_value(this.name) + " is not configurable");

        //"The exception interface object for a given exception must be a
        //function object."
        //"If an object is defined to be a function object, then it has
        //characteristics as follows:"
        //"Its [[Prototype]] internal property is the Function prototype
        //object."
        //Note: This doesn't match browsers as of December 2011, see
        //http://www.w3.org/Bugs/Public/show_bug.cgi?id=14813
        assert_true(Function.prototype.isPrototypeOf(window[this.name]),
                    "prototype of window's property " + format_value(this.name) + " is not Function.prototype");
        //"Its [[Get]] internal property is set as described in ECMA-262
        //section 15.3.5.4."
        //Not much to test for this.
        //"Its [[Construct]] internal property is set as described in ECMA-262
        //section 13.2.2."
        //Tested below.
        //"Its [[HasInstance]] internal property is set as described in
        //ECMA-262 section 15.3.5.3, unless otherwise specified."
        //TODO
        //"Its [[Class]] internal property is “Function”."
        //String() returns something implementation-dependent, because it calls
        //Function#toString.
        assert_equals({}.toString.call(window[this.name]), "[object Function]",
                      "{}.toString.call(" + this.name + ")");

        //TODO: Test 4.9.1.1. Exception interface object [[Call]] method (which
        //does not match browsers: //http://www.w3.org/Bugs/Public/show_bug.cgi?id=14885)
    }.bind(this), this.name + " exception: existence and properties of exception interface object");

    test(function()
    {
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));

        //"The exception interface object must also have a property named
        //“prototype” with attributes { [[Writable]]: false, [[Enumerable]]:
        //false, [[Configurable]]: false } whose value is an object called the
        //exception interface prototype object. This object also provides
        //access to the constants that are declared on the exception."
        assert_own_property(window[this.name], "prototype",
                            'exception "' + this.name + '" does not have own property "prototype"');
        var desc = Object.getOwnPropertyDescriptor(window[this.name], "prototype");
        assert_false("get" in desc, this.name + ".prototype has getter");
        assert_false("set" in desc, this.name + ".prototype has setter");
        assert_false(desc.writable, this.name + ".prototype is writable");
        assert_false(desc.enumerable, this.name + ".prototype is enumerable");
        assert_false(desc.configurable, this.name + ".prototype is configurable");

        //"The exception interface prototype object for a given exception must
        //have an internal [[Prototype]] property whose value is as follows:
        //
        //"If the exception is declared to inherit from another exception, then
        //the value of the internal [[Prototype]] property is the exception
        //interface prototype object for the inherited exception.
        //"Otherwise, the exception is not declared to inherit from another
        //exception. The value of the internal [[Prototype]] property is the
        //Error prototype object ([ECMA-262], section 15.11.3.1)."
        //Note: This doesn't match browsers as of December 2011, see
        //https://www.w3.org/Bugs/Public/show_bug.cgi?id=14887.
        var inherit_exception = this.inheritance.length ? this.inheritance[0] : "Error";
        assert_own_property(window, inherit_exception,
                            'should inherit from ' + inherit_exception + ', but window has no such property');
        assert_own_property(window[inherit_exception], "prototype",
                            'should inherit from ' + inherit_exception + ', but that object has no "prototype" property');
        assert_true(window[inherit_exception].prototype.isPrototypeOf(window[this.name].prototype),
                    'prototype of ' + this.name + '.prototype is not ' + inherit_exception + '.prototype');

        //"The class string of an exception interface prototype object is the
        //concatenation of the exception’s identifier and the string
        //“Prototype”."
        //String() and {}.toString.call() should be equivalent, since nothing
        //defines a stringifier.
        assert_equals({}.toString.call(window[this.name].prototype), "[object " + this.name + "Prototype]",
                      "{}.toString.call(" + this.name + ")");
        assert_equals(String(window[this.name].prototype), "[object " + this.name + "Prototype]",
                      "String(" + this.name + ")");
    }.bind(this), this.name + " exception: existence and properties of exception interface prototype object");

    test(function()
    {
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));
        assert_own_property(window[this.name], "prototype",
                            'interface "' + this.name + '" does not have own property "prototype"');

        //"There must be a property named “name” on the exception interface
        //prototype object with attributes { [[Writable]]: true,
        //[[Enumerable]]: false, [[Configurable]]: true } and whose value is
        //the identifier of the exception."
        assert_own_property(window[this.name].prototype, "name",
                'prototype object does not have own property "name"');
        var desc = Object.getOwnPropertyDescriptor(window[this.name].prototype, "name");
        assert_false("get" in desc, this.name + ".prototype.name has getter");
        assert_false("set" in desc, this.name + ".prototype.name has setter");
        assert_true(desc.writable, this.name + ".prototype.name is not writable");
        assert_false(desc.enumerable, this.name + ".prototype.name is enumerable");
        assert_true(desc.configurable, this.name + ".prototype.name is not configurable");
        assert_equals(desc.value, this.name, this.name + ".prototype.name has incorrect value");
    }.bind(this), this.name + " exception: existence and properties of exception interface prototype object's \"name\" property");

    test(function()
    {
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));
        assert_own_property(window[this.name], "prototype",
                            'interface "' + this.name + '" does not have own property "prototype"');

        //"If the [NoInterfaceObject] extended attribute was not specified on
        //the exception, then there must also be a property named “constructor”
        //on the exception interface prototype object with attributes {
        //[[Writable]]: true, [[Enumerable]]: false, [[Configurable]]: true }
        //and whose value is a reference to the exception interface object for
        //the exception."
        assert_own_property(window[this.name].prototype, "constructor",
                            this.name + '.prototype does not have own property "constructor"');
        var desc = Object.getOwnPropertyDescriptor(window[this.name].prototype, "constructor");
        assert_false("get" in desc, this.name + ".prototype.constructor has getter");
        assert_false("set" in desc, this.name + ".prototype.constructor has setter");
        assert_true(desc.writable, this.name + ".prototype.constructor is not writable");
        assert_false(desc.enumerable, this.name + ".prototype.constructor is enumerable");
        assert_true(desc.configurable, this.name + ".prototype.constructor in not configurable");
        assert_equals(window[this.name].prototype.constructor, window[this.name],
                      this.name + '.prototype.constructor is not the same object as ' + this.name);
    }.bind(this), this.name + " exception: existence and properties of exception interface prototype object's \"constructor\" property");
}

//@}
IdlException.prototype.test_members = function()
//@{
{
    for (var i = 0; i < this.members.length; i++)
    {
        var member = this.members[i];
        if (member.untested)
        {
            continue;
        }
        if (member.type == "const" && member.name != "prototype")
        {
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));

                //"For each constant defined on the exception, there must be a
                //corresponding property on the exception interface object, if
                //it exists, if the identifier of the constant is not
                //“prototype”."
                assert_own_property(window[this.name], member.name);
                //"The value of the property is the ECMAScript value that is
                //equivalent to the constant’s IDL value, according to the
                //rules in section 4.2 above."
                assert_equals(window[this.name][member.name], eval(member.value),
                              "property has wrong value");
                //"The property has attributes { [[Writable]]: false,
                //[[Enumerable]]: true, [[Configurable]]: false }."
                var desc = Object.getOwnPropertyDescriptor(window[this.name], member.name);
                assert_false("get" in desc, "property has getter");
                assert_false("set" in desc, "property has setter");
                assert_false(desc.writable, "property is writable");
                assert_true(desc.enumerable, "property is not enumerable");
                assert_false(desc.configurable, "property is configurable");
            }.bind(this), this.name + " exception: constant " + member.name + " on exception interface object");
            //"In addition, a property with the same characteristics must
            //exist on the exception interface prototype object."
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));
                assert_own_property(window[this.name], "prototype",
                                    'exception "' + this.name + '" does not have own property "prototype"');

                assert_own_property(window[this.name].prototype, member.name);
                assert_equals(window[this.name].prototype[member.name], eval(member.value),
                              "property has wrong value");
                var desc = Object.getOwnPropertyDescriptor(window[this.name].prototype, member.name);
                assert_false("get" in desc, "property has getter");
                assert_false("set" in desc, "property has setter");
                assert_false(desc.writable, "property is writable");
                assert_true(desc.enumerable, "property is not enumerable");
                assert_false(desc.configurable, "property is configurable");
            }.bind(this), this.name + " exception: constant " + member.name + " on exception interface prototype object");
        }
        else if (member.type == "field")
        {
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));
                assert_own_property(window[this.name], "prototype",
                                    'exception "' + this.name + '" does not have own property "prototype"');

                //"For each exception field, there must be a corresponding
                //property on the exception interface prototype object, whose
                //characteristics are as follows:
                //"The name of the property is the identifier of the exception
                //field."
                assert_own_property(window[this.name].prototype, member.name);
                //"The property has attributes { [[Get]]: G, [[Enumerable]]:
                //true, [[Configurable]]: true }, where G is the exception
                //field getter, defined below."
                var desc = Object.getOwnPropertyDescriptor(window[this.name].prototype, member.name);
                assert_false("value" in desc, "property descriptor has value but is supposed to be accessor");
                assert_false("writable" in desc, 'property descriptor has "writable" field but is supposed to be accessor');
                //TODO: ES5 doesn't seem to say whether desc should have a .set
                //property.
                assert_true(desc.enumerable, "property is not enumerable");
                assert_true(desc.configurable, "property is not configurable");
                //"The exception field getter is a Function object whose
                //behavior when invoked is as follows:"
                assert_equals(typeof desc.get, "function", "typeof getter");
                //"The value of the Function object’s “length” property is the
                //Number value 0."
                //This test is before the TypeError tests so that it's easiest
                //to see that Firefox 11a1 only fails one assert in this test.
                assert_equals(desc.get.length, 0, "getter length");
                //"Let O be the result of calling ToObject on the this value.
                //"If O is not a platform object representing an exception for
                //the exception on which the exception field was declared, then
                //throw a TypeError."
                //TODO: Test on a platform object representing an exception.
                assert_throws(new TypeError(), function()
                {
                    window[this.name].prototype[member.name];
                }, "getting property on prototype object must throw TypeError");
                assert_throws(new TypeError(), function()
                {
                    desc.get.call({});
                }, "calling getter on wrong object type must throw TypeError");
            }.bind(this), this.name + " exception: field " + member.name + " on exception interface prototype object");
        }
    }
}

//@}
IdlException.prototype.test_object = function(desc)
//@{
{
    var obj, exception = null;
    try
    {
        obj = eval(desc);
    }
    catch(e)
    {
        exception = e;
    }

    test(function()
    {
        assert_equals(exception, null, "Unexpected exception when evaluating object");
        assert_equals(typeof obj, "object", "wrong typeof object");

        //We can't easily test that its prototype is correct if there's no
        //interface object, or the object is from a different global environment
        //(not instanceof Object).  TODO: test in this case that its prototype at
        //least looks correct, even if we can't test that it's actually correct.
        if (!this.has_extended_attribute("NoInterfaceObject")
        && (typeof obj != "object" || obj instanceof Object))
        {
            assert_own_property(window, this.name,
                                "window does not have own property " + format_value(this.name));
            assert_own_property(window[this.name], "prototype",
                                'exception "' + this.name + '" does not have own property "prototype"');

            //"The value of the internal [[Prototype]] property of the
            //exception object must be the exception interface prototype object
            //from the global environment the exception object is associated
            //with."
            assert_true(window[this.name].prototype.isPrototypeOf(obj),
                desc + "'s prototype is not " + this.name + ".prototype");
        }

        //"The class string of the exception object must be the identifier of
        //the exception."
        assert_equals({}.toString.call(obj), "[object " + this.name + "]", "{}.toString.call(" + desc + ")");
        //Stringifier is not defined for DOMExceptions, because message isn't
        //defined.
    }.bind(this), this.name + " must be represented by " + desc);

    for (var i = 0; i < this.members.length; i++)
    {
        var member = this.members[i];
        test(function()
        {
            assert_equals(exception, null, "Unexpected exception when evaluating object");
            assert_equals(typeof obj, "object", "wrong typeof object");
            assert_inherits(obj, member.name);
            if (member.type == "const")
            {
                assert_equals(obj[member.name], eval(member.value));
            }
            if (member.type == "field")
            {
                this.array.assert_type_is(obj[member.name], member.idlType);
            }
        }.bind(this), this.name + " exception: " + desc + ' must inherit property "' + member.name + '" with the proper type');
    }
}
//@}

/// IdlInterface ///
function IdlInterface(obj)
//@{
{
    this.name = obj.name;
    this.array = obj.array;
    this.untested = obj.untested;
    this.extAttrs = obj.extAttrs ? obj.extAttrs : [];
    this.members = obj.members ? obj.members.map(function(m){return new IdlInterfaceMember(m)}) : [];
    this.inheritance = obj.inheritance ? obj.inheritance : [];
    interfaces[this.name] = this;
}

//@}
IdlInterface.prototype = Object.create(IdlObject.prototype);
IdlInterface.prototype.test = function()
//@{
{
    if (this.has_extended_attribute("NoInterfaceObject"))
    {
        //No tests to do without an instance
        return;
    }

    if (!this.untested)
    {
        this.test_self();
    }
    this.test_members();
}

//@}
IdlInterface.prototype.test_self = function()
//@{
{
    test(function()
    {
        //"For every interface that is not declared with the
        //[NoInterfaceObject] extended attribute, a corresponding property
        //must exist on the interface’s relevant namespace object. The name
        //of the property is the identifier of the interface, and its value
        //is an object called the interface object. The property has the
        //attributes { [[Writable]]: true, [[Enumerable]]: false,
        //[[Configurable]]: true }."
        //TODO: Should we test here that the property is actually writable
        //etc., or trust getOwnPropertyDescriptor?
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));
        var desc = Object.getOwnPropertyDescriptor(window, this.name);
        assert_false("get" in desc, "window's property " + format_value(this.name) + " has getter");
        assert_false("set" in desc, "window's property " + format_value(this.name) + " has setter");
        assert_true(desc.writable, "window's property " + format_value(this.name) + " is not writable");
        assert_false(desc.enumerable, "window's property " + format_value(this.name) + " is enumerable");
        assert_true(desc.configurable, "window's property " + format_value(this.name) + " is not configurable");

        //"Interface objects are always function objects."
        //"If an object is defined to be a function object, then it has
        //characteristics as follows:"
        //"Its [[Prototype]] internal property is the Function prototype
        //object."
        //Note: This doesn't match browsers as of December 2011, see
        //http://www.w3.org/Bugs/Public/show_bug.cgi?id=14813
        assert_true(Function.prototype.isPrototypeOf(window[this.name]),
                    "prototype of window's property " + format_value(this.name) + " is not Function.prototype");
        //"Its [[Get]] internal property is set as described in ECMA-262
        //section 15.3.5.4."
        //Not much to test for this.
        //"Its [[Construct]] internal property is set as described in ECMA-262
        //section 13.2.2."
        //Tested below if no constructor is defined.  TODO: test constructors
        //if defined.
        //"Its [[HasInstance]] internal property is set as described in
        //ECMA-262 section 15.3.5.3, unless otherwise specified."
        //TODO
        //"Its [[Class]] internal property is “Function”."
        //String() returns something implementation-dependent, because it calls
        //Function#toString.
        assert_equals({}.toString.call(window[this.name]), "[object Function]",
                      "{}.toString.call(" + this.name + ")");

        if (!this.has_extended_attribute("Constructor"))
        {
            //"The internal [[Call]] method of the interface object behaves as
            //follows . . .
            //
            //"If I was not declared with a [Constructor] extended attribute,
            //then throw a TypeError."
            assert_throws(new TypeError(), function()
            {
                window[this.name]();
            }, "interface object didn't throw TypeError when called as a function");
            assert_throws(new TypeError(), function()
            {
                new window[this.name]();
            }, "interface object didn't throw TypeError when called as a constructor");
        }
    }.bind(this), this.name + " interface: existence and properties of interface object");

    if (this.has_extended_attribute("Constructor"))
    {
        test(function()
        {
            assert_own_property(window, this.name,
                                "window does not have own property " + format_value(this.name));

            //"Interface objects for interfaces declared with a [Constructor]
            //extended attribute must have a property named “length” with
            //attributes { [[Writable]]: false, [[Enumerable]]: false,
            //[[Configurable]]: false } whose value is a Number determined as
            //follows: . . .
            //"Return the maximum argument list length of the constructors in
            //the entries of S."
            //TODO: Variadic constructors.  Should generalize this so that it
            //works for testing operation length too (currently we just don't
            //support multiple operations with the same identifier).
            var expected_length = this.extAttrs
                .filter(function(attr) { return attr.name == "Constructor" })
                .map(function(attr) { return attr.arguments ? attr.arguments.length : 0 })
                .reduce(function(m, n) { return Math.max(m, n) });
            assert_own_property(window[this.name], "length");
            assert_equals(window[this.name].length, expected_length, "wrong value for " + this.name + ".length");
            var desc = Object.getOwnPropertyDescriptor(window[this.name], "length");
            assert_false("get" in desc, this.name + ".length has getter");
            assert_false("set" in desc, this.name + ".length has setter");
            assert_false(desc.writable, this.name + ".length is writable");
            assert_false(desc.enumerable, this.name + ".length is enumerable");
            assert_false(desc.configurable, this.name + ".length is configurable");
        }.bind(this), this.name + " interface constructor");
    }

    //TODO: Test named constructors if I find any interfaces that have them.

    test(function()
    {
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));

        //"The interface object must also have a property named “prototype”
        //with attributes { [[Writable]]: false, [[Enumerable]]: false,
        //[[Configurable]]: false } whose value is an object called the
        //interface prototype object. This object has properties that
        //correspond to the attributes and operations defined on the interface,
        //and is described in more detail in section 4.5.3 below."
        assert_own_property(window[this.name], "prototype",
                            'interface "' + this.name + '" does not have own property "prototype"');
        var desc = Object.getOwnPropertyDescriptor(window[this.name], "prototype");
        assert_false("get" in desc, this.name + ".prototype has getter");
        assert_false("set" in desc, this.name + ".prototype has setter");
        assert_false(desc.writable, this.name + ".prototype is writable");
        assert_false(desc.enumerable, this.name + ".prototype is enumerable");
        assert_false(desc.configurable, this.name + ".prototype is configurable");

        //"The interface prototype object for a given interface A must have
        //an internal [[Prototype]] property whose value is as follows:
        //"If A is not declared to inherit from another interface, then the
        //value of the internal [[Prototype]] property of A is the Array
        //prototype object ([ECMA-262], section 15.4.4) if the interface
        //was declared with ArrayClass, or the Object prototype object
        //otherwise ([ECMA-262], section 15.2.4).
        //"Otherwise, A does inherit from another interface. The value of
        //the internal [[Prototype]] property of A is the interface
        //prototype object for the inherited interface."
        var inherit_interface = (function()
        {
            for (var i = 0; i < this.inheritance.length; ++i)
            {
                if (!interfaces[this.inheritance[i]].has_extended_attribute("NoInterfaceObject"))
                {
                    return this.inheritance[i];
                }
            }
            if (this.has_extended_attribute("ArrayClass"))
            {
                return "Array";
            }
            return "Object";
        }).bind(this)();
        assert_own_property(window, inherit_interface,
                            'should inherit from ' + inherit_interface + ', but window has no such property');
        assert_own_property(window[inherit_interface], "prototype",
                            'should inherit from ' + inherit_interface + ', but that object has no "prototype" property');
        assert_true(window[inherit_interface].prototype.isPrototypeOf(window[this.name].prototype),
                    'prototype of ' + this.name + '.prototype is not ' + inherit_interface + '.prototype');

        //"The class string of an interface prototype object is the
        //concatenation of the interface’s identifier and the string
        //“Prototype”."
        //String() and {}.toString.call() should be equivalent, since nothing
        //defines a stringifier.
        assert_equals({}.toString.call(window[this.name].prototype), "[object " + this.name + "Prototype]",
                      "{}.toString.call(" + this.name + ".prototype)");
        assert_equals(String(window[this.name].prototype), "[object " + this.name + "Prototype]",
                      "String(" + this.name + ".prototype)");
    }.bind(this), this.name + " interface: existence and properties of interface prototype object");

    test(function()
    {
        assert_own_property(window, this.name,
                            "window does not have own property " + format_value(this.name));
        assert_own_property(window[this.name], "prototype",
                            'interface "' + this.name + '" does not have own property "prototype"');

        //"If the [NoInterfaceObject] extended attribute was not specified on
        //the interface, then the interface prototype object must also have a
        //property named “constructor” with attributes { [[Writable]]: true,
        //[[Enumerable]]: false, [[Configurable]]: true } whose value is a
        //reference to the interface object for the interface."
        assert_own_property(window[this.name].prototype, "constructor",
                            this.name + '.prototype does not have own property "constructor"');
        var desc = Object.getOwnPropertyDescriptor(window[this.name].prototype, "constructor");
        assert_false("get" in desc, this.name + ".prototype.constructor has getter");
        assert_false("set" in desc, this.name + ".prototype.constructor has setter");
        assert_true(desc.writable, this.name + ".prototype.constructor is not writable");
        assert_false(desc.enumerable, this.name + ".prototype.constructor is enumerable");
        assert_true(desc.configurable, this.name + ".prototype.constructor in not configurable");
        assert_equals(window[this.name].prototype.constructor, window[this.name],
                      this.name + '.prototype.constructor is not the same object as ' + this.name);
    }.bind(this), this.name + ' interface: existence and properties of interface prototype object\'s "constructor" property');
}

//@}
IdlInterface.prototype.test_members = function()
//@{
{
    for (var i = 0; i < this.members.length; i++)
    {
        var member = this.members[i];
        if (member.untested)
        {
            continue;
        }
        if (member.type == "const")
        {
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));

                //"For each constant defined on an interface A, there must
                //be a corresponding property on the interface object, if
                //it exists."
                assert_own_property(window[this.name], member.name);
                //"The value of the property is that which is obtained by
                //converting the constant’s IDL value to an ECMAScript
                //value."
                assert_equals(window[this.name][member.name], eval(member.value),
                              "property has wrong value");
                //"The property has attributes { [[Writable]]: false,
                //[[Enumerable]]: true, [[Configurable]]: false }."
                var desc = Object.getOwnPropertyDescriptor(window[this.name], member.name);
                assert_false("get" in desc, "property has getter");
                assert_false("set" in desc, "property has setter");
                assert_false(desc.writable, "property is writable");
                assert_true(desc.enumerable, "property is not enumerable");
                assert_false(desc.configurable, "property is configurable");
            }.bind(this), this.name + " interface: constant " + member.name + " on interface object");
            //"In addition, a property with the same characteristics must
            //exist on the interface prototype object."
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));
                assert_own_property(window[this.name], "prototype",
                                    'interface "' + this.name + '" does not have own property "prototype"');

                assert_own_property(window[this.name].prototype, member.name);
                assert_equals(window[this.name].prototype[member.name], eval(member.value),
                              "property has wrong value");
                var desc = Object.getOwnPropertyDescriptor(window[this.name], member.name);
                assert_false("get" in desc, "property has getter");
                assert_false("set" in desc, "property has setter");
                assert_false(desc.writable, "property is writable");
                assert_true(desc.enumerable, "property is not enumerable");
                assert_false(desc.configurable, "property is configurable");
            }.bind(this), this.name + " interface: constant " + member.name + " on interface prototype object");
        }
        else if (member.type == "attribute")
        {
            if (member.has_extended_attribute("Unforgeable"))
            {
                //We do the checks in test_interface_of instead
                continue;
            }
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));
                assert_own_property(window[this.name], "prototype",
                                    'interface "' + this.name + '" does not have own property "prototype"');

                do_interface_attribute_asserts(window[this.name].prototype, member);
            }.bind(this), this.name + " interface: attribute " + member.name);
        }
        else if (member.type == "operation")
        {
            //TODO: Need to correctly handle multiple operations with the
            //same identifier.
            if (!member.name)
            {
                //Unnamed getter or such
                continue;
            }
            test(function()
            {
                assert_own_property(window, this.name,
                                    "window does not have own property " + format_value(this.name));
                assert_own_property(window[this.name], "prototype",
                                    'interface "' + this.name + '" does not have own property "prototype"');

                //"For each unique identifier of an operation defined on
                //the interface, there must be a corresponding property on
                //the interface prototype object (if it is a regular
                //operation) or the interface object (if it is a static
                //operation), unless the effective overload set for that
                //identifier and operation and with an argument count of 0
                //(for the ECMAScript language binding) has no entries."
                //TODO: The library doesn't seem to support static
                //operations.
                assert_own_property(window[this.name].prototype, member.name,
                    "interface prototype object missing non-static operation");

                var desc = Object.getOwnPropertyDescriptor(window[this.name].prototype, member.name);
                //"The property has attributes { [[Writable]]: true,
                //[[Enumerable]]: true, [[Configurable]]: true }."
                assert_false("get" in desc, "property has getter");
                assert_false("set" in desc, "property has setter");
                assert_true(desc.writable, "property is not writable");
                assert_true(desc.enumerable, "property is not enumerable");
                assert_true(desc.configurable, "property is not configurable");
                //"The value of the property is a Function object whose
                //behavior is as follows . . ."
                assert_equals(typeof window[this.name].prototype[member.name], "function",
                              "property must be a function");
                //"The value of the Function object’s “length” property is
                //a Number determined as follows:
                //". . .
                //"Return the maximum argument list length of the functions
                //in the entries of S."
                //TODO: Does this work for overloads?
                assert_equals(window[this.name].prototype[member.name].length, member.arguments.length,
                    "property has wrong .length");
            }.bind(this), this.name + " interface: operation " + member.name +
            "(" + member.arguments.map(function(m) { return m.type.idlType; }) +
            ")");
        }
        //TODO: check more member types, like stringifier
    }
}

//@}
IdlInterface.prototype.test_object = function(desc)
//@{
{
    var obj, exception = null;
    try
    {
        obj = eval(desc);
    }
    catch(e)
    {
        exception = e;
    }

    //TODO: WebIDLParser doesn't currently support named legacycallers, so I'm
    //not sure what those would look like in the AST
    var expected_typeof = this.members.some(function(member)
    {
        return member.legacycaller
            || ("idlType" in member && member.idlType.legacycaller)
            || ("idlType" in member && typeof member.idlType == "object"
            && "idlType" in member.idlType && member.idlType.idlType == "legacycaller");
    }) ? "function" : "object";

    this.test_primary_interface_of(desc, obj, exception, expected_typeof);
    var current_interface = this;
    while (current_interface)
    {
        if (!(current_interface.name in this.array.members))
        {
            throw "Interface " + current_interface.name + " not found (inherited by " + this.name + ")";
        }
        if (current_interface.prevent_multiple_testing && current_interface.already_tested)
        {
            return;
        }
        current_interface.test_interface_of(desc, obj, exception, expected_typeof);
        current_interface = this.array.members[current_interface.inheritance[0]];
    }
}

//@}
IdlInterface.prototype.test_primary_interface_of = function(desc, obj, exception, expected_typeof)
//@{
{
    //We can't easily test that its prototype is correct if there's no
    //interface object, or the object is from a different global environment
    //(not instanceof Object).  TODO: test in this case that its prototype at
    //least looks correct, even if we can't test that it's actually correct.
    if (!this.has_extended_attribute("NoInterfaceObject")
    && (typeof obj != expected_typeof || obj instanceof Object))
    {
        test(function()
        {
            assert_equals(exception, null, "Unexpected exception when evaluating object");
            assert_equals(typeof obj, expected_typeof, "wrong typeof object");
            assert_own_property(window, this.name,
                                "window does not have own property " + format_value(this.name));
            assert_own_property(window[this.name], "prototype",
                                'interface "' + this.name + '" does not have own property "prototype"');

            //"The value of the internal [[Prototype]] property of the platform
            //object is the interface prototype object of the primary interface
            //from the platform object’s associated global environment."
            assert_true(window[this.name].prototype.isPrototypeOf(obj),
                desc + "'s prototype is not " + this.name + ".prototype");
        }.bind(this), this.name + " must be primary interface of " + desc);
    }

    //"The class string of a platform object that implements one or more
    //interfaces must be the identifier of the primary interface of the
    //platform object."
    test(function()
    {
        assert_equals(exception, null, "Unexpected exception when evaluating object");
        assert_equals(typeof obj, expected_typeof, "wrong typeof object");
        assert_equals({}.toString.call(obj), "[object " + this.name + "]", "{}.toString.call(" + desc + ")");
        if (!this.members.some(function(member) { return member.stringifier || member.type == "stringifier"}))
        {
            assert_equals(String(obj), "[object " + this.name + "]", "String(" + desc + ")");
        }
    }.bind(this), "Stringification of " + desc);
}

//@}
IdlInterface.prototype.test_interface_of = function(desc, obj, exception, expected_typeof)
//@{
{
    //TODO: Indexed and named properties, more checks on interface members
    this.already_tested = true;

    for (var i = 0; i < this.members.length; i++)
    {
        var member = this.members[i];
        if (member.has_extended_attribute("Unforgeable"))
        {
            test(function()
            {
                assert_equals(exception, null, "Unexpected exception when evaluating object");
                assert_equals(typeof obj, expected_typeof, "wrong typeof object");
                do_interface_attribute_asserts(obj, member);
            }.bind(this), this.name + " interface: " + desc + ' must have own property "' + member.name + '"');
        }
        else if ((member.type == "const"
        || member.type == "attribute"
        || member.type == "operation")
        && member.name)
        {
            test(function()
            {
                assert_equals(exception, null, "Unexpected exception when evaluating object");
                assert_equals(typeof obj, expected_typeof, "wrong typeof object");
                assert_inherits(obj, member.name);
                if (member.type == "const")
                {
                    assert_equals(obj[member.name], eval(member.value));
                }
                if (member.type == "attribute")
                {
                    // Attributes are accessor properties, so they might
                    // legitimately throw an exception rather than returning
                    // anything.
                    var property, thrown = false;
                    try
                    {
                        property = obj[member.name];
                    }
                    catch (e)
                    {
                        thrown = true;
                    }
                    if (!thrown)
                    {
                        this.array.assert_type_is(property, member.idlType);
                    }
                }
                if (member.type == "operation")
                {
                    assert_equals(typeof obj[member.name], "function");
                }
            }.bind(this), this.name + " interface: " + desc + ' must inherit property "' + member.name + '" with the proper type (' + i + ')');
        }
        //TODO: This is wrong if there are multiple operations with the same
        //identifier.
        //TODO: Test passing arguments of the wrong type.
        if (member.type == "operation" && member.name && member.arguments.length)
        {
            test(function()
            {
                assert_equals(exception, null, "Unexpected exception when evaluating object");
                assert_equals(typeof obj, expected_typeof, "wrong typeof object");
                assert_inherits(obj, member.name);
                var args = [];
                for (var i = 0; i < member.arguments.length; i++)
                {
                    if (member.arguments[i].optional)
                    {
                        break;
                    }
                    assert_throws(new TypeError(), function()
                    {
                        obj[member.name].apply(obj, args);
                    }, "Called with " + i + " arguments");

                    args.push(create_suitable_object(member.arguments[i].type));
                }
            }.bind(this), this.name + " interface: calling " + member.name +
            "(" + member.arguments.map(function(m) { return m.type.idlType; }) +
            ") on " + desc + " with too few arguments must throw TypeError");
        }
    }
}

//@}
function do_interface_attribute_asserts(obj, member)
//@{
{
    //"For each attribute defined on the interface, there must exist a
    //corresponding property. If the attribute was declared with the
    //[Unforgeable] extended attribute, then the property exists on every
    //object that implements the interface.  Otherwise, it exists on the
    //interface’s interface prototype object."
    //This is called by test_self() with the prototype as obj, and by
    //test_interface_of() with the object as obj.
    assert_own_property(obj, member.name);

    //"The property has attributes { [[Get]]: G, [[Set]]: S,
    //[[Enumerable]]: true, [[Configurable]]: configurable },
    //where:
    //"configurable is false if the attribute was declared with
    //the [Unforgeable] extended attribute and true otherwise;
    //"G is the attribute getter, defined below; and
    //"S is the attribute setter, also defined below."
    var desc = Object.getOwnPropertyDescriptor(obj, member.name);
    assert_false("value" in desc, 'property descriptor has value but is supposed to be accessor');
    assert_false("writable" in desc, 'property descriptor has "writable" field but is supposed to be accessor');
    assert_true(desc.enumerable, "property is not enumerable");
    if (member.has_extended_attribute("Unforgeable"))
    {
        assert_false(desc.configurable, "[Unforgeable] property must not be configurable");
    }
    else
    {
        assert_true(desc.configurable, "property must be configurable");
    }

    //"The attribute getter is a Function object whose behavior
    //when invoked is as follows:
    //"...
    //"The value of the Function object’s “length” property is
    //the Number value 0."
    assert_equals(typeof desc.get, "function", "getter must be Function");
    assert_equals(desc.get.length, 0, "getter length must be 0");

    //TODO: Test calling setter on the interface prototype (should throw
    //TypeError in most cases).
    //
    //"The attribute setter is undefined if the attribute is
    //declared readonly and has neither a [PutForwards] nor a
    //[Replaceable] extended attribute declared on it.
    //Otherwise, it is a Function object whose behavior when
    //invoked is as follows:
    //"...
    //"The value of the Function object’s “length” property is
    //the Number value 1."
    if (member.readonly
    && !member.has_extended_attribute("PutForwards")
    && !member.has_extended_attribute("Replaceable"))
    {
        assert_equals(desc.set, undefined, "setter must be undefined for readonly attributes");
    }
    else
    {
        assert_equals(typeof desc.set, "function", "setter must be function for PutForwards, Replaceable, or non-readonly attributes");
        assert_equals(desc.set.length, 1, "setter length must be 1");
    }
}
//@}

/// IdlInterfaceMember ///
function IdlInterfaceMember(obj)
//@{
{
    for (var k in obj)
    {
        if (k == "extAttrs")
        {
            this.extAttrs = obj.extAttrs ? obj.extAttrs : [];
        }
        else
        {
            this[k] = obj[k];
        }
    }
    if (!("extAttrs" in this))
    {
        this.extAttrs = [];
    }
}

//@}
IdlInterfaceMember.prototype = Object.create(IdlObject.prototype);

/// Internal helper functions ///
function create_suitable_object(type)
//@{
{
    if (type.nullable)
    {
        return null;
    }
    switch (type.idlType)
    {
        case "any":
        case "boolean":
            return true;

        case "byte": case "octet": case "short": case "unsigned short":
        case "long": case "unsigned long": case "long long":
        case "unsigned long long": case "float": case "double":
            return 7;

        case "DOMString":
            return "foo";

        case "object":
            return {a: "b"};

        case "Node":
            return document.createTextNode("abc");
    }
    return null;
}
//@}
})();
// vim: set expandtab shiftwidth=4 tabstop=4 foldmarker=@{,@} foldmethod=marker:
