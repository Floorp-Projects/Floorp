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

 #ifndef __avmplus_XMLListObject__
#define __avmplus_XMLListObject__


// E4X 9.1.1 Internal Properties and Methods
// Name
// Parent
// Attributes
// InScopeNamespaces
// Length
// Delete (overrides Object version)
// Get (overrides Object version)
// HasProperty (overrides Object version)
// Put (overrides Object version)
// DeleteByIndex (PropertyName)
// DeepCopy
// ResolveValue
// Descendants (PropertyName)
// Equals (Value)
// Insert (PropertyName, Value)
// Replace (PropertyName, Value)
// AddInScopeNamespace (Namespace)

// XMLList.prototype.constructor ??
// XMLList.prototype.attribute
// XMLList.prototype.attributes
// XMLList.prototype.child
// XMLList.prototype.children
// XMLList.prototype.comments
// XMLList.prototype.contains
// XMLList.prototype.copy
// XMLList.prototype.descendants
// XMLList.prototype.elements
// XMLList.prototype.hasOwnProperty
// XMLList.prototype.hasComplexContent
// XMLList.prototype.hasSimpleContent
// XMLList.prototype.length
// XMLList.prototype.normalize
// XMLList.prototype.parent
// XMLList.prototype.processingInstructions
// XMLList.prototype.prependChild
// XMLList.prototype.propertyIsEnumerable
// XMLList.prototype.text
// XMLList.prototype.toString
// XMLList.prototype.toXMLString
// XMLList.prototype.valueOf


namespace avmplus
{
	/**
	 * The XMLListObject class is the C++ implementation of the
	 * "XMLList" type in the E4X Specification.
	 */
	class XMLListObject : public ScriptObject
	{
		XMLClass* xmlClass() const
		{
			return toplevel()->xmlClass();
		}

	private:
		HeapMultiname m_targetProperty;
		Atom m_targetObject;

		// An array of XMLObjects (NOT E4XNodes)
		AtomArray m_children;
		friend class XMLObject;

		void setTargetObject(Atom a) { 
			WBATOM(MMgc::GC::GetGC(this), this, &m_targetObject, a); 
		}

	public:
		// Functions that override object version
		// Delete (deleteProperty)
		// Get (getProperty)
		// HasProperty 
		// Put (setProperty)
		Atom callProperty(Multiname* name, int argc, Atom* argv);

		Atom getProperty(Atom name) const;			// [[Get]]
		void setProperty(Atom name, Atom value);	// [[Put]]
		bool deleteProperty(Atom name);				// [[Delete]

		Atom getProperty(Stringp name) const
		{
			AvmAssert(name != NULL && name->isInterned());
			return getProperty(name->atom());
		}

		Atom getProperty(Multiname* name) const;
		void setProperty(Multiname* name, Atom value);
		bool deleteProperty(Multiname* name);

		bool hasProperty(Multiname* name) const;
		bool hasProperty(Atom name) const;
		bool hasUintProperty(uint32 i) const;

		Atom getDescendants(Multiname* name) const;

		Atom getUintProperty(uint32 i) const;
		void setUintProperty(uint32 i, Atom value);
		bool delUintProperty(uint32 i);

		// private helper functions
		void _append (E4XNode *node);				// [[Append]]
		void _append (Atom child);					// [[Append]]
		XMLListObject *_deepCopy () const;			// [[DeepCopy]]
		Atom _equals(Atom V) const;					// [[Equals]]
		Atom _resolveValue ();						// [[ResolveValue]

		uint32 numChildren()   const { return m_children.getLength(); };
		uint32 _length() const { return (numChildren()); }; //[[Length]]

		XMLObject *_getAt (uint32 i) const;

		void __toXMLString(StringBuffer &output, Atom AncestorNamespace, int indentLevel = 0);

		// Iterator support - for in, for each
		Atom nextName(int index);
		Atom nextValue(int index);
		int nextNameIndex(int index);

		// Exposed routines to AS (NATIVE_METHODs)
		XMLListObject *attribute (Atom arg);
		XMLListObject *attributes ();
		XMLListObject *child (Atom propertyName);
		XMLListObject *children ();
		XMLListObject *comments ();
		bool contains (Atom value);
		XMLListObject *copy ();
		XMLListObject *descendants (Atom name); 
		XMLListObject *elements (Atom name); 
		bool hasOwnProperty (Atom P);
		uint32 AS_based_length () const;
		bool hasComplexContent ();
		bool hasSimpleContent ();
		Atom name();
		XMLListObject *normalize ();
		Atom parent ();
		XMLListObject *processingInstructions (Atom name); 
		bool propertyIsEnumerable (Atom P);
		XMLListObject *text ();
		Atom toString ();
		Stringp toStringMethod();
		String *toXMLString ();

		// The following are not in the spec but work if XMLList has one element
		XMLObject *addNamespace (Atom _namespace);
		XMLObject *appendChild (Atom child);
		int childIndex();
		ArrayObject *inScopeNamespaces ();
		Atom insertChildAfter (Atom child1, Atom child2);
		Atom insertChildBefore (Atom child1, Atom child2);
		Atom getNamespace (Atom *argv, int argc); // prefix is optional
		Atom localName ();
		ArrayObject *namespaceDeclarations ();
		String *nodeKind ();
		XMLObject *prependChild (Atom value);
		XMLObject *removeNamespace (Atom _namespace);
		XMLObject *replace (Atom propertyName, Atom value);
		XMLObject *setChildren (Atom value);
		void setLocalName (Atom name);
		void setName (Atom name);
		void setNamespace (Atom ns);

		// non-E4X extensions
		ScriptObject* getNotification();
		void setNotification(ScriptObject* f);

#ifdef XML_FILTER_EXPERIMENT
		XMLListObject *filter (Atom propertyName, Atom value);
#endif

	public:

		XMLListObject(XMLListClass *type, Atom targetObject = nullObjectAtom, const Multiname* targetProperty = 0);
		~XMLListObject();
	};
}

#endif /* __avmplus_XMLListObject__ */
