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
	extern wchar *stripPrefix (const wchar *str, const char *pre);

	E4XNodeAux::E4XNodeAux (Stringp s, Namespace *ns, ScriptObject* notify)
	{
		m_name = s;
		m_ns = ns;
		m_notification = notify;
	}

	AttributeE4XNode::AttributeE4XNode (E4XNode *parent, String *value) : E4XNode(parent)
	{
		m_value = value;
	}

	CDATAE4XNode::CDATAE4XNode (E4XNode *parent, String *value) : E4XNode(parent)
	{
		m_value = value;
	}

	CommentE4XNode::CommentE4XNode (E4XNode *parent, String *value) : E4XNode(parent)
	{
		m_value = value;
	}

	PIE4XNode::PIE4XNode (E4XNode *parent, String *value) : E4XNode(parent)
	{
		m_value = value;
	}

	TextE4XNode::TextE4XNode (E4XNode *parent, String *value) : E4XNode(parent)
	{
		m_value = value;
	}

	ElementE4XNode::ElementE4XNode (E4XNode *parent) : E4XNode(parent)
	{
	}

	// Fast append with no checks for type, etc.
	void ElementE4XNode::_append (E4XNode *childNode)
	{
		childNode->setParent (this);
		if (!m_children)
		{
			m_children = uintptr(childNode) | SINGLECHILDBIT;
			return;
		}

		if (m_children & SINGLECHILDBIT)
		{
			convertToAtomArray();
		}

		AtomArray *aa = ((AtomArray *)(uintptr)m_children);
		aa->push (AvmCore::gcObjectToAtom(childNode));
	}

	uint32 ElementE4XNode::numChildren() const
	{
		if (!m_children)
			return 0;

		if (m_children & SINGLECHILDBIT)
			return 1;
		else 
		{
			AtomArray *aa = ((AtomArray *)(uintptr)m_children);
			return aa->getLength();
		}
	}

	void ElementE4XNode::clearChildren()
	{
		if (m_children & ~SINGLECHILDBIT)
		{
			// !!@ delete our AtomArray
		}

		m_children = 0;
	}

	void ElementE4XNode::convertToAtomArray ()
	{
		if (m_children & SINGLECHILDBIT)
		{
			E4XNode *firstChild = (E4XNode *) (m_children & ~SINGLECHILDBIT);
			AtomArray *aa = new (gc()) AtomArray(2);
			aa->push (AvmCore::gcObjectToAtom(firstChild));
			m_children = uintptr(aa);
		}
		else if (!m_children)
		{
			m_children = uintptr(new (gc()) AtomArray (1));
		}
	}

	void ElementE4XNode::insertChild (uint32 i, E4XNode *x)
	{
		// m_children->insert (i, a)
		convertToAtomArray();
		AtomArray *aa = ((AtomArray *)(uintptr)m_children);
		aa->insert (i, AvmCore::gcObjectToAtom(x));
	}

	void ElementE4XNode::removeChild (uint32 i)
	{
		// m_children->removeAt (i)
		convertToAtomArray();
		AtomArray *aa = ((AtomArray *)(uintptr)m_children);
		aa->removeAt (i);
	}

	void ElementE4XNode::setChildAt (uint32 i, E4XNode *x)
	{
		if ((i == 0) && (m_children & SINGLECHILDBIT))
		{
			m_children = uintptr(x) | SINGLECHILDBIT;
		}
		else
		{
			convertToAtomArray();
			AtomArray *aa = ((AtomArray *)(uintptr)m_children);
			aa->setAt (i, AvmCore::gcObjectToAtom(x));
		}
	}

	E4XNode *ElementE4XNode::_getAt(uint32 i) const
	{
		if (i >= _length())
			return 0;

		if (int(this->m_children) & SINGLECHILDBIT)
		{
			return (E4XNode *)(this->m_children & ~SINGLECHILDBIT);
		}
		else
		{
			AtomArray *aa = (AtomArray *)(uintptr)this->m_children;
			E4XNode *x = (E4XNode *) AvmCore::atomToGCObject(aa->getAt(i));
			return x;
		}
	}

	uint32 ElementE4XNode::childIndex() const
	{
		// pull the parent
		int index = -1;
		E4XNode* p = this->getParent();
		if (p) 
		{
			// yea old linear search...
			int size = p->numChildren();
			for(int i=0; i<size; i++)
			{
				if (p->_getAt(i) == const_cast<ElementE4XNode*>(this))
				{
					index = i;
					break;
				}
			}
			AvmAssert(index < size); // My parent should know about me!
		}
		return index;
	}

	bool E4XNode::getQName(AvmCore *core, Multiname *mn) const
	{
		if (!m_nameOrAux)
			return false;

		uintptr nameOrAux = m_nameOrAux;
		if (AUXBIT & nameOrAux)
		{
			E4XNodeAux *aux = (E4XNodeAux *)(nameOrAux & ~AUXBIT);
			// We can have a notification only aux which won't have a real name
			if (aux->m_name)
			{
				mn->setName (aux->m_name);
				mn->setNamespace (aux->m_ns);
				mn->setQName();
			}
			else
			{
				return false;
			}
		}
		else
		{
			Stringp str = (String *)(nameOrAux);
			mn->setName (str);
			mn->setNamespace (core->publicNamespace);
		}

		if (getClass() == kAttribute)
			mn->setAttr();

		return true;
	}

	void E4XNode::setQName (AvmCore *core, Stringp name, Namespace *ns)
	{
		// If we already have an aux, use it.  (It may have notification atom set)
		uintptr nameOrAux = m_nameOrAux;
		if (AUXBIT & nameOrAux)
		{
			E4XNodeAux *aux = (E4XNodeAux *)(nameOrAux & ~AUXBIT);
			aux->m_name = name;
			aux->m_ns = ns;
			return;
		}

		if (!name && !ns)
		{
			m_nameOrAux = 0;
			return;
		}

		if (!ns || ns == core->publicNamespace || 
			(ns->getPrefix() == core->kEmptyString->atom() && ns->getURI() == core->kEmptyString))
		{
			//m_nameOrAux = int (name);
			WBRC(core->GetGC(), this, &m_nameOrAux, uintptr(name));
			return;
		}

		E4XNodeAux *aux = new (core->GetGC()) E4XNodeAux (name, ns);
		//m_nameOrAux = AUXBIT | int(aux);
		WB(core->GetGC(), this, &m_nameOrAux, AUXBIT | uintptr(aux));
	}

	void E4XNode::setQName (AvmCore *core, Multiname *mn)
	{
		if (!mn)
		{
			m_nameOrAux = 0;
		}
		else
		{
			setQName (core, mn->getName(), mn->getNamespace());
		}
	}

	// E4X 9.1.1.3, pg 20
	void E4XNode::_addInScopeNamespace (AvmCore* /*core*/, Namespace* /*ns*/)
	{
		// do nothing for non-element nodes
	}

	void ElementE4XNode::_addInScopeNamespace (AvmCore *core, Namespace *ns)
	{
//		if (getClass() & (kText | kCDATA | kComment | kProcessingInstruction | kAttribute))
//			return; 
		
		if (ns->getPrefix() == undefinedAtom)
			return;

		Multiname m;
		getQName (core, &m);

		if ((ns->getPrefix() == core->kEmptyString->atom()) && (!m.isAnyNamespace()) && (m.getNamespace()->getURI() == core->kEmptyString))
			return;

		// step 2b + 2c
		int index = -1;
		for (uint32 i = 0; i < numNamespaces(); i++)
		{
			Namespace *ns2 = AvmCore::atomToNamespace (getNamespaces()->getAt(i));
			if (ns2->getPrefix() == ns->getPrefix())
				index = i;
		}

		// step 2d
		if (index != -1)
		{
			Namespace *ns2 = AvmCore::atomToNamespace (getNamespaces()->getAt(index));
			if (ns2->getURI() != ns->getURI())
			{
				// remove match from inscopenamespaces
				m_namespaces->removeAt (index);
			}
		}

		// step 2e - add namespace to inscopenamespaces
		if (!m_namespaces)
			m_namespaces = new (core->GetGC()) AtomArray(1);

		m_namespaces->push (ns->atom());

		// step 2f
		// If this nodes prefix == n prefix
			// set this nodes prefix to undefined
		if  (!m.isAnyNamespace() && (m.getNamespace()->getPrefix() == ns->getPrefix()))
		{
			setQName (core, m.getName(), core->newNamespace (m.getNamespace()->getURI()));
		}

		// step 2g
		// for all attributes
		// if their nodes prefix == n.prefix
		//     set the node prefix to undefined
		for (unsigned int i = 0; i < numAttributes(); i++)
		{
			E4XNode *curAttr = (E4XNode *) (AvmCore::atomToGCObject(m_attributes->getAt(i)));
			Multiname ma;
			curAttr->getQName (core, &ma);
			if (!ma.isAnyNamespace() && ma.getNamespace()->getPrefix() == ns->getPrefix())
			{
				curAttr->setQName (core, ma.getName(), core->newNamespace (ma.getNamespace()->getURI()));
			}
		}

		return;
	}

	int E4XNode::FindMatchingNamespace (AvmCore *core, Namespace *ns)
	{
		for (uint32 i = 0; i < numNamespaces(); i++)
		{
			Namespace *ns2 = AvmCore::atomToNamespace (getNamespaces()->getAt(i));
			if (ns2->getURI() == ns->getURI())
			{
				if (ns->getPrefix() == undefinedAtom)
					return i;

				if (ns2->getPrefix() == core->kEmptyString->atom())
					return -1;

				if (ns2->getPrefix() == ns->getPrefix())
					return i;
			}
		}

		return -1;
	}

	Namespace *E4XNode::FindNamespace (AvmCore *core, Toplevel *toplevel, const wchar *tagName, const wchar **localName, bool bAttribute)
	{
		const wchar *ptr = tagName;
		Stringp prefix = core->kEmptyString;
		while (*ptr)
		{
			// found a separator between namespace prefix and localName
			if (*ptr == ':')
			{
				// handle case of ":name"
				if (ptr == tagName)
				{
					toplevel->throwTypeError(kXMLBadQName, core->toErrorString(tagName));
				}

				*localName = (ptr + 1);
				if (!*localName)
				{
					// throw error - badly formed prefix:localName pair
					// where is the localName?
					toplevel->throwTypeError(kXMLBadQName, core->toErrorString(tagName));
				}

				prefix = core->internAlloc (tagName, (ptr - tagName));
				break;
			}

			ptr++;
		}

		// An attribute without a prefix is unqualified and does not inherit a namespace
		// from its parent.  
		if (bAttribute && prefix == core->kEmptyString)
			return 0;

		// search all in scope namespaces for a matching prefix.  If we find one
		// return that prefix, otherwise we need to throw an error.
		E4XNode *y = this;
		while (y)
		{
			for (uint32 i = 0; i < y->numNamespaces(); i++)
			{
				Namespace *ns = AvmCore::atomToNamespace (y->getNamespaces()->getAt(i));
				if (((prefix == core->kEmptyString) && !ns->hasPrefix()) ||
					(prefix->atom() == ns->getPrefix()))
				{
					return ns;
				}
			}

			y = y->m_parent;
		}

		if (prefix == toplevel->xmlClass()->kXml)
		{
			return toplevel->xmlClass()->nsXML;
		}

		// throw error because we didn't match this prefix
		if (prefix != core->kEmptyString)
		{
			toplevel->throwTypeError(kXMLPrefixNotBound, prefix, core->toErrorString(tagName));
		}
		return 0;
	}

	void E4XNode::BuildInScopeNamespaceList (AvmCore* /*core*/, AtomArray *inScopeNS) const
	{
		const E4XNode *y = this;
		while (y)
		{
			for (uint32 i = 0; i < y->numNamespaces(); i++)
			{
				Namespace *ns1 = AvmCore::atomToNamespace (y->getNamespaces()->getAt(i));
				uint32 j;
				for (j = 0; j < inScopeNS->getLength(); j++)
				{
					Namespace *ns2 = AvmCore::atomToNamespace (inScopeNS->getAt(j));
					if (ns1->getPrefix() == undefinedAtom)
					{
						if (ns1->getURI() == ns2->getURI())
							break;
					}
					else
					{
						if (ns1->getPrefix() == ns2->getPrefix())
							break;
					}
				}

				if (j == inScopeNS->getLength()) // no match
				{
#ifdef STRING_DEBUG
					Stringp u = ns1->getURI();
					Stringp p = core->string(ns1->getPrefix());
#endif
					inScopeNS->push (ns1->atom());
				}
			}

			y = y->m_parent;
		}
	}

	void E4XNode::addAttribute (E4XNode* /*x*/)
	{
		AvmAssert(0);
	}

	void ElementE4XNode::addAttribute (E4XNode *x)
	{
		if (!m_attributes)
			m_attributes = new (gc()) AtomArray (1);

		m_attributes->push (AvmCore::gcObjectToAtom(x));
	}

	void ElementE4XNode::CopyAttributesAndNamespaces(AvmCore *core, Toplevel *toplevel, XMLTag& tag, const wchar *elementName)
	{
		m_attributes = 0;
		m_namespaces = 0;

		uint32 numAttr = 0;

		// We first handle namespaces because the a attribute tag can reference a namespace
		// defined farther on in the same node...
		// <ns2:table2 ns2:bar=\"last\" xmlns:ns2=\"http://www.macromedia.com/home\">...
		uint32 index = 0;
		Stringp attributeName, attributeValue;
		while (tag.nextAttribute(index, attributeName, attributeValue))
		{
			wchar *xmlns = stripPrefix (attributeName->c_str(), "xmlns");
			if (xmlns) // a namespace xnlns:prefix="URI" or xmlns="URI"
			{
				wchar *prefix = 0;
				if (xmlns[0] == ':')
				{
					// handle case of ":name"
					if (xmlns == attributeName->c_str())
					{
						toplevel->throwTypeError(kXMLBadQName, attributeName);
					}

					prefix = xmlns + 1;
					if (!prefix[0])
					{
						// throw exception because of badly formed XML???
						toplevel->throwTypeError(kXMLBadQName, attributeName);
					}
				}

				Namespace *ns;
				if (prefix)
				{
					Stringp prefixs = core->internAlloc (prefix, String::Length(prefix));
					ns = core->newNamespace (prefixs->atom(), attributeValue->atom());
				}
				else
				{
					ns = core->newNamespace (core->kEmptyString->atom(), attributeValue->atom());
				}
				// !!@ Don't intern these namespaces since the intern table ignores
				// the prefix value of the namespace.
				if (ns) // ns can be null if prefix is defined and attributeValue = ""
					this->_addInScopeNamespace (core, ns);
			}
			else
			{
				numAttr++;
			}
		}

		if (!numAttr)
			return;

		m_attributes = new (core->GetGC()) AtomArray (numAttr);

		// Now we read the attributes
		index = 0;
		while (tag.nextAttribute(index, attributeName, attributeValue))
		{
			wchar *xmlns = stripPrefix (attributeName->c_str(), "xmlns");
			if (!xmlns) // it's an attribute
			{
				// !!@ intern our attributeValue??
				E4XNode *attrObj = new (core->GetGC()) AttributeE4XNode(this, attributeValue);

				const wchar *localName = attributeName->c_str();
				Namespace *ns = this->FindNamespace (core, toplevel, attributeName->c_str(), &localName, true);
				if (!ns)
					ns = core->publicNamespace;
				Stringp name = core->internAlloc (localName, String::Length(localName));

				attrObj->setQName(core, name, ns);

				// check for a duplicate attribute here and throw a kXMLDuplicateAttribute if found

				Multiname m2;
				attrObj->getQName (core, &m2);
				for (unsigned int i = 0; i < numAttributes(); i++)
				{
					E4XNode *curAttr = (E4XNode *) (AvmCore::atomToGCObject(m_attributes->getAt(i)));
					Multiname m;
					curAttr->getQName (core, &m);
					if (m.matches (&m2))
					{
						toplevel->typeErrorClass()->throwError(kXMLDuplicateAttribute, attributeName, core->toErrorString(elementName), core->toErrorString(String::Length(elementName)));
					}
				}

				m_attributes->push (AvmCore::gcObjectToAtom(attrObj));
			}
		}
	}

	//////////////////////////////////////////////////////////////////////
	// E4X Section 9.1.1
	//////////////////////////////////////////////////////////////////////

	// E4X 9.1.1.4, pg 15
	void E4XNode::_deleteByIndex (uint32 i)
	{
		if ((i >= 0) && (i < numChildren()))
		{
			E4XNode *x = _getAt(i);
			if (x)
			{
				x->m_parent = NULL;
			}

			removeChild (i);
			AvmAssert(numChildren() ^ 0x80000000); // check for underflow
		}
	}

	// E4X 9.1.1.7, page 16
	E4XNode *E4XNode::_deepCopy (AvmCore *core, Toplevel *toplevel) const
	{
		E4XNode *x = 0;
		switch (this->getClass())
		{
		case kAttribute:
			x = new (core->GetGC()) AttributeE4XNode (0, getValue());
			break;
		case kText:
			x = new (core->GetGC()) TextE4XNode (0, getValue());
			break;
		case kCDATA:
			x = new (core->GetGC()) CDATAE4XNode (0, getValue());
			break;
		case kComment:
			x = new (core->GetGC()) CommentE4XNode (0, getValue());
			break;
		case kProcessingInstruction:
			x = new (core->GetGC()) PIE4XNode (0, getValue());
			break;
		case kElement:
			x = new (core->GetGC()) ElementE4XNode (0);
			break;
		}

		Multiname m;
		if (this->getQName (core, &m))
		{
			x->setQName (core, &m); 
		}

		if (x->getClass() == kElement)
		{
			ElementE4XNode *y = (ElementE4XNode *) x;

			// step 2 - for each ns in inScopeNamespaces
			if (numNamespaces())
			{
				y->m_namespaces = new (core->GetGC()) AtomArray (numNamespaces());
				uint32 i;
				for (i = 0; i < numNamespaces(); i++)
				{
					y->m_namespaces->push(getNamespaces()->getAt(i));
				}
			}

			// step 3 - duplicate attribute nodes
			if (numAttributes())
			{
				y->m_attributes = new (core->GetGC()) AtomArray (numAttributes());
				uint32 i;
				for (i = 0; i < numAttributes(); i++)
				{
					E4XNode *ax = getAttribute (i);
					E4XNode *bx = ax->_deepCopy(core, toplevel);
					bx->setParent(y);
					y->addAttribute(bx);
				}
			}

			// step 4 - duplicate children
			if (numChildren())
			{
				AvmAssert(y->m_children == 0);
				y->m_children = uintptr(new (core->GetGC()) AtomArray (numChildren()));
				for (uint32 k = 0; k < _length(); k++)
				{
					E4XNode *child = _getAt(k);
					if (((child->getClass() == E4XNode::kComment) && toplevel->xmlClass()->getIgnoreComments()) ||
						((child->getClass() == E4XNode::kProcessingInstruction) && toplevel->xmlClass()->getIgnoreProcessingInstructions()))
					{
						continue;
					}

					E4XNode *cx = child->_deepCopy (core, toplevel);
					cx->setParent (y);
					//y->m_children->push (c);
					y->_append (cx);
				}
			}
		}

		return x;
	}

