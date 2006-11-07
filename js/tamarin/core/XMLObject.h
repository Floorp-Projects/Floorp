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

#ifndef __avmplus_XMLObject__
#define __avmplus_XMLObject__


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

// These are static objects on the XML type
// E4X: The XML constructor has the following properties
// XML.ignoreComments
// XML.ignoreProcessingInstructions
// XML.ignoreWhitespace
// XML.prettyPrinting
// XML.prettyIndent
// XML.settings
// XML.setSettings ([settings])
// XML.defaultSettings()

// XML.prototype.constructor ??
// XML.prototype.addNamespace
// XML.prototype.appendChild
// XML.prototype.attribute
// XML.prototype.attributes
// XML.prototype.child
// XML.prototype.childIndex
// XML.prototype.children
// XML.prototype.comments
// XML.prototype.contains
// XML.prototype.copy
// XML.prototype.descendants
// XML.prototype.elements
// XML.prototype.hasOwnProperty
// XML.prototype.hasComplexContent
// XML.prototype.hasSimpleContent
// XML.prototype.inScopeNamespaces
// XML.prototype.insertChildAfter
// XML.prototype.insertChildBefore
// XML.prototype.length
// XML.prototype.localName
// XML.prototype.name
// XML.prototype.namespace
// XML.prototype.namespaceDeclarations
// XML.prototype.nodeKind
// XML.prototype.normalize
// XML.prototype.parent
// XML.prototype.processingInstructions
// XML.prototype.prependChild
// XML.prototype.propertyIsEnumerable
// XML.prototype.removeNamespace
// XML.prototype.replace
// XML.prototype.setChildren
// XML.prototype.setLocalName
// XML.prototype.setName
// XML.prototype.setNamespace
// XML.prototype.text
// XML.prototype.toString
// XML.prototype.toXMLString
// XML.prototype.valueOf


namespace avmplus
{
	class XMLObject : public ScriptObject
	{
		XMLClass* xmlClass() const
		{
			return toplevel()->xmlClass();
		}

	protected:
		E4XNode *m_node;

		friend class XMLClass;

	public:

		E4XNode *getNode() const { return m_node; }
		void setNode(E4XNode* node) { WB(MMgc::GC::GetGC(this), this, &m_node, node); }

		// Functions that override object version
		// Delete (deleteProperty)
		// Get (getProperty)
		// HasProperty 
		// Put (setProperty)
		Atom callProperty(Multiname* name, int argc, Atom* argv);

		Atom getProperty(Atom name) const;		
		void setProperty(Atom name, Atom value);
		bool deleteProperty(Atom name);

