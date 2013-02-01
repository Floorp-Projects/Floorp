# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import unittest
from cuddlefish.docs.apiparser import parse_hunks, ParseError

tests_path = os.path.abspath(os.path.dirname(__file__))
static_files_path = os.path.join(tests_path, "static-files")

class ParserTests(unittest.TestCase):
    def pathname(self, filename):
        return os.path.join(static_files_path, "docs", filename)

    def parse_text(self, text):
        return list(parse_hunks(text))

    def parse(self, pathname):
        return self.parse_text(open(pathname).read())

    def test_parser(self):
        parsed = self.parse(self.pathname("APIsample.md"))
        #for i,h in enumerate(parsed):
        #    print i, h
        self.assertEqual(parsed[0],
                         ("version", 4))
        self.assertEqual(parsed[1],
                         ("markdown", """\
<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Title #

Some text here

"""))

        self.assertEqual(parsed[2][0], "api-json")
        p_test = parsed[2][1]
        self.assertEqual(p_test["name"], "test")
        self.assertEqual(p_test["type"], "function")
        self.assertEqual(p_test["signature"], "test(argOne, argTwo, \
argThree, options)")
        self.assertEqual(p_test["description"],
                         "This is a function which does nothing in \
particular.")
        r = p_test["returns"]
        self.assertEqual(r["datatype"], "object")
        self.assertEqual(r["description"], "")
        self.assertEqual(len(r["props"]), 2)
        self.assertEqual(r["props"][0]["datatype"], "string")
        self.assertEqual(r["props"][0]["description"], "First string")
        self.assertEqual(r["props"][1]["datatype"], "url")
        self.assertEqual(r["props"][1]["description"], "First URL")

        self.assertEqual(p_test["params"][0],
                         {"name": "argOne",
                          "required": True,
                          "datatype": "string",
                          "description": "This is the first argument.",
                          "line_number": 15,
                          })

        self.assertEqual(p_test["params"][1],
                         {"name": "argTwo",
                          "required": False,
                          "datatype": "bool",
                          "description": "This is the second argument.",
                          "line_number": 16,
                          })

        self.assertEqual(p_test["params"][2],
                         {"name": "argThree",
                          "required": False,
                          "default": "default",
                          "datatype": "uri",
                          "line_number": 17,
                          "description": """\
This is the third and final argument. And this is
a test of the ability to do multiple lines of
text.""",
                          })
        p3 = p_test["params"][3]
        self.assertEqual(p3["name"], "options")
        self.assertEqual(p3["required"], False)
        self.failIf("type" in p3)
        self.assertEqual(p3["description"], "Options Bag")
        self.assertEqual(p3["props"][0],
                         {"name": "style",
                          "required": False,
                          "datatype": "string",
                          "description": "Some style information.",
                          "line_number": 22,
                          })
        self.assertEqual(p3["props"][1],
                         {"name": "secondToLastOption",
                          "required": False,
                          "default": "True",
                          "datatype": "bool",
                          "description": "The last property.",
                          "line_number": 23,
                          })
        self.assertEqual(p3["props"][2]["name"], "lastOption")
        self.assertEqual(p3["props"][2]["required"], False)
        self.assertEqual(p3["props"][2]["datatype"], "uri")
        self.assertEqual(p3["props"][2]["description"], """\
And this time we have
A multiline description
Written as haiku""")

        self.assertEqual(parsed[3][0], "markdown")
        self.assertEqual(parsed[3][1], "\n\nThis text appears between the \
API blocks.\n\n")

        self.assertEqual(parsed[4][0], "api-json")
        p_test = parsed[4][1]

        expected = {'line_number': 32,
 'name': 'append',
 'params': [{'props':[{'line_number': 37,
                       'required': False,
                       'datatype': 'uri',
                       'name': 'icon',
                       'description': 'The HREF of an icon to show as the \
method of accessing your features slideBar'},
                      {'line_number': 38,
                       'required': False,
                       'datatype': 'string/xml',
                       'name': 'html',
                       'description': 'The content of the feature, either \
as an HTML string,\nor an E4X document fragment.'},
                      {'line_number': 41,
                       'required': False,
                       'datatype': 'uri',
                       'name': 'url',
                       'description': 'The url to load into the content area \
of the feature'},
                      {'line_number': 42,
                       'required': False,
                       'datatype': 'int',
                       'name': 'width',
                       'description': 'Width of the content area and the \
selected slide size'},
                      {'line_number': 43,
                       'required': False,
                       'datatype': 'bool',
                       'name': 'persist',
                       'description': 'Default slide behavior when being \
selected as follows:\nIf true: blah; If false: double blah.'},
                      {'line_number': 46,
                       'required': False,
                       'datatype': 'bool',
                       'name': 'autoReload',
                       'description': 'Automatically reload content on \
select'},
                      {'line_number': 47,
                       'required': False,
                       'datatype': 'function',
                       'name': 'onClick',
                       'description': 'Callback when the icon is \
clicked'},
                      {'line_number': 48,
                       'required': False,
                       'datatype': 'function',
                       'name': 'onSelect',
                       'description': 'Callback when the feature is selected'},
                      {'line_number': 49,
                       'required': False,
                       'datatype': 'function',
                       'name': 'onReady',
                       'description':
                       'Callback when featured is loaded'}],
                       'line_number': 35,
             'required': True,
             'name': 'options',
             'description': 'Pass in all of your options here.'}],
 'signature': 'append(options)',
 'type': 'function',
 'description': 'This is a list of options to specify modifications to your \
slideBar instance.'}
        self.assertEqual(p_test, expected)

        self.assertEqual(parsed[6][0], "api-json")
        p_test = parsed[6][1]
        self.assertEqual(p_test["name"], "cool-func.dot")
        self.assertEqual(p_test["signature"], "cool-func.dot(howMuch, double, \
options, onemore, options2)")
        self.assertEqual(p_test["returns"]["description"],
                         """\
A value telling you just how cool you are.
A boa-constructor!
This description can go on for a while, and can even contain
some **realy** fancy things. Like `code`, or even
~~~~{.javascript}
// Some code!
~~~~""")
        self.assertEqual(p_test["params"][2]["props"][0],
                         {"name": "callback",
                          "required": True,
                          "datatype": "function",
                          "line_number": 67,
                          "description": "The callback",
                          })
        self.assertEqual(p_test["params"][2]["props"][1],
                         {"name": "random",
                          "required": False,
                          "datatype": "bool",
                          "line_number": 68,
                          "description": "Do something random?",
                          })

        p_test = parsed[8][1]
        self.assertEqual(p_test["signature"],"random()")

        # tests for classes
        #1) empty class
        p_test = parsed[10][1]
        self.assertEqual(len(p_test), 4)
        self.assertEqual(p_test["name"], "empty-class")
        self.assertEqual(p_test["description"], "This class contains nothing.")
        self.assertEqual(p_test["type"], "class")
        # 2) class with just one ctor
        p_test = parsed[12][1]
        self.assertEqual(len(p_test), 5)
        self.assertEqual(p_test["name"], "only-one-ctor")
        self.assertEqual(p_test["description"], "This class contains only \
one constructor.")
        self.assertEqual(p_test["type"], "class")
        constructors = p_test["constructors"]
        self.assertEqual(len(constructors), 1)
        self._test_class_constructor(constructors[0], "one-constructor")
        # 3) class with 2 ctors
        p_test = parsed[14][1]
        self.assertEqual(len(p_test), 5)
        self.assertEqual(p_test["name"], "two-ctors")
        self.assertEqual(p_test["description"], "This class contains two \
constructors.")
        self.assertEqual(p_test["type"], "class")
        constructors = p_test["constructors"]
        self.assertEqual(len(constructors), 2)
        self._test_class_constructor(constructors[0], "one-constructor")
        self._test_class_constructor(constructors[1], "another-constructor")
        # 4) class with ctor + method
        p_test = parsed[16][1]
        self.assertEqual(len(p_test), 6)
        self.assertEqual(p_test["name"], "ctor-and-method")
        self.assertEqual(p_test["description"], "This class contains one \
constructor and one method.")
        self.assertEqual(p_test["type"], "class")
        constructors = p_test["constructors"]
        self.assertEqual(len(constructors), 1)
        self._test_class_constructor(constructors[0], "one-constructor")
        methods = p_test["methods"]
        self.assertEqual(len(methods), 1)
        self._test_class_method(methods[0])
        # 5) class with ctor + method + property
        p_test = parsed[18][1]
        self.assertEqual(len(p_test), 8)
        self.assertEqual(p_test["name"], "ctor-method-prop-event")
        self.assertEqual(p_test["description"], "This class contains one \
constructor, one method, one property and an event.")
        self.assertEqual(p_test["type"], "class")
        constructors = p_test["constructors"]
        self.assertEqual(len(constructors), 1)
        self._test_class_constructor(constructors[0], "one-constructor")
        methods = p_test["methods"]
        self.assertEqual(len(methods), 1)
        self._test_class_method(methods[0])
        properties = p_test["properties"]
        self.assertEqual(len(properties), 1)
        self._test_class_property(properties[0])
        events = p_test["events"]
        self.assertEqual(len(events), 1)
        self._test_class_event(events[0])

        self.assertEqual(parsed[-1][0], "markdown")
        self.assertEqual(parsed[-1][1], "\n\nSome more text here, \
at the end of the file.\n\n")

    def _test_class_constructor(self, constructor, name):
        self.assertEqual(constructor["type"], "constructor")
        self.assertEqual(constructor["name"], name)
        params = constructor["params"]
        self.assertEqual(len(params), 1)
        self.assertEqual(params[0]["name"], "options")
        self.assertEqual(params[0]["description"], "An object-bag of goodies.")

    def _test_class_method(self, method):
        self.assertEqual(method["type"], "method")
        self.assertEqual(method["name"], "a-method")
        self.assertEqual(method["description"], "Does things.")
        params = method["params"]
        self.assertEqual(len(params), 1)
        self.assertEqual(params[0]["name"], "options")
        self.assertEqual(params[0]["description"], "An argument.")

    def _test_class_property(self, prop):
        self.assertEqual(prop["type"], "property")
        self.assertEqual(prop["name"], "a-property")
        self.assertEqual(prop["description"], "Represents stuff.")
        self.assertEqual(prop["datatype"], "bool")

    def _test_class_event(self, event):
        self.assertEqual(event["type"], "event")
        self.assertEqual(event["name"], "message")
        self.assertEqual(event["description"], "Event emitted when the \
content script sends a message to the add-on.")
        arguments = event["arguments"]
        self.assertEqual(len(arguments), 1)
        argument = arguments[0]
        self.assertEqual(argument["datatype"], "JSON")
        self.assertEqual(argument["description"], "The message itself as a \
JSON-serialized object.")

    def test_missing_return_propname(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns {object}
  @prop {string} First string, but the property name is missing
  @prop {url} First URL, same problem
@param argOne {string} This is the first argument.
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_missing_return_proptype(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns {object}
  @prop untyped It is an error to omit the type of a return property.
@param argOne {string} This is the first argument.
@param [argTwo=True] {bool} This is the second argument.
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_return_propnames(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns {object}
  @prop firststring {string} First string.
  @prop [firsturl] {url} First URL, not always provided.
@param argOne {string} This is the first argument.
@param [argTwo=True] {bool} This is the second argument.
</api>
'''
        parsed = self.parse_text(md)
        r = parsed[1][1]["returns"]
        self.assertEqual(r["props"][0]["name"], "firststring")
        self.assertEqual(r["props"][0],
                         {"name": "firststring",
                          "datatype": "string",
                          "description": "First string.",
                          "required": True,
                          "line_number": 5, # 1-indexed
                          })
        self.assertEqual(r["props"][1],
                         {"name": "firsturl",
                          "datatype": "url",
                          "description": "First URL, not always provided.",
                          "required": False,
                          "line_number": 6,
                          })

    def test_return_description_1(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns {object} A one-line description.
  @prop firststring {string} First string.
  @prop [firsturl] {url} First URL, not always provided.
@param argOne {string} This is the first argument.
@param [argTwo=True] {bool} This is the second argument.
</api>
'''
        parsed = self.parse_text(md)
        r = parsed[1][1]["returns"]
        self.assertEqual(r["description"], "A one-line description.")

    def test_return_description_2(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns {object} A six-line description
  which is consistently indented by two spaces
    except for this line
  and preserves the following empty line
  
  from which a two-space indentation will be removed.
  @prop firststring {string} First string.
  @prop [firsturl] {url} First URL, not always provided.
@param argOne {string} This is the first argument.
@param [argTwo=True] {bool} This is the second argument.
</api>
'''
        parsed = self.parse_text(md)
        r = parsed[1][1]["returns"]
        self.assertEqual(r["description"],
                         "A six-line description\n"
                         "which is consistently indented by two spaces\n"
                         "  except for this line\n"
                         "and preserves the following empty line\n"
                         "\n"
                         "from which a two-space indentation will be removed.")

    def test_return_description_3(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns A one-line untyped description.
@param argOne {string} This is the first argument.
@param [argTwo=True] {bool} This is the second argument.
</api>
'''
        parsed = self.parse_text(md)
        r = parsed[1][1]["returns"]
        self.assertEqual(r["description"], "A one-line untyped description.")

    # if the return value was supposed to be an array, the correct syntax
    # would not have any @prop tags:
    #  @returns {array}
    #   Array consists of two elements, a string and a url...

    def test_return_array(self):
        md = '''\
<api name="test">
@method
This is a function which returns an array.
@returns {array}
  Array consists of two elements, a string and a url.
@param argOne {string} This is the first argument.
@param [argTwo=True] {bool} This is the second argument.
</api>
'''
        parsed = self.parse_text(md)
        r = parsed[1][1]["returns"]
        self.assertEqual(r["description"],
                         "Array consists of two elements, a string and a url.")

    def test_bad_default_on_required_parameter(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@returns something
@param argOne=ILLEGAL {string} Mandatory parameters do not take defaults.
@param [argTwo=Chicago] {string} This is the second argument.
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_missing_apitype(self):
        md = '''\
<api name="test">
Sorry, you must have a @method or something before the description.
Putting it after the description is not good enough
@method
@returns something
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_missing_param_propname(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@param p1 {object} This is a parameter.
  @prop {string} Oops, props must have a name.
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_missing_param_proptype(self):
        md = '''\
<api name="test">
@method
This is a function which does nothing in particular.
@param p1 {object} This is a parameter.
  @prop name Oops, props must have a type.
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_property(self):
        md = '''\
<api name="test">
@property {foo}
An object property named test of type foo.
</api>
'''
        parsed = self.parse_text(md)
        self.assertEqual(parsed[1][0], 'api-json')
        actual_api_json_obj = parsed[1][1]
        expected_api_json_obj = {
            'line_number': 1,
            'datatype': 'foo',
            'type': 'property',
            'name': 'test',
            'description': "An object property named test of type foo."
            }
        self.assertEqual(actual_api_json_obj, expected_api_json_obj)

    def test_property_no_type(self):
        md = '''\
<api name="test">
@property
This property needs to specify a type!
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

    def test_missing_api_closing_tag(self):
        md = '''\
<api name="test">
@class
This is a class with a missing closing tag.
<api name="doStuff"
@method
This method does stuff.
</api>
'''
        self.assertRaises(ParseError, self.parse_text, md)

if __name__ == "__main__":
    unittest.main()
