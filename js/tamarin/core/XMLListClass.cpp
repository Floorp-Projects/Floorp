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


#include "avmplus.h"

namespace avmplus
{
	BEGIN_NATIVE_MAP(XMLListClass)
		NATIVE_METHOD(XMLList_AS3_attribute, XMLListObject::attribute)
		NATIVE_METHOD(XMLList_AS3_attributes, XMLListObject::attributes)
		NATIVE_METHOD(XMLList_AS3_child, XMLListObject::child)
		NATIVE_METHOD(XMLList_AS3_children, XMLListObject::children)
		NATIVE_METHOD(XMLList_AS3_comments, XMLListObject::comments)
		NATIVE_METHOD(XMLList_AS3_contains, XMLListObject::contains)
		NATIVE_METHOD(XMLList_AS3_copy, XMLListObject::copy)
		NATIVE_METHOD(XMLList_AS3_descendants, XMLListObject::descendants)
		NATIVE_METHOD(XMLList_AS3_elements, XMLListObject::elements)
		NATIVE_METHOD(XMLList_AS3_hasOwnProperty, XMLListObject::hasOwnProperty)
		NATIVE_METHOD(XMLList_AS3_hasComplexContent, XMLListObject::hasComplexContent)
		NATIVE_METHOD(XMLList_AS3_hasSimpleContent, XMLListObject::hasSimpleContent)
		NATIVE_METHOD(XMLList_AS3_length, XMLListObject::AS_based_length)
		NATIVE_METHOD(XMLList_AS3_name, XMLListObject::name)
		NATIVE_METHOD(XMLList_AS3_normalize, XMLListObject::normalize)
		NATIVE_METHOD(XMLList_AS3_parent, XMLListObject::parent)
		NATIVE_METHOD(XMLList_AS3_processingInstructions, XMLListObject::processingInstructions)
		NATIVE_METHOD(XMLList_AS3_propertyIsEnumerable, XMLListObject::propertyIsEnumerable)
		NATIVE_METHOD(XMLList_AS3_text, XMLListObject::text)
		NATIVE_METHOD(XMLList_AS3_toString, XMLListObject::toStringMethod)
		NATIVE_METHOD(XMLList_AS3_toXMLString, XMLListObject::toXMLString)

		// These are not in the spec but work if there's only element in the XMLList
		NATIVE_METHOD(XMLList_AS3_addNamespace, XMLListObject::addNamespace)
		NATIVE_METHOD(XMLList_AS3_appendChild, XMLListObject::appendChild)
		NATIVE_METHOD(XMLList_AS3_childIndex, XMLListObject::childIndex)
		NATIVE_METHOD(XMLList_AS3_inScopeNamespaces, XMLListObject::inScopeNamespaces)
		NATIVE_METHOD(XMLList_AS3_insertChildAfter, XMLListObject::insertChildAfter)
		NATIVE_METHOD(XMLList_AS3_insertChildBefore, XMLListObject::insertChildBefore)
		NATIVE_METHODV(XMLList_AS3_namespace, XMLListObject::getNamespace)
		NATIVE_METHOD(XMLList_AS3_localName, XMLListObject::localName)
		NATIVE_METHOD(XMLList_AS3_namespaceDeclarations, XMLListObject::namespaceDeclarations)
		NATIVE_METHOD(XMLList_AS3_nodeKind, XMLListObject::nodeKind)
		NATIVE_METHOD(XMLList_AS3_prependChild, XMLListObject::prependChild)
		NATIVE_METHOD(XMLList_AS3_removeNamespace, XMLListObject::removeNamespace)
		NATIVE_METHOD(XMLList_AS3_replace, XMLListObject::replace)
		NATIVE_METHOD(XMLList_AS3_setChildren, XMLListObject::setChildren)
		NATIVE_METHOD(XMLList_AS3_setLocalName, XMLListObject::setLocalName)
		NATIVE_METHOD(XMLList_AS3_setName, XMLListObject::setName)
		NATIVE_METHOD(XMLList_AS3_setNamespace, XMLListObject::setNamespace)

	END_NATIVE_MAP()

	XMLListClass::XMLListClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		AvmAssert(traits()->sizeofInstance == sizeof(XMLListClass));
		createVanillaPrototype();
	}

	// E4X 10.4, pg 36
	Atom XMLListClass::ToXMLList(Atom arg)
	{
		AvmCore* core = this->core();

		if (AvmCore::isNullOrUndefined(arg))
		{
			toplevel()->throwTypeError(
					   (arg == undefinedAtom) ? kConvertUndefinedToObjectError :
											kConvertNullToObjectError);
			return arg;
		}

		if (core->isXMLList(arg))
		{
			return arg;
		}
		else if (core->isXML (arg))
		{
			XMLObject *x = core->atomToXMLObject(arg);
			Multiname m;
			bool bFound = x->getQName (&m);
			XMLListObject *xl = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass(), x->parent(), bFound ? &m : 0);
			xl->_append (arg);
			return xl->atom();
		}
		else
		{
			Toplevel* toplevel = this->toplevel();
	
			Stringp s = core->string(arg);

			Stringp startTag = new (core->GetGC()) String(s, 0, 2);
			Stringp endTag = new (core->GetGC()) String(s, s->length() - 3, 3);

			if (startTag->Equals("<>") && endTag->Equals("</>"))
			{
				s = new (core->GetGC()) String(s, 2, s->length() - 5);
			}

			Namespace *defaultNamespace = toplevel->getDefaultNamespace();
			// We handle this step in the XMLObject constructor to avoid concatenation huge strings together
			// parentString = <parent xnlns=defaultNamespace> s </parent>
			// 3. Parse parentString as a W3C element information info e
			// 4. If the parse fails, throw a SyntaxError exception
			// 5. x = toXML(e);
			//StringBuffer parentString (core);
			//parentString << "<parent xmlns=\"";
			//parentString << defaultNamespace->getURI();
			//parentString << "\">";
			//parentString << s;
			//parentString << "</parent>";
			XMLObject *x = new (core->GetGC()) XMLObject(toplevel->xmlClass(), s, defaultNamespace);

			XMLListObject *xl = new (core->GetGC()) XMLListObject(toplevel->xmlListClass());
			for (uint32 i = 0; i < x->getNode()->_length(); i++)
			{
				E4XNode *c = x->getNode()->_getAt (i);
				c->setParent (NULL);

				// !!@ trying to emulate rhino behavior here
				// Add the default namespace to our top element.
				Namespace *ns = toplevel->getDefaultNamespace();
				c->_addInScopeNamespace (core, ns);

				xl->_append (c);
			}
			return xl->atom();
		}
	}

	// E4X 13.5.2, pg 87
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom XMLListClass::construct(int argc, Atom* argv)
	{
		AvmCore* core = this->core();
		if ((!argc) || AvmCore::isNullOrUndefined(argv[1]))
		{
			return ToXMLList (core->newString("")->atom());
		}

		// if args[0] is xmllist, create new list and call append
		if (core->isXMLList(argv[1]))
		{
			XMLListObject *l = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass());
			l->_append (argv[1]);
			return l->atom();
		}

		return ToXMLList(argv[1]);
	}
	
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom XMLListClass::call(int argc, Atom* argv)
	{
		if ((!argc) || AvmCore::isNullOrUndefined(argv[1]))
		{
			return ToXMLList (core()->kEmptyString->atom());
		}

		return ToXMLList(argv[1]);
    }
}
