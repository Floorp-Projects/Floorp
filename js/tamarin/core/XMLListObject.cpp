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
	XMLListObject::XMLListObject(XMLListClass *type, Atom tObject, const Multiname* tProperty)
		: ScriptObject(type->ivtable(), type->prototype),
		  m_children(0)
	{
		if (tProperty)
			m_targetProperty.setMultiname (*tProperty);
		else // should be equivalent to a null m_targetProperty
			m_targetProperty.setAnyName();
		setTargetObject(tObject);
	}

	XMLListObject::~XMLListObject()
	{
		setTargetObject(0);
	}	

	//////////////////////////////////////////////////////////////////////
	// E4X Section 9.2.1 below (internal methods)
	//////////////////////////////////////////////////////////////////////

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom XMLListObject::callProperty(Multiname* multiname, int argc, Atom* argv)
	{
		AvmCore *core = this->core();

		// sec 11.2.2.1 CallMethod(r,args)

		Atom f = getDelegate()->getMultinameProperty(multiname);
		if (f == undefinedAtom)
		{
			f = getMultinameProperty(multiname);
			// If our method returned is a 0 element XMLList, it means that we did not
			// find a matching property for this method name.  In this case, if our XMLList
			// has one child, we get the child and callproperty on the child object.
			// This allows node elements to be treated as simple strings even if they
			// are XML or XMLList objects.  See 11.2.2.1 in the E4X spec for CallMethod.
			if (core->isXMLList(f) && 
				!core->atomToXMLList(f)->_length() &&
				(this->_length() == 1))
			{
				XMLObject *x0 = _getAt(0);
				return x0->callProperty (multiname, argc, argv);
			}
		}
		argv[0] = atom(); // replace receiver
		return toplevel()->op_call(f, argc, argv);
	}

	Atom XMLListObject::getMultinameProperty(Multiname* m) const
	{
		AvmCore *core = this->core();
		Toplevel* toplevel = this->toplevel();

		// step 1 - We have an integer argument - direct child lookup
		if (!m->isAnyName() && !m->isAttr())
		{
			uint32 index;
			if (!m->isAnyName() && AvmCore::getIndexFromString (m->getName(), &index))
			{
				return getUintProperty (index);
			}
		}

		// step 2
		//QNameObject* qname = new (core->GetGC()) QNameObject(toplevel->qnameClass(), *m);
		XMLListObject *l = new (core->GetGC()) XMLListObject(toplevel->xmlListClass(), this->atom(), m);

		// step 3
		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *xml = _getAt(i);
			if (xml && xml->getClass() == E4XNode::kElement)
			{
				// step 3ai
				Atom gq = xml->getMultinameProperty(m);
				if (core->atomToXML (gq))
				{
					XMLObject *x = core->atomToXMLObject (gq);
					if (x && x->_length())
						l->_append (gq);
				}
				else
				{
					XMLListObject *xl = core->atomToXMLList (gq);
					if (xl && xl->_length())
					{
						l->_append (gq);
					}
				}
			}
		}

		return l->atom();
	}

	void XMLListObject::setMultinameProperty(Multiname* m, Atom V)
	{
		AvmCore *core = this->core();
		Toplevel *toplevel = this->toplevel();

		uint32 i;
		if (!m->isAnyName() && !m->isAttr() && AvmCore::getIndexFromString (m->getName(), &i))
		{
			setUintProperty (i, V);
			return;
		}
		// step 3
		else if (_length() <= 1)
		{
			//step3a
			if (_length() == 0)
			{
				Atom r = this->_resolveValue();
				if (AvmCore::isNull(r))
					return;

				if (core->isXML (r) && (core->atomToXML (r)->_length() != 1))
					return;

				if (core->isXMLList (r) && (core->atomToXMLList (r)->_length() != 1))
					return;

				this->_append (r);
			}

			XMLObject *x = _getAt (0);
			x->setMultinameProperty(m, V);		
		}
		else if (_length() > 1)
		{
			toplevel->throwTypeError(kXMLAssigmentOneItemLists);
		}

#ifdef _DEBUG
		// The only thing allowed in XMLLists is XML objects
		for (uint32 z = 0; z < _length(); z++)
		{
			AvmAssert(core->isXML(m_children.getAt(z)));
		}
#endif
				
		return;
	}

	bool XMLListObject::deleteMultinameProperty(Multiname* m)
	{
		if (!m->isAnyName() && !m->isAttr())
		{
			Stringp name = m->getName();
			uint32 index;
			if (AvmCore::getIndexFromString (name, &index))
			{
				return this->delUintProperty (index);
			}
		}

		for (uint32 i = 0; i < numChildren(); i++)
		{
			XMLObject *xm = _getAt(i);
			if (xm->getClass() == E4XNode::kElement)
			{
				xm->deleteMultinameProperty(m);
			}
		}

		return true;
	}

	Atom XMLListObject::getDescendants(Multiname* m) const
	{
		AvmCore *core = this->core();

		XMLListObject *l = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass());
		for (uint32 i = 0; i < numChildren(); i++)
		{
			XMLObject *xm = _getAt(i);
			if (xm->getClass() == E4XNode::kElement)
			{
				Atom dq = xm->getDescendants (m);
				XMLListObject *dqx = core->atomToXMLList (dq);
				if (dqx && dqx->_length())
				{
					l->_append (dq);
				}
			}
		}

		return l->atom();
	}

   	// E4X 9.2.1.1, page 21
	Atom XMLListObject::getAtomProperty(Atom P) const
	{
		Multiname m;
		toplevel()->ToXMLName (P, m);
		return getMultinameProperty(&m);
	}

	// E4X 9.2.1.2, pg 21 - [[PUT]]
	void XMLListObject::setAtomProperty(Atom P, Atom V)
	{
		Multiname m;
		toplevel()->ToXMLName (P, m);
		setMultinameProperty(&m, V);
	}

	Atom XMLListObject::getUintProperty(uint32 index) const
	{
		if ((index >= 0) && (index < _length()))
		{
			return _getAt(index)->atom();
		}
		else
		{
			return undefinedAtom;
		}
	}

	void XMLListObject::setUintProperty(uint32 i, Atom V)
	{
		AvmCore* core = this->core();
		Toplevel* toplevel = this->toplevel();

		Atom r = nullStringAtom;
		// step 2a
		if (!AvmCore::isNull(this->m_targetObject))
		{
			if (core->isXML(m_targetObject))
			{
				r = core->atomToXMLObject (m_targetObject)->_resolveValue();
			}
			else if (core->isXMLList (m_targetObject))
			{
				r = core->atomToXMLList (m_targetObject)->_resolveValue();
			}

			if (AvmCore::isNull(r))
				return;
		}

		// step 2c
		if (i >= _length())
		{
			// step 2 c i
			if (core->isXMLList (r))
			{
				XMLListObject *xl = core->atomToXMLList (r);
				if (xl->_length() != 1)
					return;
				else
					r = xl->_getAt(0)->atom();

				// r should now be an XMLObject atom from this point forward
			}

			// step 2cii - newer spec
			XMLObject *rx = core->atomToXMLObject (r);
			if (rx && rx->getClass() != E4XNode::kElement)
				return;

			E4XNode *y = 0;
			// step 2 c iii
			if (m_targetProperty.isAttr())
			{
				Atom attributesExist = rx->getMultinameProperty(m_targetProperty);
				if (core->isXMLList (attributesExist) && (core->atomToXMLList (attributesExist)->_length() > 0))
					return;

				y = new (core->GetGC()) AttributeE4XNode (core->atomToXML (r), 0);
				y->setQName (core, m_targetProperty);

				// warning: it looks like this is sent prior to the attribute being added.
				rx->nonChildChanges(xmlClass()->kAttrAdded, this->m_targetProperty.getName()->atom());
			}
			// if targetProperty is null or targetProperty.localName = "*"
			else if (m_targetProperty.isAnyName() ||
				// Added because of bug 145184 - appendChild with a text node not working right
				(core->isXML(V) && core->atomToXML(V)->getClass() == E4XNode::kText))
			{
				y = new (core->GetGC()) TextE4XNode (core->atomToXML(r), 0);
			}
			else
			{
				y = new (core->GetGC()) ElementE4XNode (core->atomToXML (r));
				y->setQName (core, this->m_targetProperty);
			}

			i = _length();
			// step 2.c.viii
			if (y->getClass() != E4XNode::kAttribute)
			{
				AvmAssert(y->getParent() == (rx ? rx->getNode() : 0));
				if (y->getParent() != NULL)
				{
					// y->m_parent = r = XMLObject * 
					E4XNode *parent = y->getParent();
					AvmAssert(parent != 0); 
					uint32 j = 0;
					if (i > 0)
					{
						// step 2 c vii 1 ii 
						// spec says "y.[[parent]][j] is not the same object as x[i-1]" and I'm
						// assuming that means pointer equality and not "equals" comparison
						while ((j < (parent->_length() - 1)) && parent->_getAt (j) != _getAt(i - 1)->getNode())
						{
							j++;
						}

						j = j + 1; // slightly different than spec but our insert call does not have a + 1 on it
					}
					else
					{
						j = parent->_length(); // slightly different than spec but our insert call does not have a + 1 on it
					}
					// These two lines are equivalent to _insert which takes
					// an Atom since we know that y is an XMLObject/E4XNode
					y->setParent (parent);
					parent->insertChild (j, y);

				}
				if (core->isXML(V))
				{
					Multiname mV;
					if (core->atomToXML (V)->getQName(core, &mV))
						y->setQName (core, &mV);

				}
				else if (core->isXMLList (V))
				{
					//ERRATA : 9.2.1.2 step 2(c)(vii)(3) what is V.[[PropertyName]]? s.b. [[TargetProperty]]
					if (!core->atomToXMLList (V)->m_targetProperty.isAnyName())
						y->setQName (core, core->atomToXMLList (V)->m_targetProperty); 
				}
			}

			this->_append (y);
		}

		// step 2d
		E4XNode *v = core->atomToXML (V);
		if ((!core->atomToXMLList(V) && !v) || (v && (v->getClass() & (E4XNode::kText | E4XNode::kCDATA | E4XNode::kAttribute))))
		{
			// This string is converted into a XML object below in step 2(g)(iii)
			V = core->string(V)->atom();
		}

		// step 2e
		XMLObject *xi = _getAt(i);
		if (xi->getClass() == E4XNode::kAttribute)
		{
			XMLObject *parent = xi->getParent();
			Multiname mxi;
			xi->getQName (&mxi);
			parent->setMultinameProperty(&mxi, V);
			Atom attr = parent->getMultinameProperty(&mxi);
			XMLListObject *attrx = core->atomToXMLList (attr);
			// x[i] = attr[0]; 
			m_children.setAt (i, attrx->_getAt(0) ->atom());
		}
		// step 2f
		else if (core->isXMLList (V))
		{
			// create a shallow copy c of V
			XMLListObject *src = core->atomToXMLList (V);
			XMLListObject *c = new (core->GetGC()) XMLListObject(toplevel->xmlListClass());

			//c->m_children = new (core->GetGC()) AtomArray (src->numChildren());
			c->m_children.checkCapacity(src->numChildren());
			for (uint32 i2 = 0; i2 < src->numChildren(); i2++)
				c->m_children.push (src->m_children.getAt(i2));

			E4XNode *parent = _getAt(i)->getNode()->getParent();
			// step 2 f iii
			if (parent)
			{
				uint32 q = 0;
				while (q < parent->numChildren())
				{
					if (parent->_getAt(q) == _getAt(i)->getNode())
					{
						parent->_replace (core, toplevel, q, c->atom());
						for (uint32 j = 0; j < c->_length(); j++)
						{
							XMLObject *xo = new (core->GetGC()) XMLObject(toplevel->xmlClass(), parent->_getAt(q + j));
							c->m_children.setAt (j, xo->atom());
						}

						break;
					}

					q++;
				}
			}

			// resize our buffer to ([[Length]] + c.[[Length]]) (2 f vi)

			bool notify = (parent && XMLObject::notifyNeeded(parent));
			XMLObject* prior =  (notify) ? _getAt(i) : 0;
			XMLObject* target = (notify) ? (new (core->GetGC())  XMLObject (toplevel->xmlClass(), parent)) : 0;

			m_children.removeAt (i);
			for (uint32 i2 = 0; i2 < src->numChildren(); i2++)
			{
				m_children.insert (i + i2, c->m_children.getAt(i2));

				XMLObject* obj = c->_getAt(i2);
                if (notify && (parent == obj->getNode()->getParent()))
                {
                    if (i2 == 0)
                    {
                        // @todo; is this condition ever true?
                        if (obj->getNode() != prior->getNode())
                            target->childChanges( xmlClass()->kNodeChanged, obj->atom(), prior->getNode());
                    }
                    else
                    {
                        target->childChanges( xmlClass()->kNodeAdded, obj->atom());
                    }
                }
			}
		}
		// step 2g
		// !!@ always going into this step supports how Rhino acts with setting XMLList props
		//var y = new XMLList("<alpha>one</alpha><bravo>two</bravo>");
		//y[0] = "five";
		//print(y);
		// <alpha>one</alpha> which is an element node is converted to a text node with "five" as it's label
		// !!@ but in this case, the node's contents are replaced and not the node itself...
		// o = <o><i id="1">A</i><i id="2">B</i><i id="3">C</i><i id="4">D</i></o>;
		// o.i[1] = "BB";
		// print(o);
		else if (core->isXML (V) || xi->getClass() & (E4XNode::kText | E4XNode::kCDATA | E4XNode::kComment | E4XNode::kProcessingInstruction))
		{
			E4XNode *parent = _getAt(i)->getNode()->getParent();
			if (parent)
			{
				// search our parent for a match between a child and x[i]
				uint32 q = 0;
				while (q < parent->numChildren())
				{
					if (parent->_getAt(q) == _getAt(i)->getNode())
					{
						parent->_replace (core, toplevel, q, V);
						XMLObject *xo = new (core->GetGC()) XMLObject (toplevel->xmlClass(), parent->_getAt(q));
						V = xo->atom();

						if (XMLObject::notifyNeeded(parent))
						{
							XMLObject *po = new (core->GetGC()) XMLObject (toplevel->xmlClass(), parent);
							po->childChanges(xmlClass()->kNodeAdded, xo->atom());
						}
						break;
					}

					q++;
				}
			}

			// From E4X errrata
			//9.2.1.2 step 2(g)(iii) erratum: _V_ may not be of type XML, but all index-named
			//		properties _x[i]_ in an XMLList _x_ must be of type XML, according to
			//		9.2.1.1 Overview and other places in the spec.

			//		Thanks to 2(d), we know _V_ (*vp here) is either a string or an
			//		XML/XMLList object.  If _V_ is a string, call ToXML on it to satisfy
			//		the constraint before setting _x[i] = V_.

			if (core->isXML(V))
				m_children.setAt (i, V);
			else
				m_children.setAt (i, xmlClass()->ToXML (V));
		}
		// step 2h
		else
		{
			_getAt(i)->setStringProperty(core->kAsterisk, V);
		}
	}

	bool XMLListObject::delUintProperty(uint32 index)
	{
		if (index >= _length())
		{
			return true; 
		}
		else
		{
			XMLObject *xi = _getAt(index);
			XMLObject *px = xi->getParent();
			if (px != NULL)
			{
				if (xi->getClass() == E4XNode::kAttribute)
				{
					Multiname mx;
					xi->getQName (&mx);
					px->deleteMultinameProperty(&mx);
				}
				else
				{
					// let q be the property of parent, where parent[q] is the same object as x[i].
					int q = xi->childIndex ();

					E4XNode *x = px->getNode()->_getAt(q);
					px->getNode()->_deleteByIndex (q);
					
					if (XMLObject::notifyNeeded(px->getNode()) && x->getClass() == E4XNode::kElement)
					{
						AvmCore *core = this->core();
						XMLObject *r = new (core->GetGC())  XMLObject (xmlClass(), x);
						px->childChanges(xmlClass()->kNodeRemoved, r->atom());
					}
				}
			}

			// delete index from this list

			m_children.removeAt (index);
			return true; 
		}
	}

	// E4X 9.2.1.3, pg 23
	bool XMLListObject::deleteAtomProperty(Atom P)
	{
		Multiname m;
		toplevel()->ToXMLName (P, m);
		return deleteMultinameProperty(&m);
	}

	bool XMLListObject::hasUintProperty(uint32 index) const
	{
		return (index < _length());
	}

	bool XMLListObject::hasMultinameProperty(Multiname* m) const
	{
		if (!m->isAnyName() && !m->isAttr())
		{
			Stringp name = m->getName();
			uint32 index;
			if (AvmCore::getIndexFromString (name, &index))
			{
				return (index < _length());
			}
		}

		for (uint32 i = 0; i < numChildren(); i++)
		{
			XMLObject *xm = _getAt(i);
			if (xm->getClass() == E4XNode::kElement)
			{
				if (xm->hasMultinameProperty(m))
					return true;
			}
		}

		return false;
	}

	// E4X 9.2.1.5, pg 24
	bool XMLListObject::hasAtomProperty(Atom P) const
	{
		Multiname m;
		toplevel()->ToXMLName (P, m);
		return hasMultinameProperty(&m);
	}

	void XMLListObject::_append(E4XNode *v)
	{
		AvmCore *core = this->core();
		XMLObject *x = new (core->GetGC()) XMLObject (toplevel()->xmlClass(), v);

		if (v->getParent())
		{
			XMLObject *p = new (core->GetGC()) XMLObject (toplevel()->xmlClass(), v->getParent());
			setTargetObject(p->atom());
		}
		else
			setTargetObject(nullObjectAtom);
		if (v->getClass() != E4XNode::kProcessingInstruction)
		{
			Multiname m;
			if (v->getQName (core, &m))
				this->m_targetProperty.setMultiname(m);
		}

		m_children.push (x->atom());
	}

	// E4X 9.2.1.6, pg 24
	void XMLListObject::_append(Atom V)
	{
		AvmCore *core = this->core();

		// !!@ what the docs say
		// Atom children = this->get ("*");
		// children->put (children->length(), child)
		// return x;

		if (core->isXMLList(V))
		{
			XMLListObject *v = core->atomToXMLList(V);
			setTargetObject(v->m_targetObject);
			this->m_targetProperty = v->m_targetProperty;

			if (v->_length())
			{
				m_children.checkCapacity(v->_length());
				for (uint32 j = 0; j < v->_length(); j++)
				{
					m_children.push (v->_getAt(j)->atom());
				}
			}
		}
		else if (core->isXML(V))
		{
			E4XNode *v = core->atomToXML (V);
			_append (v);
		}
	}

	// E4X 9.2.1.7, pg 25
	XMLListObject *XMLListObject::_deepCopy () const
	{
		AvmCore *core = this->core();

		XMLListObject *l = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass(), m_targetObject, &m_targetProperty.getMultiname());

		l->m_children.checkCapacity(numChildren());
		for (uint32 i = 0; i < numChildren(); i++)
		{
			l->m_children.push (_getAt(i)->_deepCopy()->atom());
		}

		return l; 
	}

	// E4X 9.2.1.8, pg 25
	XMLListObject *XMLListObject::descendants (Atom P)
	{
		Multiname m;
		toplevel()->ToXMLName (P, m);
		return core()->atomToXMLList (getDescendants (&m));
	}

	// E4X 9.2.1.9, pg 26
	Atom XMLListObject::_equals(Atom V) const
	{
		AvmCore *core = this->core();

		// null or "" return false
		if ((V == undefinedAtom) && (_length() == 0))
			return trueAtom;

		if (core->isXMLList(V))
		{
			XMLListObject *v = core->atomToXMLList (V);
			if (_length() != v->_length())
				return falseAtom;

			for (uint32 i = 0; i < _length(); i++)
			{
				if (core->eq (m_children.getAt(i), v->m_children.getAt(i)) == falseAtom)
					return falseAtom;
			}			

			return trueAtom;
		}
		else if (_length() == 1)
		{
			return core->eq (m_children.getAt(0), V);
		}

		return falseAtom;
	}

	// E4X 9.2.1.10, pg 26
	Atom XMLListObject::_resolveValue()
	{
		if (_length() > 0)
			return this->atom();

		AvmCore *core = this->core();

		if (AvmCore::isNull(m_targetObject) || 
			(m_targetProperty.isAttr()) || 
			(m_targetProperty.isAnyName()))
		{
			return nullObjectAtom;
		}

		Atom base = nullObjectAtom;
		XMLObject *x = core->atomToXMLObject (m_targetObject);
		if (x)
		{
			base = x->_resolveValue();
		}
		XMLListObject *xl = core->atomToXMLList (m_targetObject);
		if (xl)
		{
			base = xl->_resolveValue ();
		}
		if (AvmCore::isNull(base))
			return nullObjectAtom;

		XMLListObject *target = 0;
		if (core->isXML(base))
		{
			target = core->atomToXMLList(core->atomToXMLObject(base)->getMultinameProperty(m_targetProperty));
		}
		else if (core->isXMLList(base))
		{
			target = core->atomToXMLList(core->atomToXMLList(base)->getMultinameProperty(m_targetProperty));
		}
		else
		{
			AvmAssert(0);// base should be an XML or XMLList always
		}

		if (!target)
			return nullObjectAtom;

		if (!target->_length())
		{
			if (core->isXMLList (base) && core->atomToXMLList(base)->_length() > 1)
			{
				toplevel()->throwTypeError(kXMLAssigmentOneItemLists);
				return nullObjectAtom;
			}

			if (core->isXML(base))
			{
				core->atomToXMLObject(base)->setMultinameProperty(m_targetProperty, core->kEmptyString->atom());
				return core->atomToXMLObject(base)->getMultinameProperty(m_targetProperty);
			}
			else if (core->isXMLList(base))
			{
				core->atomToXMLList(base)->setMultinameProperty(m_targetProperty, core->kEmptyString->atom());
				return core->atomToXMLList(base)->getMultinameProperty(m_targetProperty);
			}
			else
			{
				AvmAssert(0); // b should be one or the other at this point since target is non-null
			}
		}

		return target->atom();
	}

	XMLObject *XMLListObject::_getAt (uint32 i) const
	{
		AvmCore *core = this->core();

		if ((i < 0) || (i >= _length()))
			return 0;

		return core->atomToXMLObject (m_children.getAt(i));
	}

	// E4X 12.2, page 59
	// Support for for-in, for-each for XMLListObjects
	Atom XMLListObject::nextName(int index)
	{
		AvmAssert(index > 0);

		if (index <= (int)_length())
		{
			AvmCore *core = this->core();
			return core->internInt (index-1)->atom();
		}
		else
		{
			return nullStringAtom;
		}
	}

	Atom XMLListObject::nextValue(int index)
	{
		AvmAssert(index > 0);

		if (index <= (int)_length())
            return _getAt (index-1)->atom();
		else
			return undefinedAtom;
	}

	int XMLListObject::nextNameIndex(int index)
	{
		AvmAssert(index >= 0);

		// XMLList types return the same number of indexes as children
		// We return a 1-N iterator (and then subtract off the one in 
		// the nextName and nextValue functions).
		if (index < (int) _length())
			return index + 1;
		else
			return 0;
	}

	// E4X 13.5.4.21, page 93 which just refers to 10.2, page 31 (XMLList case)
	void XMLListObject::__toXMLString(StringBuffer &output, Atom, int /*indentLevel*/)
	{
		AvmCore *core = this->core();

		// For i = 0 to x.[[length]-1
		//	if (xml.prettyprinting == true and i is not equal to 0)
		//		s = s + line terminator
		//  s = s + toxmlstring(x[i], ancestor namespace)

		for (uint32 i = 0; i < _length(); i++)
		{
			// iterate over entire array. If any prop is an "element" type, return false
			XMLObject *xm = _getAt(i);
			if (xm) 
			{
				// !!@ This does not seem be affected by the prettyPrinting flag
				// but always occurs (comparing to Rhino)
				if (/*xmlClass()->getPrettyPrinting() && */i)
					output << "\n";	
					AtomArray *AncestorNamespaces = new (core->GetGC()) AtomArray();
					xm->__toXMLString(output, AncestorNamespaces, 0);
			}
		}

		return;
	}	

	//////////////////////////////////////////////////////////////////////
	// E4X Section 13.5.4 below (AS methods)
	//////////////////////////////////////////////////////////////////////

	// E4X 13.5.4.2, pg 88
	XMLListObject *XMLListObject::attribute (Atom arg)
	{
		// name= ToAttributeName (attributeName);
		// return [[get]](name)

		return core()->atomToXMLList(getAtomProperty(toplevel()->ToAttributeName(arg)->atom()));
	}

	// E4X 13.5.4.3, pg 88
	XMLListObject *XMLListObject::attributes ()
	{
		return core()->atomToXMLList(getAtomProperty(toplevel()->ToAttributeName(core()->kAsterisk)->atom()));
	}

	// E4X 13.5.4.4, pg 88
	XMLListObject *XMLListObject::child (Atom propertyName)
	{
		AvmCore *core = this->core();

		XMLListObject *m = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass(), this->atom());

		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *x = _getAt(i);
			XMLListObject *rxl = x->child (propertyName);
			if (rxl && rxl->_length())
			{
				m->_append (rxl->atom());
			}
		}
		
		return m;
	}

	XMLListObject *XMLListObject::children ()
	{
		return core()->atomToXMLList(getStringProperty(core()->kAsterisk));
	}

	XMLListObject *XMLListObject::comments ()
	{
		AvmCore *core = this->core();

		XMLListObject *m = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass(), this->atom());

		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *x = _getAt(i);
			if (x->getClass() == E4XNode::kElement)
			{
				XMLListObject *rxl = x->comments();
				if (rxl && rxl->_length())
				{
					m->_append (rxl->atom());
				}
			}
		}
		
		return m;
	}

	bool XMLListObject::contains (Atom value)
	{
		AvmCore *core = this->core();;
		for (uint32 i = 0; i < _length(); i++)
		{
			// Spec says "comparison l[i] == value)" which is different than _equals
			if (core->eq (m_children.getAt(i), value) == trueAtom)
				return true;
		}

		return false;
	}

	XMLListObject *XMLListObject::copy ()
	{
		return _deepCopy();
	}

	// E4X 13.5.4.10, pg 90
	XMLListObject *XMLListObject::elements (Atom name) // name defaults to '*'
	{
		AvmCore *core = this->core();
		Toplevel* toplevel = this->toplevel();

		Multiname m;
		toplevel->ToXMLName (name, m);

		XMLListObject *xl = new (core->GetGC()) XMLListObject(toplevel->xmlListClass(), this->atom(), &m);

		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *x = _getAt(i);
			if (x->getClass() == E4XNode::kElement)
			{
				XMLListObject *rxl = x->elements(name);
				if (rxl && rxl->_length())
				{
					xl->_append (rxl->atom());
				}
			}
		}

		return xl;
	}

	// E4X 13.5.4.11, pg 90
	bool XMLListObject::hasOwnProperty (Atom P)
	{
		if (hasAtomProperty(P))
			return true;

		// If x has a property with name ToString(P), return true;
		// !!@ (prototype different than regular object??

		return false;
	}

	// E4X 13.5.4.12, pg 90
	bool XMLListObject::hasComplexContent ()
	{
		if (_length() == 0)
			return false;

		if (_length() == 1)
		{
			XMLObject *x = _getAt(0);
			return x->hasComplexContent();
		}

		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *x = _getAt(i);
			if (x->getClass() == E4XNode::kElement)
			{
				return true;
			}
		}

		return false;
	}

	// E4X 13.5.4.13, pg 91
	bool XMLListObject::hasSimpleContent ()
	{
		if (!_length())
		{
			return true;
		}
		else if (_length() == 1)
		{
			XMLObject *xm = _getAt(0);
			if (xm)
				return xm->hasSimpleContent();
			else
			{
				AvmAssert(0);
				return false;
			}
		}
		else
		{
			for (uint32 i = 0; i < _length(); i++)
			{
				XMLObject *xm = _getAt(i);
				AvmAssert(xm != 0);
				if (xm && xm->getClass() == E4XNode::kElement)
				{
					return false;
				}
			}
		}

		return true;
	}

	uint32 XMLListObject::AS_based_length () const
	{
		return _length();
	}

	XMLListObject *XMLListObject::normalize ()
	{
		AvmCore *core = this->core();

		uint32 i = 0;
		while (i < _length())
		{
			XMLObject *x = _getAt(i);
			if (x->getClass() == E4XNode::kElement)
			{
				x->normalize();
				i++;
			}
			else if ((x->getClass() & (E4XNode::kText | E4XNode::kCDATA)))
			{
				while (((i + 1) < _length()) && ((_getAt(i + 1)->getNode()->getClass() & (E4XNode::kText | E4XNode::kCDATA))))
				{
					x->setValue (core->concatStrings(x->getNode()->getValue(), _getAt(i + 1)->getNode()->getValue()));
					deleteAtomProperty(core->intToAtom(i + 1));
				}
				if (x->getValue()->length() == 0)
				{
					deleteAtomProperty(core->intToAtom(i));
				}
				else
				{
					i++;
				}
			}
			else
			{
				i++;
			}
		}

		return this;
	}

	Atom XMLListObject::parent ()
	{
		if (!_length())
			return undefinedAtom;

		E4XNode *parent = _getAt(0)->getNode()->getParent();
		for (uint32 i = 1; i < _length(); i++)
		{
			E4XNode *p = _getAt(i)->getNode()->getParent();
			if (parent != p)
				return undefinedAtom;
		}

		if (parent)
			return _getAt(0)->getParent()->atom();
		else
			return undefinedAtom;
	}

	XMLListObject *XMLListObject::processingInstructions (Atom name) // name defaults to '*'
	{
		AvmCore *core = this->core();
		XMLListObject *m = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass(), this->atom());

		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *x = _getAt(i);
			if (x->getClass() == E4XNode::kElement)
			{
				XMLListObject *rxl = x->processingInstructions(name);
				if (rxl && rxl->_length())
				{
					m->_append (rxl->atom());
				}
			}
		}
		
		return m;
	}

	bool XMLListObject::xmlListPropertyIsEnumerable(Atom P) // NOT virtual, NOT an override
	{
		AvmCore *core = this->core();
		double index = core->number(P);
		if ((index >= 0.0) && (index < _length()))
			return true;

		return false;
	}

	XMLListObject *XMLListObject::text ()
	{
		AvmCore *core = this->core();

		XMLListObject *m = new (core->GetGC()) XMLListObject(toplevel()->xmlListClass(), this->atom());

		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *x = _getAt(i);
			if (x->getClass() == E4XNode::kElement)
			{
				XMLListObject *rxl = x->text();
				if (rxl && rxl->_length())
				{
					m->_append (rxl->atom());
				}
			}
		}
		
		return m;
	}

	// E4X 10.1.2, page 28
	Atom XMLListObject::toString()
	{
		AvmCore *core = this->core();
		if (hasSimpleContent ())
		{
			Stringp output = core->kEmptyString;

			// s is the empty string
			// for all props in children array
			// if (class != comment or processing instruction)
			// s = s + toString(prop)
			for (uint32 i = 0; i < _length(); i++)
			{
				XMLObject *xm = _getAt(i);
				if (xm && (xm->getClass() != E4XNode::kComment) && (xm->getClass() != E4XNode::kProcessingInstruction))
				{
					output = core->concatStrings(output, core->string (xm->toString()));
				}
			}

			return output->atom();
		}
		else
		{
			StringBuffer output(core);
			this->__toXMLString(output, nullStringAtom, 0);
			return core->newString(output.c_str())->atom();
		}
	}

	Stringp XMLListObject::toStringMethod()
	{
		// This is a non-virtual version of toString.
		// This method is needed because pointer->method in Codewarrior
		// is different depending on wheher the method is virtual or not,
		// causing problems with NATIVE_METHOD.
        return core()->atomToString(toString());
	}
	
	String *XMLListObject::toXMLString ()
	{
		StringBuffer output(core());
		this->__toXMLString(output, nullStringAtom, 0);
		return core()->newString(output.c_str());
	}

	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////
	// Routines below are not in the spec but work if XMLList has one element
	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////

	XMLObject *XMLListObject::addNamespace (Atom _namespace)
	{
		if (_length() == 1)
		{
			return _getAt(0)->addNamespace(_namespace);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("addNamespace"));
			return 0;
		}
	}

	XMLObject *XMLListObject::appendChild (Atom child)
	{
		if (_length() == 1)
		{
			return _getAt(0)->appendChild (child);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("appendChild"));
			return 0;
		}
	}

	int XMLListObject::childIndex ()
	{
		if (_length() == 1)
		{
			return _getAt(0)->childIndex ();
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("childIndex"));
			return -1;
		}
	}

	ArrayObject *XMLListObject::inScopeNamespaces ()
	{
		if (_length() == 1)
		{
			return _getAt(0)->inScopeNamespaces();
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("inScopeNamespaces"));
			return 0;
		}
	}

	Atom XMLListObject::insertChildAfter (Atom child1, Atom child2)
	{
		if (_length() == 1)
		{
			return _getAt(0)->insertChildAfter (child1, child2);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("insertChildAfter"));
			return undefinedAtom;
		}
	}

	Atom XMLListObject::insertChildBefore (Atom child1, Atom child2)
	{
		if (_length() == 1)
		{
			return _getAt(0)->insertChildBefore (child1, child2);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("insertChildBefore"));
			return undefinedAtom;
		}
	}

	Atom XMLListObject::name() 
	{
		if (_length() == 1)
		{
			return _getAt(0)->name();
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("name"));
			return nullStringAtom;
		}
	}


	Atom XMLListObject::getNamespace (Atom *argv, int argc) // prefix is optional
	{
		if (_length() == 1)
		{
			return _getAt(0)->getNamespace(argv, argc);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("namespace"));
			return nullStringAtom;
		}
	}

	Atom XMLListObject::localName ()
	{
		if (_length() == 1)
		{
			return _getAt(0)->localName();
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("localName"));
			return nullStringAtom;
		}
	}

	ArrayObject *XMLListObject::namespaceDeclarations ()
	{
		if (_length() == 1)
		{
			return _getAt(0)->namespaceDeclarations();
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("namespaceDeclarations"));
			return 0;
		}
	}

	String *XMLListObject::nodeKind ()
	{
		// if our list has one element, return the nodeKind of the first element
		if (_length() == 1)
		{
			return _getAt(0)->nodeKind();
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("nodeKind"));
			return 0;
		}
	}

	XMLObject *XMLListObject::prependChild (Atom value)
	{
		if (_length() == 1)
		{
			return _getAt(0)->prependChild(value);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("prependChild"));
			return 0;
		}
	}

	XMLObject *XMLListObject::removeNamespace (Atom _namespace)
	{
		if (_length() == 1)
		{
			return _getAt(0)->removeNamespace (_namespace);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("removeNamespace"));
			return 0;
		}
	}

	XMLObject *XMLListObject::replace (Atom propertyName, Atom value)
	{
		if (_length() == 1)
		{
			return _getAt(0)->replace(propertyName, value);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("replace"));
			return 0;
		}
	}

	XMLObject *XMLListObject::setChildren (Atom value)
	{
		if (_length() == 1)
		{
			return _getAt(0)->setChildren (value);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("setChildren"));
			return 0;
		}
	}

	void XMLListObject::setLocalName (Atom name)
	{
		if (_length() == 1)
		{
			_getAt(0)->setLocalName (name);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("setLocalName"));
		}
	}

	void XMLListObject::setName (Atom name)
	{
		if (_length() == 1)
		{
			_getAt(0)->setName(name);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("setName"));
		}
	}

	void XMLListObject::setNamespace (Atom ns)
	{
		if (_length() == 1)
		{
			_getAt(0)->setNamespace (ns);
		}
		else
		{
			// throw type error
			toplevel()->throwTypeError(kXMLOnlyWorksWithOneItemLists, core()->toErrorString("setNamespace"));
		}
	}

#ifdef XML_FILTER_EXPERIMENT
	XMLListObject * XMLListObject::filter (Atom propertyName, Atom value)
	{
		Multiname m;
		toplevel()->ToXMLName(propertyName, m);

		Multiname name;
		toplevel()->CoerceE4XMultiname(&m, name);

		// filter opcode experiment
		XMLListObject *l = new (core()->gc) XMLListObject(toplevel()->xmlListClass());
		for (uint32 i = 0; i < _length(); i++)
		{
			XMLObject *xm = _getAt(i);
			xm->_filter (l, name, value);
		}

		return l;
	}
#endif // XML_FILTER_EXPERIMENT

	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////

	// reserved for now
	void XMLListObject::setNotification(ScriptObject* /*f*/)
	{
		// nop
	}

	ScriptObject* XMLListObject::getNotification()
	{
		return 0;
	}

}