		Atom getProperty(Stringp name) const
		{
			AvmAssert(name != NULL && name->isInterned());
			return getProperty(name->atom());
		}
		void setProperty(Stringp name, Atom value)
		{
			AvmAssert(name != NULL && name->isInterned());
			setProperty(name->atom(), value);
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

		// Iterator support - for in, for each
		Atom nextName(int index);
		Atom nextValue(int index);
		int nextNameIndex(int index);

		Atom _resolveValue ();

		// Exposed routines to AS (NATIVE_METHODs)
		XMLObject *addNamespace (Atom _namespace);
		XMLObject * appendChild (Atom child);
		XMLListObject *attribute (Atom arg);
		XMLListObject *attributes ();
		XMLListObject *child (Atom propertyName);
		int childIndex();
		XMLListObject *children ();
		XMLListObject *comments ();
		bool contains (Atom value);
		XMLObject *copy ();
		XMLListObject *descendants (Atom PropertyName) const;
		XMLListObject *elements (Atom name); 
		bool hasOwnProperty (Atom P);
		bool hasComplexContent ();
		bool hasSimpleContent ();
		ArrayObject *inScopeNamespaces ();
		Atom insertChildAfter (Atom child1, Atom child2);
		Atom insertChildBefore (Atom child1, Atom child2);

		// non-E4X extensions
		ScriptObject* getNotification();
		void setNotification(ScriptObject* f);

		Atom localName ();
		Atom name ();
		Atom getNamespace (Atom *argv, int argc); // prefix is optional (was previously called namespace)
		ArrayObject *namespaceDeclarations ();
		String *nodeKind () const;
		XMLObject *normalize ();
		Atom parent ();
		XMLListObject *processingInstructions (Atom name); 
		XMLObject *prependChild (Atom value);
		bool propertyIsEnumerable (Atom P);
		XMLObject *removeNamespace (Atom _namespace);
		XMLObject *replace (Atom propertyName, Atom value);
		XMLObject *setChildren (Atom value);
		void setLocalName (Atom name);
		void setName (Atom name);
		void setNamespace (Atom ns);
		XMLListObject *text ();
		Atom toString ();
		Stringp toStringMethod();
		String *toXMLString ();

		// E4X support routines
		void __toXMLString(StringBuffer &s, AtomArray *AncestorNamespace, int indentLevel = 0, bool includeChildren = true) const;
		XMLObject *_deepCopy () const;

		Namespace *GenerateUniquePrefix (Namespace *ns, const AtomArray *namespaces) const;

		static bool notifyNeeded(E4XNode* target);
		void issueNotifications(AvmCore* core, Toplevel* top, E4XNode* initialTarget, Atom target, Atom type, Atom value, Atom detail=undefinedAtom);

		void childChanges(Atom type, Atom value, E4XNode* prior=0);
		void nonChildChanges(Atom type, Atom value, Atom detail=undefinedAtom);

		// accessors to node functions
		int getClass() const; 
		uint32 _length() const;
		XMLObject *getParent();
		void setValue(Stringp s);
		Stringp getValue();
		bool getQName(Multiname *m);

		Namespace *GetNamespace (const Multiname &mn, const AtomArray *nsArray) const;

#ifdef XML_FILTER_EXPERIMENT
		XMLListObject *filter (Atom propertyName, Atom value);
		void _filter (XMLListObject *, const Multiname &m, Atom value);
#endif

	public:
		XMLObject(XMLClass *type, Stringp s=NULL, Namespace *defaultNamespace=NULL);
		XMLObject(XMLClass *type, E4XNode *node);

		~XMLObject();

#ifdef AVMPLUS_VERBOSE
	public:
		Stringp format(AvmCore* core) const;
#endif
	private:
		/**
		 * Conversion routine to verify XML related flags of Multiname
		 * (strips @ from strings and marks multiname as attribute)
		 */
		void CoerceE4XMultiname (Multiname *m, Multiname &out) const;

		bool NodeNameEquals (const wchar *NodeName, int len, Stringp parentName, Namespace *ns);
	};

	/**
	 * QNameObject is used to represent the "QName" object in the E4X Specification
	 *
	 * We also use this same object to represent "AttributeName" in the E4X spec.
	 * An AttributeName is simply a QName wrapper for finding properties that have a leading @ sign.
	 * It's an internal class to the spec and the only difference between a QName is the @.  Instead of
	 * having the overhead of an AttributeName class that wraps the QName class, we just use a boolean
	 * inside the QName to differentiate betweent the two types.
	 */
	class QNameObject : public ScriptObject
	{
		XMLClass* xmlClass() const
		{
			return toplevel()->xmlClass();
		}

		HeapMultiname m_mn;
		friend class QNameClass;

	public:
		QNameObject(QNameClass *type, Multiname &mn);
		QNameObject(QNameClass *type, Atom name, bool bAttribute=false);
		QNameObject(QNameClass *type, Namespace *nameSpace, Atom name, bool bAttribute=false);

		Stringp getLocalName() const;
		Atom getURI() const;

		bool isAttr() const { return m_mn.isAttr(); };

		// Iterator support - for in, for each
		Atom nextName(int index);
		Atom nextValue(int index);
		int nextNameIndex(int index);

		void getMultiname(Multiname& name) const
		{
			name = m_mn.getMultiname();
		}
	};
}
#endif /* __avmplus_XMLObject__ */
