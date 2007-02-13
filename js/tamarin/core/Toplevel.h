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

#ifndef __avmplus_Toplevel__
#define __avmplus_Toplevel__


namespace avmplus
{
	/**
	 * class Toplevel
	 */
    class Toplevel : public ScriptObject
    {
	public:
		/**
		 * Constructor
		 */
		Toplevel(VTable* vtable, ScriptObject* delegate);
		~Toplevel();

		/**
		 * @name actionscript.lang Classes
		 * These are the toplevel class closures.
		 */
		/*@{*/
		DRCWB(ArrayClass*)     arrayClass;
		DRCWB(BooleanClass*)   booleanClass;
		DRCWB(ClassClass*)     classClass;
		DateClass*           dateClass() { return (DateClass*)getBuiltinClass(avmplus::NativeID::abcclass_Date); }
		DRCWB(FunctionClass*)  functionClass;		
		DRCWB(MethodClosureClass*)  methodClosureClass;		
		DRCWB(NamespaceClass*) namespaceClass;
		DRCWB(NumberClass*)    numberClass;
		DRCWB(IntClass*)       intClass;
		DRCWB(UIntClass*)      uintClass;
		DRCWB(ObjectClass*)    objectClass;
		RegExpClass*         regexpClass() { return (RegExpClass*)getBuiltinClass(avmplus::NativeID::abcclass_RegExp); }
		DRCWB(StringClass*)    stringClass;
		XMLClass*            xmlClass() { return (XMLClass*)getBuiltinClass(avmplus::NativeID::abcclass_XML); }
		XMLListClass*        xmlListClass() { return (XMLListClass*)getBuiltinClass(avmplus::NativeID::abcclass_XMLList); }
		QNameClass*          qnameClass() { return (QNameClass*)getBuiltinClass(avmplus::NativeID::abcclass_QName); }
		/*@}*/

		/**
		 * @name Error Subclasses
		 * These are subclasses of Error used in the VM.
		 */
		/*@{*/
		ErrorClass *errorClass() const { return getErrorClass(avmplus::NativeID::abcclass_Error); }
		ErrorClass *argumentErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_ArgumentError); }
		ErrorClass *evalErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_EvalError); }
		ErrorClass *typeErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_TypeError); }
		ErrorClass *rangeErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_RangeError); }
		ErrorClass *uriErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_URIError); }
		ErrorClass *referenceErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_ReferenceError); }
		ErrorClass *securityErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_SecurityError); }
		ErrorClass *verifyErrorClass() const { return getErrorClass(avmplus::NativeID::abcclass_VerifyError); }
		/*@}*/

		void throwVerifyError(int id) const;

#ifdef DEBUGGER
		void throwVerifyError(int id, Stringp arg1) const;
		void throwVerifyError(int id, Stringp arg1, Stringp arg2) const;
#else
		void throwVerifyError(int id, Stringp arg1) const { throwVerifyError(id); (void)arg1; }
		void throwVerifyError(int id, Stringp arg1, Stringp arg2) const { throwVerifyError(id); (void)arg1;(void)arg2; }
