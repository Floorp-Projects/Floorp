/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

package {
	
// E4X definitions.  based on ECMA-357
	
// {DontEnum} length=1
public native function isXMLName(str=void 0):Boolean

public final dynamic class XML extends Object
{
	// { ReadOnly, DontDelete, DontEnum }
	public static const length = 1
	
	// { DontDelete, DontEnum }
	public native static function get ignoreComments():Boolean
	public native static function set ignoreComments(newIgnore:Boolean)
	
	// { DontDelete, DontEnum }
	public native static function get ignoreProcessingInstructions():Boolean
	public native static function set ignoreProcessingInstructions(newIgnore:Boolean)
	
	// { DontDelete, DontEnum }
	public native static function get ignoreWhitespace():Boolean
	public native static function set ignoreWhitespace(newIgnore:Boolean)

	// { DontDelete, DontEnum }
	public native static function get prettyPrinting():Boolean
	public native static function set prettyPrinting(newPretty:Boolean)

	// { DontDelete, DontEnum }
	public native static function get prettyIndent():int
	public native static function set prettyIndent(newIndent:int)

	AS3 static function settings ():Object
	{
		return {
			ignoreComments: XML.ignoreComments,
			ignoreProcessingInstructions: XML.ignoreProcessingInstructions,
			ignoreWhitespace: XML.ignoreWhitespace,
			prettyPrinting: XML.prettyPrinting,
			prettyIndent: XML.prettyIndent
		};
	}
	
	AS3 static function setSettings(o:Object=null):void
	{
		if (o == null) // undefined or null
		{
			XML.ignoreComments = true;
			XML.ignoreProcessingInstructions = true;
			XML.ignoreWhitespace = true;
			XML.prettyPrinting = true;
			XML.prettyIndent = 2;
			return;
		}

		if (("ignoreComments" in o) && (o.ignoreComments is Boolean))			
			XML.ignoreComments = o.ignoreComments;
		if (("ignoreProcessingInstructions" in o) && (o.ignoreProcessingInstructions is Boolean))
			XML.ignoreProcessingInstructions = o.ignoreProcessingInstructions;
		if (("ignoreWhitespace" in o) && (o.ignoreWhitespace is Boolean))
			XML.ignoreWhitespace = o.ignoreWhitespace;
		if (("prettyPrinting" in o) && (o.prettyPrinting is Boolean))
			XML.prettyPrinting = o.prettyPrinting;
		if (("prettyIndent" in o) && (o.prettyIndent is Number))		
			XML.prettyIndent = o.prettyIndent;
	}
	
	AS3 static function defaultSettings():Object
	{
		return {
			ignoreComments: true,
			ignoreProcessingInstructions: true,
			ignoreWhitespace: true,
			prettyPrinting: true,
			prettyIndent: 2
		};
	}
	
	// override (hide) functions from object
	// ISSUE why do we override valueOf?  it does the same thing as the one in Object

	AS3 native function toString ():String

	// override AS3 methods from Object
	override AS3 native function hasOwnProperty (P=void 0):Boolean
	override AS3 native function propertyIsEnumerable (P=void 0):Boolean

	// XML functions
	AS3 native function addNamespace (ns):XML;
	AS3 native function appendChild (child):XML;
	AS3 native function attribute (arg):XMLList;
	AS3 native function attributes():XMLList;
	AS3 native function child (propertyName):XMLList;
	AS3 native function childIndex():int;
	AS3 native function children ():XMLList;
	AS3 native function comments ():XMLList;
	AS3 native function contains (value):Boolean;
	AS3 native function copy ():XML;
	AS3 native function descendants (name="*"):XMLList; // name is optional
	AS3 native function elements (name="*"):XMLList; // name is optional
	AS3 native function hasComplexContent ():Boolean;
	AS3 native function hasSimpleContent ():Boolean;
	AS3 native function inScopeNamespaces ():Array;
	AS3 native function insertChildAfter (child1, child2):*; // undefined or XML
	AS3 native function insertChildBefore (child1, child2):*; // undefined or XML
	AS3 function length ():int { return 1; }
	AS3 native function localName ():Object; // null or String;
	AS3 native function name ():Object; // null or String;
	AS3 native function namespace (prefix=null):*; // prefix is optional
	AS3 native function namespaceDeclarations ():Array;
	AS3 native function nodeKind ():String;
	AS3 native function normalize ():XML;
	AS3 native function parent ():*; // undefined or String
	AS3 native function processingInstructions (name="*"):XMLList; // name is optional
	AS3 native function prependChild (value):XML;
	AS3 native function removeNamespace (ns):XML;
	AS3 native function replace (propertyName, value):XML;
	AS3 native function setChildren (value):XML;
	AS3 native function setLocalName (name):void;
	AS3 native function setName (name):void;
	AS3 native function setNamespace (ns):void;
	AS3 native function text ():XMLList;
	AS3 native function toXMLString ():String;

	// notification extensions
	AS3 native function notification():Function;
	AS3 native function setNotification(f:Function);

    // Dummy constructor function - This is neccessary so the compiler can do arg # checking for the ctor in strict mode
    // The code for the actual ctor is in XMLClass::construct in the avmplus
    public function XML(value = void 0)
    {}
	//
	// dynamic, proto-hackable properties from E-357
	//
	
   	XML.settings = function():Object {
		return AS3::settings()
	}
	XML.setSettings = function(o=undefined):void {
		AS3::setSettings(o)
	}
	XML.defaultSettings = function():Object {
		return AS3::defaultSettings()
	}

	AS3 function valueOf():XML { return this }

	// this is what rhino appears to do
	prototype.valueOf = Object.prototype.valueOf
	
	prototype.hasOwnProperty = function(P=void 0):Boolean {
		if (this === prototype) 
		{
			return this.AS3::hasOwnProperty(P);
		}
		var x:XML = this
		return x.AS3::hasOwnProperty(P)
	}
	
	prototype.propertyIsEnumerable = function(P=void 0):Boolean {
		if (this === prototype) 
		{
			return this.AS3::propertyIsEnumerable(P);
		}
		var x:XML = this
		return x.AS3::propertyIsEnumerable(P)
	}
	
	prototype.toString = function():String {
		if (this === prototype) 
		{
			return "";
		}
		var x:XML = this
		return x.AS3::toString()
	}
	
	prototype.addNamespace = function(ns):XML {
		var x:XML = this
		return x.AS3::addNamespace(ns)
	}
	
	prototype.appendChild = function(child):XML {
		var x:XML = this
		return x.AS3::appendChild(child)
	}
	
	prototype.attribute = function(arg):XMLList {
		var x:XML = this
		return x.AS3::attribute(arg)
	}

	prototype.attributes = function():XMLList {
		var x:XML = this
		return x.AS3::attributes()
	}
	
	prototype.child = function(propertyName):XMLList {
		var x:XML = this
		return x.AS3::child(propertyName)
	}

	prototype.childIndex = function():int {
		var x:XML = this
		return x.AS3::childIndex()
	}

	prototype.children = function():XMLList {
		var x:XML = this
		return x.AS3::children()
	}
	
	prototype.comments = function():XMLList {
		var x:XML = this
		return x.AS3::comments()
	}
	
	prototype.contains = function(value):Boolean {
		var x:XML = this
		return x.AS3::contains(value)
	}
	
	prototype.copy = function():XML {
		var x:XML = this
		return x.AS3::copy()
	}
	
	prototype.descendants = function(name="*"):XMLList {
		var x:XML = this
		return x.AS3::descendants(name)
	}
	
	prototype.elements = function(name="*"):XMLList {
		var x:XML = this
		return x.AS3::elements(name)
	}
	
	prototype.hasComplexContent = function():Boolean {
		var x:XML = this
		return x.AS3::hasComplexContent()
	}
	
	prototype.hasSimpleContent = function():Boolean {
		var x:XML = this
		return x.AS3::hasSimpleContent()
	}
	
	prototype.inScopeNamespaces = function():Array {
		var x:XML = this
		return x.AS3::inScopeNamespaces()
	}
	
	prototype.insertChildAfter = function(child1, child2):*  {
		var x:XML = this
		return x.AS3::insertChildAfter(child1,child2)
	}
	
	prototype.insertChildBefore = function(child1, child2):* {
		var x:XML = this
		return x.AS3::insertChildBefore(child1,child2)
	}
	
	prototype.length = function():int {
		var x:XML = this
		return x.AS3::length()
	}
	
	prototype.localName = function():Object {
		var x:XML = this
		return x.AS3::localName()
	}

	prototype.name = function():Object {
		var x:XML = this
		return x.AS3::name()
	}
	
	prototype.namespace = function(prefix=null):* {
		var x:XML = this
		return x.AS3::namespace.apply(x, arguments)
	}

	prototype.namespaceDeclarations = function():Array {
		var x:XML = this
		return x.AS3::namespaceDeclarations()
	}
	
	prototype.nodeKind = function():String {
		var x:XML = this
		return x.AS3::nodeKind()
	}
	
	prototype.normalize = function():XML {
		var x:XML = this
		return x.AS3::normalize()
	}
	
	prototype.parent = function():* {
		var x:XML = this
		return x.AS3::parent()
	}

	prototype.processingInstructions = function(name="*"):XMLList {
		var x:XML = this
		return x.AS3::processingInstructions(name)
	}
	
	prototype.prependChild = function(value):XML {
		var x:XML = this
		return x.AS3::prependChild(value)
	}
	
	prototype.removeNamespace = function(ns):XML {
		var x:XML = this
		return x.AS3::removeNamespace(ns)
	}
	
	prototype.replace = function(propertyName, value):XML {
		var x:XML = this
		return x.AS3::replace(propertyName, value)
	}
	
	prototype.setChildren = function(value):XML {
		var x:XML = this
		return x.AS3::setChildren(value)
	}

	prototype.setLocalName = function(name):void {
		var x:XML = this
		x.AS3::setLocalName(name)
	}

	prototype.setName = function(name):void {
		var x:XML = this
		x.AS3::setName(name)
	}

	prototype.setNamespace = function(ns):void {
		var x:XML = this
		x.AS3::setNamespace(ns)
	}

	prototype.text = function():XMLList {
		var x:XML = this
		return x.AS3::text()
	}
	
	prototype.toXMLString = function():String {
		var x:XML = this
		return x.AS3::toXMLString()
	}
	
	
    _dontEnumPrototype(prototype);
}

public final dynamic class XMLList extends Object
{
	// { ReadOnly, DontDelete, DontEnum }
	public static const length = 1

	AS3 native function toString ():String
	AS3 function valueOf():XMLList { return this }

	// these Override (hide) the same functions from Object
	override AS3 native function hasOwnProperty (P=void 0):Boolean
	override AS3 native function propertyIsEnumerable (P=void 0):Boolean

	// XMLList functions
	AS3 native function attribute (arg):XMLList;
	AS3 native function attributes():XMLList;
	AS3 native function child (propertyName):XMLList;
	AS3 native function children ():XMLList;
	AS3 native function comments ():XMLList;
	AS3 native function contains (value):Boolean;
	AS3 native function copy ():XMLList;

	// E4X 13.4.4.12, pg 76
	AS3 native function descendants (name="*"):XMLList; // name is optional

	AS3 native function elements (name="*"):XMLList; // name is optional
	AS3 native function hasComplexContent ():Boolean;
	AS3 native function hasSimpleContent ():Boolean;
	AS3 native function length ():int;
	AS3 native function name ():Object;  // null or a string;
	AS3 native function normalize ():XMLList;
	AS3 native function parent ():*; // undefined or XML;
	AS3 native function processingInstructions (name="*"):XMLList; // name is optional
	AS3 native function text ():XMLList;
	AS3 native function toXMLString ():String;

	// These are not in the spec but work if the length of the XMLList is one
	// (Function just gets propagated to the first and only list element)
	AS3 native function addNamespace (ns):XML;
	AS3 native function appendChild (child):XML;
	AS3 native function childIndex():int;
	AS3 native function inScopeNamespaces ():Array;
	AS3 native function insertChildAfter (child1, child2):*; // undefined or this
	AS3 native function insertChildBefore (child1, child2):*; // undefined or this
	AS3 native function nodeKind ():String;
	AS3 native function namespace (prefix=null):*; // prefix is optional
	AS3 native function localName ():Object; // null or String
	AS3 native function namespaceDeclarations ():Array;
	AS3 native function prependChild (value):XML;
	AS3 native function removeNamespace (ns):XML;
	AS3 native function replace (propertyName, value):XML;
	AS3 native function setChildren (value):XML;
	AS3 native function setLocalName (name):void;
	AS3 native function setName (name):void;
	AS3 native function setNamespace (ns):void;

	// notification extensions(reserved)
	//public native function notification():Function;
	//public native function setNotification(f:Function);

    // Dummy constructor function - This is neccessary so the compiler can do arg # checking for the ctor in strict mode
    // The code for the actual ctor is in XMLList::construct in the avmplus
    public function XMLList(value = void 0)
    {}
    
    
	// this is what rhino appears to do, not sure why bother
	prototype.valueOf = Object.prototype.valueOf
	
	prototype.hasOwnProperty = function(P=void 0):Boolean {
		if (this === prototype) 
		{
			return this.AS3::hasOwnProperty(P);
		}
		var x:XMLList = this
		return x.AS3::hasOwnProperty(P)
	}
	
	prototype.propertyIsEnumerable = function(P=void 0):Boolean {
		if (this === prototype) 
		{
			return this.AS3::propertyIsEnumerable(P);
		}
		var x:XMLList = this
		return x.AS3::propertyIsEnumerable(P)
	}
	
	prototype.toString = function():String {
		if (this === prototype) 
		{
			return "";
		}
		var x:XMLList = this
		return x.AS3::toString()
	}
	
	prototype.addNamespace = function(ns):XML {
		var x:XMLList = this
		return x.AS3::addNamespace(ns)
	}
	
	prototype.appendChild = function(child):XML {
		var x:XMLList = this
		return x.AS3::appendChild(child)
	}
	
	prototype.attribute = function(arg):XMLList {
		var x:XMLList = this
		return x.AS3::attribute(arg)
	}

	prototype.attributes = function():XMLList {
		var x:XMLList = this
		return x.AS3::attributes()
	}
	
	prototype.child = function(propertyName):XMLList {
		var x:XMLList = this
		return x.AS3::child(propertyName)
	}

	prototype.childIndex = function():int {
		var x:XMLList = this
		return x.AS3::childIndex()
	}

	prototype.children = function():XMLList {
		var x:XMLList = this
		return x.AS3::children()
	}
	
	prototype.comments = function():XMLList {
		var x:XMLList = this
		return x.AS3::comments()
	}
	
	prototype.contains = function(value):Boolean {
		var x:XMLList = this
		return x.AS3::contains(value)
	}
	
	prototype.copy = function():XMLList {
		var x:XMLList = this
		return x.AS3::copy()
	}
	
	prototype.descendants = function(name="*"):XMLList {
		var x:XMLList = this
		return x.AS3::descendants(name)
	}
	
	prototype.elements = function(name="*"):XMLList {
		var x:XMLList = this
		return x.AS3::elements(name)
	}
	
	prototype.hasComplexContent = function():Boolean {
		var x:XMLList = this
		return x.AS3::hasComplexContent()
	}
	
	prototype.hasSimpleContent = function():Boolean {
		var x:XMLList = this
		return x.AS3::hasSimpleContent()
	}
	
	prototype.inScopeNamespaces = function():Array {
		var x:XMLList = this
		return x.AS3::inScopeNamespaces()
	}
	
	prototype.insertChildAfter = function(child1, child2):* {
		var x:XMLList = this
		return x.AS3::insertChildAfter(child1,child2)
	}
	
	prototype.insertChildBefore = function(child1, child2):* {
		var x:XMLList = this
		return x.AS3::insertChildBefore(child1,child2)
	}
	
	prototype.length = function():int {
		var x:XMLList = this
		return x.AS3::length()
	}
	
	prototype.localName = function():Object {
		var x:XMLList = this
		return x.AS3::localName()
	}

	prototype.name = function():Object {
		var x:XMLList = this
		return x.AS3::name()
	}
	
	prototype.namespace = function(prefix=null):* {
		var x:XMLList = this
		return x.AS3::namespace.apply(x, arguments)
	}

	prototype.namespaceDeclarations = function():Array {
		var x:XMLList = this
		return x.AS3::namespaceDeclarations()
	}
	
	prototype.nodeKind = function():String {
		var x:XMLList = this
		return x.AS3::nodeKind()
	}
	
	prototype.normalize = function():XMLList {
		var x:XMLList = this
		return x.AS3::normalize()
	}
	
	prototype.parent = function():* {
		var x:XMLList = this
		return x.AS3::parent()
	}

	prototype.processingInstructions = function(name="*"):XMLList {
		var x:XMLList = this
		return x.AS3::processingInstructions(name)
	}
	
	prototype.prependChild = function(value):XML {
		var x:XMLList = this
		return x.AS3::prependChild(value)
	}
	
	prototype.removeNamespace = function(ns):XML {
		var x:XMLList = this
		return x.AS3::removeNamespace(ns)
	}
	
	prototype.replace = function(propertyName, value):XML {
		var x:XMLList = this
		return x.AS3::replace(propertyName, value)
	}
	
	prototype.setChildren = function(value):XML {
		var x:XMLList = this
		return x.AS3::setChildren(value)
	}

	prototype.setLocalName = function(name):void {
		var x:XMLList = this
		x.AS3::setLocalName(name)
	}

	prototype.setName = function(name):void {
		var x:XMLList = this
		x.AS3::setName(name)
	}

	prototype.setNamespace = function(ns):void {
		var x:XMLList = this
		x.AS3::setNamespace(ns)
	}

	prototype.text = function():XMLList {
		var x:XMLList = this
		return x.AS3::text()
	}
	
	prototype.toXMLString = function():String {
		var x:XMLList = this
		return x.AS3::toXMLString()
	}

    _dontEnumPrototype(prototype);

}

public final class QName extends Object
{
	// E262 {DontDelete, ReadOnly, DontEnum}
	public static const length = 2

	// E357 {DontDelete, ReadOnly}
	public native function get localName():String

	// E357 {DontDelete, ReadOnly}
	public native function get uri()

	AS3 function valueOf():QName { return this }

	AS3 function toString():String {
		if (uri === "")
			return localName
		return (uri===null ? "*" : uri) + "::" + localName
	}

	prototype.toString = function():String
	{
		if (this === prototype) return ""
		if (!(this is QName))
			Error.throwError( TypeError, 1004 /*kInvokeOnIncompatibleObjectError*/, "QName.prototype.toString" );
		var q:QName = this
		return q.AS3::toString()
	}

    // Dummy constructor function - This is neccessary so the compiler can do arg # checking for the ctor in strict mode
    // The code for the actual ctor is in QName::construct in the avmplus
    public function QName(namespace = void 0, name = void 0)
    {}
    
    _dontEnumPrototype(prototype);
}

}