#if 0
	// E4X 9.1.1.8, page 17
	Atom E4XNode::descendants(Atom P) const
	{
		Multiname m;
		toplevel->ToXMLName (P, m);
		return getDescendants (&m);
	}
#endif

	// E4X 9.1.1.9, page 17
	Atom E4XNode::_equals(AvmCore *core, E4XNode *v) const
	{
		if (this == v)
			return trueAtom;

		if (this->getClass() != v->getClass())
			return falseAtom;

		Multiname m;
		Multiname m2;
		if (this->getQName(core, &m))
		{
			if (v->getQName(core, &m2) == 0)
				return falseAtom;

			// QName/AttributeName comparision here
			if (!m.matches (&m2))
				return falseAtom;
		}
		else if (v->getQName(core, &m2) != 0)
		{
			return falseAtom;
		}

// Not enabled after discussion with JeffD.  If the namespaces are important, they're 
// used in the node names themselves.
#if 0 
		// NOT part of the original spec.  Added in later (bug 144429)
		if (this->numNamespaces() != v->numNamespaces())
			return falseAtom;

		// Order of namespaces does not matter
		AtomArray *ns1 = getNamespaces();
		AtomArray *ns2 = v->getNamespaces();
		for (uint32 n1 = 0; n1 < numNamespaces(); n1++)
		{
			Namespace *namespace1 = core->atomToNamespace (ns1->getAt (n1));
			for (uint32 n2 = 0; n2 < numNamespaces(); n2++)
			{
				Namespace *namespace2 = core->atomToNamespace (ns2->getAt (n2));
				if (namespace1->equalTo (namespace2))
					break;
			}

			// A match was not found
			if (n2 == numNamespaces())
				return falseAtom;
		}
#endif

		if (this->numAttributes() != v->numAttributes())
			return falseAtom;

		if (this->numChildren() != v->numChildren())
			return falseAtom;

		if (this->getValue() != v->getValue() && 
			(this->getValue()==NULL || v->getValue()==NULL || *getValue() != *v->getValue()))
			return falseAtom;

		// step 8
		// for each a in x.attributes
		// if v does not containing matching attribute, return failure
		for (uint32 k1 = 0; k1 < numAttributes(); k1++)
		{
			E4XNode *x1 = getAttribute(k1);
			bool bFoundMatch = false;
			for (uint32 k2 = 0; k2 < v->numAttributes(); k2++)
			{
				if (x1->_equals (core, v->getAttribute(k2)) == trueAtom)
				{
					bFoundMatch = true;
					break;
				}
			}

			if (!bFoundMatch)
				return falseAtom;
		}

		// step 9
		for (uint32 i = 0; i < _length(); i++)
		{
			E4XNode *x1 = _getAt(i);
			E4XNode *x2 = v->_getAt(i);
			if (x1->_equals (core, x2) == falseAtom)
				return falseAtom;
		}

		return trueAtom;
	}

	// E4X 9.1.1.11, page 18
	void E4XNode::_insert (AvmCore* /*core*/, Toplevel* /*toplevel*/, uint32 /*entry*/, Atom /*value*/)
	{
		return;
	}

	void ElementE4XNode::_insert (AvmCore *core, Toplevel *toplevel, uint32 entry, Atom value)
	{
//		//step 1
//		if (m_class & (kText | kCDATA | kComment | kProcessingInstruction | kAttribute))
//			return; 

		// Spec says to throw a typeError if entry is not a number
		// We handle that in callingn functions

		uint32 n = 1;
		XMLListObject *xl = core->atomToXMLList (value);
		if (xl)
		{
			n = xl->_length();
		}
		else
		{
			E4XNode *x = core->atomToXML (value);
			if (x)
			{
				E4XNode *n = this;
				while (n)
				{
					if (x == n)
						toplevel->throwTypeError(kXMLIllegalCyclicalLoop);
					n = n->getParent();
				}

			}
		}

		if (n == 0)
			return; 

		if (!m_children)
		{
			m_children = uintptr(new (core->GetGC()) AtomArray (n));
		}

		if (xl)
		{
			// insert each element of our XMLList into our array
			for (uint32 j = 0; j < xl->_length(); j++)
			{
				E4XNode *child = core->atomToXML (xl->_getAt (j)->atom());

				// !!@ Not in spec but seems like a good idea
				E4XNode *n = this;
				while (n)
				{
					if (child == n)
						toplevel->throwTypeError(kXMLIllegalCyclicalLoop);
					n = n->getParent();
				}

				child->setParent(this);

				insertChild (entry + j, child);
			}
		}
		else
		{
			insertChild (entry, 0); // make room for our replace
			this->_replace (core, toplevel, entry, value);
		}

		return;		
	}

	// E4X 9.1.1.12, page 19
	// Autoconverts V into an XML object 
	E4XNode* E4XNode::_replace (AvmCore* /*core*/, Toplevel* /*toplevel*/, uint32 /*i*/, Atom /*V*/)
	{
		return 0;
	}

	E4XNode* ElementE4XNode::_replace (AvmCore *core, Toplevel *toplevel, uint32 i, Atom V)
	{
		//step 1
		//if (getClass() & (kText | kCDATA | kComment | kProcessingInstruction | kAttribute))
		//	return; 

		// step 2 + 3
		// API throws a typeError if entry is not a number
		// This is always handled back in the caller.

		// step 4
		if (i >= _length())
		{
			i = _length();
			// add a blank spot for this child
			if (!m_children)
				m_children = uintptr(new (core->GetGC()) AtomArray (1));
			convertToAtomArray();
			AtomArray *aa = ((AtomArray *)(uintptr)m_children);
			aa->push (Atom(0));
		}
		
		E4XNode *prior = _getAt(i);

		// step 5
		E4XNode *xml = core->atomToXML (V);
		if (xml && (xml->getClass() & (kElement | kComment | kProcessingInstruction | kText | kCDATA)))
		{
			//a.	If V.[[Class]] is "element" and (V is x or an ancestor of x) throw an Error exception
			if (xml->getClass() == kElement)
			{
				E4XNode *n = this;
				while (n)
				{
					if (xml == n)
						toplevel->throwTypeError(kXMLIllegalCyclicalLoop);
					n = n->getParent();
				}
			}

			xml->setParent (this);
			if ((i >= 0) && (i < this->numChildren()))
			{ 
				if (prior)
				{
					prior->setParent (NULL);
				}
			}

			this->setChildAt (i, xml);
		}
		else if (core->atomToXMLList (V))
		{
			_deleteByIndex (i);
			_insert (core, toplevel, i, V);
		}
		else
		{
			Stringp s = core->string(V);
			E4XNode *newXML = new (core->GetGC()) TextE4XNode(this, s);
			// if this[i] is going away, clear its parent
			if (prior)
			{
				prior->setParent (NULL);
			}

			setChildAt (i, newXML);

			if (XMLObject::notifyNeeded(newXML))
			{
				Atom detail = prior ? prior->getValue()->atom() : 0;
				XMLObject* target = new (core->GetGC()) XMLObject(toplevel->xmlClass(), newXML);
				target->nonChildChanges(toplevel->xmlClass()->kTextSet, newXML->getValue()->atom(), detail);
			}
		}

		return prior;
	}

	void ElementE4XNode::setNotification(AvmCore *core, ScriptObject* f) 
	{ 
		uintptr nameOrAux = m_nameOrAux;
		// We already have an aux structure
		if (AUXBIT & nameOrAux)
		{
			E4XNodeAux *aux = (E4XNodeAux *)(nameOrAux & ~AUXBIT);
			aux->m_notification = f;
		}
		// allocate one to hold our name and notification atom
		else
		{
			Stringp str = (String *)(nameOrAux);
			E4XNodeAux *aux = new (core->GetGC()) E4XNodeAux (str, core->publicNamespace, f);
			//m_nameOrAux = AUXBIT | int(aux);
			WB(core->GetGC(), this, &m_nameOrAux, AUXBIT | uintptr(aux));
		}
	}

	ScriptObject* ElementE4XNode::getNotification() const 
	{ 
		uintptr nameOrAux = m_nameOrAux;
		if (AUXBIT & m_nameOrAux)
		{
			E4XNodeAux *aux = (E4XNodeAux *)(nameOrAux & ~AUXBIT);
			return aux->m_notification;
		}

		return 0; 
	}

}

