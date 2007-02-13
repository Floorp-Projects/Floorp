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

#ifndef __avmplus_Multiname__
#define __avmplus_Multiname__


namespace avmplus
{
	/**
	 * Multiname is a reference to an identifier in 0 or more namespaces.  It consists
	 * of the simple name and a list of namespaces.
	 * 
	 * Multinames are always stack allocated.  Namespace and name values are interened, since
	 * they are used as hashtable keys.  
	 */
	class Multiname 
	{
		const static int ATTR   = 0x01;	// attribute name
		const static int QNAME  = 0x02;	// qualified name (size==1, explicit in code)
		const static int RTNS   = 0x04;	// runtime namespace
		const static int RTNAME = 0x08;	// runtime name
		const static int NSSET  = 0x10;
		const static int PUBLICNS = 0x20; // temporary flag to support 46.15; public implied
		#ifdef AVMPLUS_MIR
		friend class CodegenMIR;
		#endif 
		#ifdef AVMPLUS_INTERP
		friend class Interpreter;
		#endif
		friend class HeapMultiname;
		int flags;
		Stringp name;
		union
		{
			Namespace* ns;
			NamespaceSet* nsset;
		};

	public:

		Stringp getName() const
		{
			AvmAssert(!isAnyName() && !isRtname());
			return name;
		}

		void setName(Stringp _name)
		{
			AvmAssert(_name && _name->isInterned());
			this->flags &= ~RTNAME;
			this->name = _name;
		}

		void setName(Multiname* other)
		{
			// copy name settings from other
			flags &= ~RTNAME;
			flags |= other->flags & RTNAME;
			this->name = other->name;
		}

		int namespaceCount() const 
		{
			return (nsset && (flags & NSSET)) ? nsset->size : 1;
		}

		Namespace* getNamespace(int i) const;

		Namespace* getNamespace() const
		{
			return getNamespace(0);
		}

		void setNamespace(Namespace* _ns)
		{
			flags &= ~(NSSET|RTNS);
			AvmAssert(_ns != NULL);
			this->ns = _ns;
		}

		void setNamespace(Multiname* other)
		{
			// copy namespace settings from other
			flags &= ~(NSSET|RTNS);
			flags |= other->flags & (NSSET|RTNS);
			this->ns = other->ns;
		}

		NamespaceSet* getNsset() const
		{
			AvmAssert(!isRtns() && (flags&NSSET));
			return nsset;
		}

		void setNsset(NamespaceSet* _nsset)
		{
			flags &= ~RTNS;
			flags |= NSSET;
			AvmAssert(_nsset != NULL);
			this->nsset = _nsset;
		}

		Multiname();

		Multiname(NamespaceSet* nsset);

		Multiname(const Multiname &other)
		{
			*this = other;
		}


		Multiname(Namespace* ns, Stringp name, bool qualified=false);

		~Multiname()
		{
			flags = 0;
			name = NULL;
			nsset = NULL;
		}

		bool contains(Namespace* ns) const;

		/**
		 * return the flags we want to keep when copying a compile-time
		 * multiname into a runtime temporary multiname
		 */
		int ctFlags() const {
			return flags & ~(RTNS|RTNAME);
		}

		/**
		 * returns true if this multiname could resolve to a binding.  Attributes,
		 * wildcards, and runtime parts mean it can't match any binding.
		 */
		int isBinding() const {
			return !(flags & (ATTR|RTNS|RTNAME)) && name && ns;
		}
		int isRuntime() const {
			return flags & (RTNS|RTNAME);
		}
		int isRtns() const {
			return flags & RTNS;
		}
		int isRtname() const {
			return flags & RTNAME;
		}
		int isQName() const {
			return flags & QNAME;
		}
		bool isAttr() const {
			return (bool) (flags & ATTR);
		}
		bool isAnyName() const {
			return !isRtname() && !name;
		}
		bool isAnyNamespace() const {
			return !isRtns() && !(flags&NSSET) && ns == NULL;
		}
		int isNsset() const {
			return flags & NSSET;
		}

		void setAttr(bool b=true) {
			if (b)
				flags |= ATTR;
			else
				flags &= ~ATTR;
		}
		void setPublic(bool b=true) {
			if (b)
				flags |= PUBLICNS;
			else
				flags &= ~PUBLICNS;
		}
		void setQName() {
			AvmAssert(namespaceCount()==1 && !(flags&NSSET));
			flags |= QNAME;
		}
		void setRtns() {
			flags |= RTNS;
			flags &= ~NSSET;
			ns = NULL;
		}
		void setRtname() {
			flags |= RTNAME;
			name = NULL;
		}
		void setAnyName() {
			flags &= ~RTNAME;
			name = NULL;
		}
		void setAnyNamespace() {
			flags &= ~(NSSET|RTNS);
			ns = NULL;
		}

