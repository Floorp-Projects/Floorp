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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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


#include "avmplus.h"

namespace avmplus
{
	BEGIN_NATIVE_MAP(XMLClass)
		NATIVE_METHOD(XML_ignoreComments_get, XMLClass::getIgnoreComments)
		NATIVE_METHOD(XML_ignoreComments_set, XMLClass::setIgnoreComments)
		NATIVE_METHOD(XML_ignoreProcessingInstructions_get, XMLClass::getIgnoreProcessingInstructions)
		NATIVE_METHOD(XML_ignoreProcessingInstructions_set, XMLClass::setIgnoreProcessingInstructions)
		NATIVE_METHOD(XML_ignoreWhitespace_get, XMLClass::getIgnoreWhitespace)
		NATIVE_METHOD(XML_ignoreWhitespace_set, XMLClass::setIgnoreWhitespace)
		NATIVE_METHOD(XML_prettyPrinting_get, XMLClass::getPrettyPrinting)
		NATIVE_METHOD(XML_prettyPrinting_set, XMLClass::setPrettyPrinting)
		NATIVE_METHOD(XML_prettyIndent_get, XMLClass::getPrettyIndent)
		NATIVE_METHOD(XML_prettyIndent_set, XMLClass::setPrettyIndent)

		NATIVE_METHOD(XML_AS3_addNamespace, XMLObject::addNamespace)
		NATIVE_METHOD(XML_AS3_appendChild, XMLObject::appendChild)
		NATIVE_METHOD(XML_AS3_attribute, XMLObject::attribute)
		NATIVE_METHOD(XML_AS3_attributes, XMLObject::attributes)
		NATIVE_METHOD(XML_AS3_child, XMLObject::child)
		NATIVE_METHOD(XML_AS3_childIndex, XMLObject::childIndex)
		NATIVE_METHOD(XML_AS3_children, XMLObject::children)
		NATIVE_METHOD(XML_AS3_comments, XMLObject::comments)
		NATIVE_METHOD(XML_AS3_contains, XMLObject::contains)
		NATIVE_METHOD(XML_AS3_copy, XMLObject::copy)
		NATIVE_METHOD(XML_AS3_descendants, XMLObject::descendants)
		NATIVE_METHOD(XML_AS3_elements, XMLObject::elements)
		NATIVE_METHOD(XML_AS3_hasOwnProperty, XMLObject::hasOwnProperty)
		NATIVE_METHOD(XML_AS3_hasComplexContent, XMLObject::hasComplexContent)
		NATIVE_METHOD(XML_AS3_hasSimpleContent, XMLObject::hasSimpleContent)
		NATIVE_METHOD(XML_AS3_inScopeNamespaces, XMLObject::inScopeNamespaces)
		NATIVE_METHOD(XML_AS3_insertChildAfter, XMLObject::insertChildAfter)
		NATIVE_METHOD(XML_AS3_insertChildBefore, XMLObject::insertChildBefore)
		NATIVE_METHOD(XML_AS3_localName, XMLObject::localName)
		NATIVE_METHOD(XML_AS3_name, XMLObject::name)
		NATIVE_METHODV(XML_AS3_namespace, XMLObject::getNamespace)
		NATIVE_METHOD(XML_AS3_namespaceDeclarations, XMLObject::namespaceDeclarations)
		NATIVE_METHOD(XML_AS3_nodeKind, XMLObject::nodeKind)
		NATIVE_METHOD(XML_AS3_normalize, XMLObject::normalize)
		NATIVE_METHOD(XML_AS3_notification, XMLObject::getNotification)
		NATIVE_METHOD(XML_AS3_parent, XMLObject::parent)
		NATIVE_METHOD(XML_AS3_processingInstructions, XMLObject::processingInstructions)
		NATIVE_METHOD(XML_AS3_prependChild, XMLObject::prependChild)
		NATIVE_METHOD(XML_AS3_propertyIsEnumerable, XMLObject::xmlPropertyIsEnumerable)
		NATIVE_METHOD(XML_AS3_removeNamespace, XMLObject::removeNamespace)
		NATIVE_METHOD(XML_AS3_replace, XMLObject::replace)
		NATIVE_METHOD(XML_AS3_setChildren, XMLObject::setChildren)
		NATIVE_METHOD(XML_AS3_setLocalName, XMLObject::setLocalName)
		NATIVE_METHOD(XML_AS3_setName, XMLObject::setName)
		NATIVE_METHOD(XML_AS3_setNamespace, XMLObject::setNamespace)
		NATIVE_METHOD(XML_AS3_setNotification, XMLObject::setNotification)
		NATIVE_METHOD(XML_AS3_text, XMLObject::text)
		NATIVE_METHOD(XML_AS3_toString, XMLObject::toStringMethod)
		NATIVE_METHOD(XML_AS3_toXMLString, XMLObject::toXMLString)
	END_NATIVE_MAP()

