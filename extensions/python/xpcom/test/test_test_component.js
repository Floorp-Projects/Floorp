/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Python XPCOM language bindings.
 *
 * The Initial Developer of the Original Code is
 * ActiveState Tool Corp.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <MarkH@ActiveState.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Javascript code calling the Python test interface. */

var extended_unicode_string = "The Euro Symbol is '\u20ac'";

function MakeTestInterface()
{
    var clazz = Components.classes["Python.TestComponent"];
    var iface = Components.interfaces.nsIPythonTestInterfaceDOMStrings;
    return new clazz(iface);
}    

var c = new MakeTestInterface();

if (c.boolean_value != 1)
	throw("boolean_value has wrong initial value");
c.boolean_value = false;
if (c.boolean_value != false)
	throw("boolean_value has wrong new value");

// Python's own test does thorough testing of all numeric types
// Wont bother from here!

if (c.char_value != 'a')
	throw("char_value has wrong initial value");
c.char_value = 'b';
if (c.char_value != 'b')
	throw("char_value has wrong new value");

if (c.wchar_value != 'b')
	throw("wchar_value has wrong initial value");
c.wchar_value = 'c';
if (c.wchar_value != 'c')
	throw("wchar_value has wrong new value");

if (c.string_value != 'cee')
	throw("string_value has wrong initial value");
c.string_value = 'dee';
if (c.string_value != 'dee')
	throw("string_value has wrong new value");

if (c.wstring_value != 'dee')
	throw("wstring_value has wrong initial value");
c.wstring_value = 'eee';
if (c.wstring_value != 'eee')
	throw("wstring_value has wrong new value");
c.wstring_value = extended_unicode_string;
if (c.wstring_value != extended_unicode_string)
	throw("wstring_value has wrong new value");

if (c.domstring_value != 'dom')
	throw("domstring_value has wrong initial value");
c.domstring_value = 'New value';
if (c.domstring_value != 'New value')
	throw("domstring_value has wrong new value");
c.domstring_value = extended_unicode_string;
if (c.domstring_value != extended_unicode_string)
	throw("domstring_value has wrong new value");

if (c.utf8string_value != 'utf8string')
	throw("utf8string_value has wrong initial value");
c.utf8string_value = 'New value';
if (c.utf8string_value != 'New value')
	throw("utf8string_value has wrong new value");
c.utf8string_value = extended_unicode_string;
if (c.utf8string_value != extended_unicode_string)
	throw("utf8string_value has wrong new value");

var v = new Object();
v.value = "Hello"
var l = new Object();
l.value = v.value.length;
c.DoubleString(l, v);
if ( v.value != "HelloHello")
	throw("Could not double the string!");

var v = new Object();
v.value = "Hello"
var l = new Object();
l.value = v.value.length;
c.DoubleWideString(l, v);
if ( v.value != "HelloHello")
	throw("Could not double the wide string!");

// Some basic array tests
var v = new Array()
v[0] = 1;
v[2] = 2;
v[3] = 3;
var v2 = new Array()
v2[0] = 4;
v2[2] = 5;
v2[3] = 6;
if (c.SumArrays(v.length, v, v2) != 21)
	throw("Could not sum an array of integers!");


print("javascript successfully tested the Python test component.");