		bool matches (const Multiname *mn) const;

//#ifdef AVMPLUS_VERBOSE
	public:
		typedef enum _MultiFormat
		{
			MULTI_FORMAT_FULL = 0,		/* default */
			MULTI_FORMAT_NAME_ONLY,     /* only name */
			MULTI_FORMAT_NS_ONLY		/* only namespaces */
		}
		MultiFormat;

		Stringp format(AvmCore* core, MultiFormat form=MULTI_FORMAT_FULL) const;
		static Stringp format(AvmCore* core, Namespace* ns, Stringp name, bool attr=false);
//#endif
	};

	// version of multiname sporting write barriers
	class HeapMultiname 
	{
	public:

		HeapMultiname() {}

		HeapMultiname(NamespaceSet* nsset) { name.setNsset(nsset); }

		HeapMultiname(const Multiname &other)
		{
			setMultiname(other);
		}

		HeapMultiname(Namespace* ns, Stringp name, bool qualified=false);


		operator Multiname* () { return &name; }

		const Multiname& getMultiname() const { return name; }

		void setMultiname(const Multiname &other)
		{
			WBRC(gc(), this, &name.name, other.name);
			if(name.isNsset())
				name.ns = NULL;
			else
				WBRC(gc(), this, &name.ns, NULL);

			if(other.isNsset()) {
				WB(gc(), this, &name.ns, other.ns);
			} else {
				WBRC(gc(), this, &name.ns, other.ns);
			}
			name.flags = other.flags;
		}

		void setName(Stringp n) {
			WBRC(gc(), this, &name.name, n);
			name.setName(n);
		}

		void setName(Multiname* other) {
			WBRC(gc(), this, &name.name, other->name);
			name.setName(other);
		}
		
		void setNamespace(Namespace* ns) {
			WBRC(gc(), this, &name.ns, ns);
			name.setNamespace(ns);
		}

		void setNamespace(Multiname* other)	{
			WBRC(gc(), this, &name.ns, other->ns);
			name.setNamespace(other);
		}

		void setNsset(NamespaceSet* nsset) {
			WB(gc(), this, &name.nsset, nsset);
			name.setNsset(nsset);
		}
		
		void setRtns() { 
			WBRC(gc(), this, &name.name, NULL);
			name.setRtns(); 
		}
		void setRtname() 
		{ 
			WBRC(gc(), this, &name.name, NULL);
			name.setRtname(); 
		}
		void setAnyName() { 
			WBRC(gc(), this, &name.name, NULL);
			name.setAnyName(); 
		}
		void setAnyNamespace() { 
			WBRC(gc(), this, &name.ns, NULL);
			name.setAnyNamespace();
		}

		Stringp getName() const { return name.getName(); }
		int namespaceCount() const { return name.namespaceCount(); }
		Namespace* getNamespace(int i) const { return name.getNamespace(i); }
		Namespace* getNamespace() const { return name.getNamespace(); }
		NamespaceSet* getNsset() const { return name.getNsset(); }
		bool contains(Namespace* ns) const { return name.contains(ns); }
		int ctFlags() const { return name.ctFlags(); }
		int isBinding() const { return name.isBinding(); }
		int isRuntime() const { return name.isRuntime(); }
		int isRtns() const { return name.isRtns(); }
		int isRtname() const { return name.isRtname(); }
		int isQName() const { return name.isQName(); }
		bool isAttr() const { return name.isAttr(); }
		bool isAnyName() const { return name.isAnyName(); }
		bool isAnyNamespace() const { return name.isAnyNamespace(); }
		int isNsset() const { return name.isNsset(); }

		void setAttr(bool b=true) { name.setAttr(b); }
		void setQName() { name.setQName(); }

		bool matches (const Multiname *mn) const { return name.matches(mn); }

//#ifdef AVMPLUS_VERBOSE
	public:
		Stringp format(AvmCore* core, Multiname::MultiFormat form=Multiname::MULTI_FORMAT_FULL) const { return name.format(core, form); }
		static Stringp format(AvmCore* core, Namespace* ns, Stringp name, bool attr=false) { return format(core, ns, name, attr); }
//#endif
	private:
        Multiname name;
		MMgc::GC* gc() const { return MMgc::GC::GetGC(this); }
	};
}

#endif /* __avmplus_Multiname__ */