	XMLClass::XMLClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(XMLClass));

		AvmCore* core = this->core();

		createVanillaPrototype();

		// These are static objects on the XML type
		// E4X: The XML constructor has the following properties
		// XML.ignoreComments
		// XML.ignoreProcessingInstructions
		// XML.ignoreWhitespace
		// XML.prettyPrinting
		// XML.prettyIndent
		m_flags = kFlagIgnoreComments | kFlagIgnoreProcessingInstructions | kFlagIgnoreWhitespace | kFlagPrettyPrinting;
		m_prettyIndent = 2;

		kAttribute = core->constantString("attribute");
		kComment = core->constantString("comment");
		kProcessingInstruction = core->constantString("processing-instruction");
		kElement = core->constantString("element");
		kText = core->constantString("text");

		kColon = core->constantString(":");
		kXml = core->constantString("xml");
		nsXML = core->newNamespace (core->kEmptyString->atom(), core->internString (core->newString ("http://www.w3.org/XML/1998/namespace"))->atom()); 

		// for notifications
		kAttrAdded = core->constant("attributeAdded");
		kAttrRemoved = core->constant("attributeRemoved");
		kAttrChanged = core->constant("attributeChanged");
		kNodeAdded = core->constant("nodeAdded");
		kNodeRemoved = core->constant("nodeRemoved");
		kNodeChanged = core->constant("nodeChanged");
		kNamespaceAdded = core->constant("namespaceAdded");
		kNamespaceRemoved = core->constant("namespaceRemoved");
		kNamespaceSet = core->constant("namespaceSet");
		kNameSet = core->constant("nameSet");
		kTextSet = core->constant("textSet");

		// XML.settings
		// XML.setSettings ([settings])
		// XML.defaultSettings()
	}

	XMLClass::~XMLClass()
	{
		m_prettyIndent = 0;
		m_flags = 0;
	}

	// E4X 13.4.2, page 70
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom XMLClass::construct(int argc, Atom* argv)
	{
		// We are no longer supporting this weird Rhino behavior.  According to the spec
		// and a discussion with the E4X developers, Rhino is buggy and a no argument
		// constructor should be equivalent to the empty string constructor.
		// !!@ This is different then the spec but runtime behavior of Rhino shows
		// that a "new XML()" call creates some sort of special node that can change 
		// type automatically.
		//var x = new XML();
		//print(x); // prints nothing
		//print(x.nodeKind()); // prints text
		//print(x.children()); // prints nothing
		//x.foo = 5;
		//print(x); // prints 5
		//print(x.nodeKind()); // prints element - how did it switch?
		//print(x.attributes()); // prints nothing
		//print(x.children()); // prints 5

		AvmCore* core = this->core();

		if (!argc || AvmCore::isNullOrUndefined(argv[1]))
		{
			return ToXML (core->kEmptyString->atom());
		}

		Atom x = ToXML (argv[1]);
		// if args[0] is xml, xmllist or w3c xml, return a deep copy
		if (core->isXML(argv[1]) || core->isXMLList(argv[1]))
		{
			// return deepCopy of x
			XMLObject *x2 = core->atomToXMLObject(x);
			return x2->_deepCopy()->atom();
		}

		return x;
	}
	
	// E4X 13.4.1, page 70
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom XMLClass::call(int argc, Atom* argv)
	{
		if ((!argc) || AvmCore::isNullOrUndefined(argv[1]))
		{
			return ToXML (core()->kEmptyString->atom());
		}

		return ToXML(argv[1]);
    }

	// E4X 10.3, page 32
	Atom XMLClass::ToXML(Atom arg)
	{
		Toplevel* toplevel = this->toplevel();
		AvmCore* core = this->core();

		if (AvmCore::isNullOrUndefined(arg))
		{
			toplevel->throwTypeError(
					   (arg == undefinedAtom) ? kConvertUndefinedToObjectError :
											kConvertNullToObjectError);
			return arg;
		}
		else if (core->isXML(arg))
		{
			return arg;
		}
		else if (core->isXMLList (arg))
		{
			XMLListObject *xl = core->atomToXMLList (arg);
			if (xl->_length() == 1)
			{
				return xl->_getAt(0)->atom();
			}
			else
			{
				toplevel->throwTypeError(kXMLMarkupMustBeWellFormed);
				return 0;//notreached
			}
		}
		else
		{
			Namespace *defaultNamespace = toplevel->getDefaultNamespace();
			// parentString = <parent xnlns=defaultNamespace> s </parent>
			//Stringp parentString = core->concatStrings(core->newString("<parent xmlns=\""),defaultNamespace->getURI());
			//parentString = core->concatStrings(parentString, core->newString("\">"));
			//parentString = core->concatStrings(parentString, core->string(arg));
			//parentString = core->concatStrings(parentString, core->newString("</parent>"));
			// 2. Parse parentString as a W3C element information info e
			// 3. If the parse fails, throw a SyntaxError exception
			XMLObject *x = new (core->GetGC()) XMLObject(toplevel->xmlClass(), core->string(arg), defaultNamespace);

			// 4. x = toXML(e);
			// 5. if x.length == 0
			//       return new xml object
			if (!x->getNode()->_length())
			{
				x->setNode( new (core->GetGC()) TextE4XNode(0, core->kEmptyString) );
			}
			// 6. else if x.length == 1
			//       x[0].parent = null
			//		return x[0]
			else if (x->getNode()->_length() == 1)
			{
				x->setNode( x->getNode()->_getAt (0)); // discard parent node
				x->getNode()->setParent (0);
			}
			// 7. else throw a syntaxError
			else
			{
				// check for multiple nodes where the first n-1 are PI/comment nodes and the 
				// last one is an element node.  Just ignore the PI/comment nodes and return
				// the element node.  (bug 148526).
				E4XNode *node = x->getNode();
				E4XNode *lastNode = node->_getAt (node->_length() - 1);
				if (lastNode->getClass() != E4XNode::kElement)
					toplevel->throwTypeError(kXMLMarkupMustBeWellFormed);

				for (uint32 i = 0; i < node->_length() - 1; i++)
				{
					if ((node->_getAt (i)->getClass() != E4XNode::kProcessingInstruction) &&
						(node->_getAt (i)->getClass() != E4XNode::kComment))

						toplevel->throwTypeError(kXMLMarkupMustBeWellFormed);
				}

				x->setNode( lastNode ); // discard parent node
				lastNode->setParent (0);
			}
			return x->atom();
		}
	}

	void XMLClass::setIgnoreComments(uint32 ignoreFlag) 
	{ 
		if (ignoreFlag)
			m_flags |= kFlagIgnoreComments; 
		else
			m_flags &= ~kFlagIgnoreComments; 
	}

	uint32 XMLClass::getIgnoreComments() 
	{ 
		return ((m_flags & kFlagIgnoreComments) != 0); 
	}

	void XMLClass::setIgnoreProcessingInstructions(uint32 ignoreFlag)
	{ 
		if (ignoreFlag)
			m_flags |= kFlagIgnoreProcessingInstructions; 
		else
			m_flags &= ~kFlagIgnoreProcessingInstructions; 
	}

	uint32 XMLClass::getIgnoreProcessingInstructions() 
	{ 
		return ((m_flags & kFlagIgnoreProcessingInstructions) != 0); 
	}

	void XMLClass::setIgnoreWhitespace(uint32 ignoreFlag)
	{ 
		if (ignoreFlag)
			m_flags |= kFlagIgnoreWhitespace; 
		else
			m_flags &= ~kFlagIgnoreWhitespace; 
	}
	
	uint32 XMLClass::getIgnoreWhitespace()
	{ 
		return ((m_flags & kFlagIgnoreWhitespace) != 0); 
	}

	void XMLClass::setPrettyPrinting(uint32 prettyFlag)
	{
		if (prettyFlag)
			m_flags |= kFlagPrettyPrinting; 
		else
			m_flags &= ~kFlagPrettyPrinting; 
	}

	uint32 XMLClass::getPrettyPrinting()
	{
		return ((m_flags & kFlagPrettyPrinting) != 0); 
	}

	void XMLClass::setPrettyIndent(int printVal)
	{
		m_prettyIndent = printVal;
	}

	int XMLClass::getPrettyIndent()
	{
		return m_prettyIndent;
	}

	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////

	BEGIN_NATIVE_MAP(QNameClass)
		NATIVE_METHOD(QName_localName_get, QNameObject::getLocalName)
		NATIVE_METHOD(QName_uri_get, QNameObject::getURI)
	END_NATIVE_MAP()

	QNameClass::QNameClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(QNameClass));

		createVanillaPrototype();

		AvmCore* core = this->core();
		kUri = core->constant("uri");
		kLocalName = core->constant("localName");
	}

	// E4X 13.3.1, page 66
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom QNameClass::call(int argc, Atom* argv)
	{
		if (argc == 1)
		{
			AvmCore* core = this->core();
			if (core->isObject(argv[1]) && core->istype(argv[1], core->traits.qName_itraits))
				return argv[1];
		}

		return construct (argc, argv);
	}

	// E4X 13.3.2, page 67
	Atom QNameClass::construct(int argc, Atom* argv)
	{
		AvmCore* core = this->core();

		if (argc == 0)
			return (new (core->GetGC(), ivtable()->getExtraSize()) QNameObject(this, undefinedAtom))->atom();

		if (argc == 1)
		{
			if (core->isObject(argv[1]) && core->istype(argv[1], core->traits.qName_itraits))
				return argv[1];

			return (new (core->GetGC(), ivtable()->getExtraSize()) QNameObject(this, argv[1]))->atom();
		}
		else
		{
			Atom a = argv[1];
			if (a == undefinedAtom)
			{
				// ns undefined same as unspecified
				return (new (core->GetGC(), ivtable()->getExtraSize()) QNameObject(this, argv[2]))->atom();
			}
			else
			{
				Namespace* ns;
				if (AvmCore::isNull(a))
					ns = NULL;
				// It's important to preserve the incoming namespace because it's type
				// may not be public.  I.E...
				// namespace ns;
				// q = new QName(ns, "name"); // ns is a NS_PackageInternal ns
				// If we ever use this QName as a multiname, we need to preserve the exact ns
				else if (core->isNamespace(a))
					ns = core->atomToNamespace(a);				
				else
					ns = core->newNamespace (a);
				return (new (core->GetGC(), ivtable()->getExtraSize()) QNameObject(this, ns, argv[2]))->atom();
			}
		}
	}
}