#endif

		void throwTypeError(int id) const;
		void throwTypeError(int id, Stringp arg1) const;
		void throwTypeError(int id, Stringp arg1, Stringp arg2) const;

		void throwError(int id) const;
		void throwError(int id, Stringp arg1) const;
		void throwError(int id, Stringp arg1, Stringp arg2) const;

		void throwArgumentError(int id) const;
		void throwArgumentError(int id, Stringp arg1) const;
		void throwArgumentError(int id, const char *arg1) const;
		void throwArgumentError(int id, Stringp arg1, Stringp arg2) const;

		void throwRangeError(int id) const;
		void throwRangeError(int id, Stringp arg1) const;
		void throwRangeError(int id, Stringp arg1, Stringp arg2, Stringp arg3) const;

		void throwReferenceError(int id, Multiname* multiname, const Traits* traits) const;
		void throwReferenceError(int id, Multiname* multiname) const;

		DWB(VTable*) object_vtable; // instance vtable
		DWB(VTable*) class_vtable; // instance vtable

		// 
		// methods that used to be on AvmCore but depend on the caller's environment
		//
		ScriptObject* toPrototype(Atom atom);
		VTable* toVTable(Atom atom);

		/** toObject + get traits */
		Traits*        toTraits(Atom atom);
		
		/**
		 * OP_call.
		 *
		 * this = atomv[0]
		 * arg1 = atomv[1]
		 * argN = atomv[argc]
		 */
		Atom op_call(Atom method,
					 int argc,
					 Atom* atomv);

		/**
		 * OP_construct.
		 *
		 * this = atomv[0] (ignored)
		 * arg1 = atomv[1]
		 * argN = atomv[argc]
		 */
		Atom op_construct(Atom ctor,
						  int argc,
						  Atom* atomv);


		/** Implementation of OP_callproperty */
		Atom callproperty(Atom base, Multiname* name, int argc, Atom* atomv, VTable* vtable);

		/** Implementation of OP_constructprop */
		Atom constructprop(Multiname* name, int argc, Atom* atomv, VTable* vtable);
		
		/**
		 * Implements the ToAttributeName API as specified in E4X 10.5.1, pg 37
		 */
		QNameObject* ToAttributeName (const Atom arg);
		QNameObject* ToAttributeName (const Stringp arg)
		{
			return ToAttributeName(arg->atom());
		}

		/**
		 * Implements the ToXMLName API as specified in E4X 10.6.1, page 38
		 */
		void ToXMLName (const Atom arg, Multiname& m);

		/**
		 * E4X support for coercing regular Multinames into E4X specific ones
		 */
		void CoerceE4XMultiname (Multiname *m, Multiname &out) const;

		/**
		 * operator instanceof from ES3
		 */
		Atom instanceof(Atom atom, Atom ctor);

		/**
		 * This is the implicit coercion operator.  It is kind of like a
		 * Java downcast, but because of how E4 works, there are some cases
		 * when it returns a different value than what you pass in.
		 *
		 * It will happily return null if you pass in null for
		 * any reference type.  And, it will throw an exception if the
		 * value is not in the type's value set.  It does not do type
		 * conversion.
		 *
		 * @param  atom      The atom containing the value to coerce.
		 * @param  itraits   The itraits of the type to coerce to.
		 * @return The coerced atom.
		 * @throws Exception if the value is not in the type's value
		 *                   set.
		 */
		Atom coerce(Atom atom, Traits* expected) const;
		void coerceobj(ScriptObject* obj, Traits* expected) const;

		/**
		 * Reads a property from an object, with the property
		 * to retrieve specified by its binding.
		 * The binding was likely retrieved using getBinding.
		 * @param obj Object to retrieve property from
		 * @param b The binding of the property
		 * @param traits The traits of the object
		 */
		Atom getproperty(Atom obj, Multiname* name, VTable* vtable);

	    void setproperty(Atom obj, Multiname* multiname, Atom value, VTable* vtable) const;
	    void setproperty_b(Atom obj, Multiname* multiname, Atom value, VTable* vtable, Binding b) const;

		bool isXmlBase(Atom obj) const
		{
			AvmCore* core = this->core();
			if (AvmCore::isObject(obj))
			{
				ScriptObject* so = AvmCore::atomToScriptObject(obj);
				Traits* t = so->traits();
				return t == core->traits.xml_itraits || t == core->traits.xmlList_itraits;
			}
			return false;
		}
		/**
		 * operator +
		 */
		Atom add2(Atom lhs, Atom rhs);

		/**
		 * Implements the GetDefaultNamespace API as specified in E4X 12.1.1, pg 59
		 *
		 */
		Namespace *getDefaultNamespace();

		/**
		 * Retrieve a binding for a property of a class.
		 * This differs from the other overload of getBinding
		 * in that it takes a multiname instead of a name/ns
		 * pair.
		 * The returned binding can then be used to read/write
		 * the property 
		 * @param traits    The traits of the class
		 * @param multiname The multiname of the property
		 */
		Atom getBinding(Traits* traits, Multiname* multiname) const;

		#ifdef DEBUGGER
		ScriptObjectTable *scriptObjectTable;		
		#endif

		/**
		 * @name ECMA-262 Appendix B.2 extensions
		 * Extensions to ECMAScript, in ECMA-262 Appendix B.2
		 */
		/*@{*/
		Stringp escape(Stringp in);
		Stringp unescape(Stringp in);
		/*@}*/

		/**
		 * This is a variant of escape() which doesn't encode
		 * Unicode characters using the %u sequence
		 */
		Stringp escapeBytes(Stringp in);

		/**
		 * @name Toplevel Function Properties
		 * Function properties of the global object (ECMA 15.1.2)
		 */
		/*@{*/
		Stringp decodeURI(Stringp uri);
		Stringp decodeURIComponent(Stringp uri);
		Stringp encodeURI(Stringp uri);
		Stringp encodeURIComponent(Stringp uri);
		bool isNaN(double d);
		bool isFinite(double d);
		double parseInt(Stringp in, int radix);
		double parseFloat(Stringp in);
		/*@}*/

		// For E4X
		bool isXMLName(Atom v);

		DECLARE_NATIVE_SCRIPT(Toplevel)

        ClassClosure* getBuiltinClass(int class_id) const
        {
            return builtinClasses[class_id] ? builtinClasses[class_id] : const_cast<Toplevel*>(this)->resolveBuiltinClass(class_id);
        }
		ErrorClass* getErrorClass(int class_id) const { return (ErrorClass*)getBuiltinClass(class_id); }

		unsigned int readU30(const byte *&p) const;

	private:

		int parseHexChar(wchar c);
		wchar extractCharacter(const wchar*& src);
		Stringp decode(Stringp in, bool decodeURIComponentFlag);
		Stringp encode(Stringp in, bool encodeURIComponentFlag);

		static const uint32 unescaped[];
		static const uint32 uriUnescaped[];
		static const uint32 uriReservedPlusPound[];
		
		inline static bool contains(const uint32 *uriSet, uint32 ch)
		{
			return (ch<0x80) && (uriSet[ch>>5]&(1<<(ch&0x1f))) != 0;
		}

		ClassClosure* resolveBuiltinClass(int class_id);
		DWB(ClassClosure**) builtinClasses;
	};
}

#endif /* __avmplus_Toplevel__ */
