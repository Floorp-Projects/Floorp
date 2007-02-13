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
#undef DEBUG_EARLY_BINDING

	//
	// builtins
	//
	BEGIN_NATIVE_MAP(Toplevel)
		NATIVE_METHOD_FLAGS(escape, Toplevel::escape, 0)
		NATIVE_METHOD_FLAGS(unescape, Toplevel::unescape, 0)
		NATIVE_METHOD_FLAGS(decodeURI, Toplevel::decodeURI, 0)
		NATIVE_METHOD_FLAGS(decodeURIComponent, Toplevel::decodeURIComponent, 0)
		NATIVE_METHOD_FLAGS(encodeURI, Toplevel::encodeURI, 0)
		NATIVE_METHOD_FLAGS(encodeURIComponent, Toplevel::encodeURIComponent, 0)
		NATIVE_METHOD_FLAGS(isNaN, Toplevel::isNaN, 0)
		NATIVE_METHOD_FLAGS(isFinite, Toplevel::isFinite, 0)
		NATIVE_METHOD_FLAGS(parseInt, Toplevel::parseInt, 0)
		NATIVE_METHOD_FLAGS(parseFloat, Toplevel::parseFloat, 0)
		NATIVE_METHOD(isXMLName, Toplevel::isXMLName)		
	END_NATIVE_MAP()


	Toplevel::Toplevel(VTable* cvtable, ScriptObject* delegate)
		: ScriptObject(cvtable, delegate)
	{
		builtinClasses = (ClassClosure**) core()->GetGC()->Alloc(sizeof(ClassClosure*) * core()->builtinPool->cinits.capacity(), MMgc::GC::kZero | MMgc::GC::kContainsPointers);
	}

	Toplevel::~Toplevel()
	{
#ifdef DEBUGGER
		scriptObjectTable = NULL;
#endif
	}

	ClassClosure* Toplevel::resolveBuiltinClass(int class_id)
	{
		Traits *traits = core()->builtinPool->cinits[class_id]->declaringTraits->itraits;
		Multiname qname(traits->ns, traits->name);
		ScriptObject *container = vtable->init->finddef(&qname);

		Atom classAtom = getproperty(container->atom(), &qname, container->vtable);
		ClassClosure *cc = (ClassClosure*)AvmCore::atomToScriptObject(classAtom);
		//builtinClasses[class_id] = cc;
		WBRC(core()->GetGC(), builtinClasses, &builtinClasses[class_id], cc);
		return cc;
	}

	ScriptObject* Toplevel::toPrototype(Atom atom)
	{
		if (!AvmCore::isNullOrUndefined(atom))
		{
			switch (atom&7)
			{
			default:
			
			case kNamespaceType:
				return namespaceClass->prototype;

			case kStringType:
				return stringClass->prototype;

			case kObjectType:
				return AvmCore::atomToScriptObject(atom)->getDelegate();

			case kDoubleType:
			case kIntegerType:
				// ISSUE what about int?
				return numberClass->prototype;

			case kBooleanType:
				return booleanClass->prototype;
			}
		}
		else
		{
			// TypeError in ECMA
			throwTypeError(
				(atom == undefinedAtom) ? kConvertUndefinedToObjectError :
											kConvertNullToObjectError);
			return NULL;
		}
	}

	// equivalent to ToObject, obj->traits.  exception if null or undefined.
	VTable* Toplevel::toVTable(Atom atom)
	{
		if (!AvmCore::isNullOrUndefined(atom))
		{
			switch (atom&7)
			{
			case kObjectType:
				return AvmCore::atomToScriptObject(atom)->vtable;
			case kNamespaceType:
				return namespaceClass->ivtable();
			case kStringType:
				return stringClass->ivtable();
			case kBooleanType:
				return booleanClass->ivtable();
			case kIntegerType:
			case kDoubleType:
				// ISSUE what about int?
				return numberClass->ivtable();
			}
		}
		else
		{
            // TypeError in ECMA
			throwTypeError(
					(atom == undefinedAtom) ? kConvertUndefinedToObjectError :
										kConvertNullToObjectError);
		}
		return NULL;
	}
	
	// equivalent to ToObject, obj->traits.  exception if null or undefined.
	Traits* Toplevel::toTraits(Atom atom)
	{
		if (!AvmCore::isNullOrUndefined(atom))
		{
			switch (atom&7)
			{
			case kObjectType:
				return AvmCore::atomToScriptObject(atom)->traits();
			case kNamespaceType:
				return core()->traits.namespace_itraits;
			case kStringType:
				return core()->traits.string_itraits;
			case kBooleanType:
				return core()->traits.boolean_itraits;
			case kIntegerType:
			case kDoubleType:
				// ISSUE what about int?
				return core()->traits.number_itraits;
			}
		}
		else
		{
            // TypeError in ECMA
			ErrorClass *error = typeErrorClass();
			if( error )
				error->throwError(
					(atom == undefinedAtom) ? kConvertUndefinedToObjectError :
										kConvertNullToObjectError);
			else
				throwVerifyError(kCorruptABCError);
		}
		return NULL;
	}
	
    /**
     * OP_call.
     *
     * arg0 = argv[0]
     * arg1 = argv[1]
     * argN = argv[argc]
     */
    Atom Toplevel::op_call(Atom method, int argc, Atom* atomv)
    {
		if (!AvmCore::isObject(method))
		{
			Multiname name(core()->publicNamespace, core()->constantString("value"));
			throwTypeError(kCallOfNonFunctionError, core()->toErrorString(&name));
		}
		return AvmCore::atomToScriptObject(method)->call(argc, atomv);
    }

    /**
     * OP_construct.  Note that arguments are in the opposite order from AVM.
     *
	 * this = argv[0] // ignored
     * arg1 = argv[1]
     * argN = argv[argc]
     */
    Atom Toplevel::op_construct(Atom ctor, int argc, Atom* atomv)
    {
		if (!AvmCore::isObject(ctor))
		{
			throwTypeError(kConstructOfNonFunctionError);
		}

		ScriptObject *ct = AvmCore::atomToScriptObject(ctor);
		Atom val = ct->construct(argc, atomv);
#ifdef DEBUGGER
		if(core()->allocationTracking)
			ct->addInstance(val);
#endif
		return val;
	}

	
	// E4X 10.5.1, pg 37
	QNameObject* Toplevel::ToAttributeName(Atom attributeName)
	{
		if (!AvmCore::isNullOrUndefined(attributeName))
		{
			AvmCore* core = this->core();
			switch (attributeName&7)
			{
			case kNamespaceType:
				attributeName = AvmCore::atomToNamespace(attributeName)->getURI()->atom();
				break;
			case kObjectType:
				// check for XML, XMLList, Object, AttributeName, AnyName
				// if XML, toString, then do default string case
				// if XMLList, toString, then do default string case
				// if AttributeName, return the input argument
				// if AnyName, return the result of calling "ToAttributeName(*)"
				// if QName, return attributeName
				// otherwise, do toString, then to default case
				if (core->isQName(attributeName))
				{
					QNameObject *q = core->atomToQName (attributeName);
					if (q->isAttr())
						return q;
					else
						return new (core->GetGC(), qnameClass()->ivtable()->getExtraSize()) QNameObject(qnameClass(), attributeName, true);
				}
				else
				{
					attributeName = core->string(attributeName)->atom();
					break;
				}
				break;
			case kStringType:
				{
					break;
				}
			case kBooleanType:
			case kIntegerType:
			case kDoubleType:
			default: // number
				throwTypeError(kConvertUndefinedToObjectError);
			}

			return new (core->GetGC(), qnameClass()->ivtable()->getExtraSize()) QNameObject(qnameClass(), attributeName, true);
		}
		else
		{
			throwTypeError(kConvertUndefinedToObjectError);
			return NULL;
		}
	}

	// E4X 10.6.1, page 38
	// This returns a Multiname create from a unqualified generic type.
	// The multiname returned will have one namespace and will be correctly
	// marked as an attribute if input is an attribute
	void Toplevel::ToXMLName (const Atom p, Multiname& m) 
	{
		Stringp s = 0;
		AvmCore* core = this->core();

		if (!AvmCore::isNullOrUndefined(p))
		{
			switch (p & 7)
			{
			case kNamespaceType:
				s = AvmCore::atomToNamespace(p)->getURI();
				break;
			case kObjectType:
				{
					// check for XML, XMLList, Object, AttributeName, AnyName
					// if XML, toString, then do default string case
					// if XMLList, toString, then do default string case
					// if AttributeName, return the input argument
					// if AnyName, return the result of calling "ToAttributeName(*)"
					// if QName, return attributeName
					if (core->isQName(p))
					{
						QNameObject *q = core->atomToQName (p);

						m.setAttr(q->isAttr() ? true : false);
						m.setNamespace(core->newNamespace (q->getURI()));
						Stringp name = q->getLocalName();
						if (name == core->kAsterisk)
						{
							m.setAnyName(); // marks it as an anyName
						}
						else
						{
							m.setName(name);
						}
						return;
					}
					else // XML, XMLList or generic object - convert to string
					{
						s = core->string(p);
						break;
					}
				}
			case kIntegerType:
			case kDoubleType:
			case kStringType:
			case kBooleanType:
				{
					s = core->string(p);
					break;
				}
			}
		}
		else
		{
			throwTypeError(kConvertUndefinedToObjectError);
			return;
		}

		// At this point p should be a string atom
		AvmAssert (s != 0);

		// if s is integer, throw TypeError
		// if first character of s is "@", return __toAttributeName (string - @)
		// else, return QName (s)
		if ((*s)[0] == '@')
		{
			// __toAttributeName minus the @
			Stringp news = new (core->GetGC()) String(s, 1, s->length() - 1);
			m.setName(core->internString(news));
			m.setAttr();
		}
		else
		{
			m.setName(core->internString(s));
		}

		if (m.getName() == core->kAsterisk)
		{
			m.setAnyName(); // marks it as an anyName
		}

		m.setNamespace(core->publicNamespace);
	}

	void Toplevel::CoerceE4XMultiname (Multiname *m, Multiname &out) const
	{
		// This function is used to convert raw string access into correct
		// Multiname types:
		// x["*"]
		// x["@*"]
		// Unqualified anyName types are correct handled in Multiname::matches
		// so we do not edit their namespaces here.  (They should have one
		// namespace which is null according to the E4X spec.)
		//
		// Unqualified regular types have the default XML namespace added to their
		// namespace count here.

		AvmAssert(!m->isRuntime());

		AvmCore *core = this->core();

		if (m->isQName())
		{
			AvmAssert(m->namespaceCount() == 1);
			out.setNamespace(m);
			out.setQName();
		}
		else
		{
			// If we're any namespace, no work required.
			if (m->isAnyNamespace())
			{
				out.setAnyNamespace();
			}
			else
			{
				// search for a match in our nsSet for the defaultNamespace
				Namespace *defaultNs = toplevel()->getDefaultNamespace();
				bool bMatch = false;
				for (int i=0, n=m->namespaceCount(); i < n; i++)
				{
					Namespace *ns = m->getNamespace(i);
					if (ns && ns->getPrefix() == defaultNs->getPrefix() && 
						ns->getURI() == defaultNs->getURI() &&
						ns->getType() == defaultNs->getType())
					{
						bMatch = true;
						break;
					}
				}

				// For an unqualified reference, we need to add in the default xml namespace
				// since we did not find a match for it in our namespace set
				if (!bMatch)
				{
					int newNameCount = m->namespaceCount() + 1;
					NamespaceSet *nsset = core->newNamespaceSet(newNameCount);
					for (int i=0, n=m->namespaceCount(); i < n; i++)
					{
						nsset->namespaces[i] = m->getNamespace(i);
					}
					//Stringp s1 = string(getDefaultNamespace()->getPrefix());
					//Stringp s2 = string(getDefaultNamespace()->getURI());
					nsset->namespaces[newNameCount-1] = toplevel()->getDefaultNamespace();
					out.setNsset(nsset);
				}
				else
				{	
					if (m->namespaceCount() > 1)
						out.setNsset(m->getNsset());
					else
						out.setNamespace (m->getNamespace());
				}
			}
		}

		out.setAttr(m->isAttr() ? true : false);

		if (m->isAnyName())
		{
			out.setAnyName();
		}
		else
		{
			Stringp s = m->getName();
			if ((s->length() == 1) && ((*s)[0] == '*'))
			{
				// Mark is as an "anyName" (name == undefined makes isAnyName true)
				out.setAnyName();
			}
			else if ((s->length() >= 1) && ((*s)[0] == '@'))
			{
				// If we're already marked as an attribute, we don't want to modify 
				// our string in any way.  Degenerative cases where you call:
				// XML.attribute (new QName("@*"))
				if (!out.isAttr())
				{
					// check for "@*"
					if ((s->length() == 2) && ((*s)[1] == '*'))
						out.setAnyName();
					else
						out.setName(core->internString (new (core->GetGC()) String(s, 1, s->length()-1)));
					out.setAttr();
				}
				else
				{
					if (m->isAnyName())
						out.setAnyName();
					else
						out.setName(m->getName());
				}
			}
			else
			{
				if (m->isAnyName())
					out.setAnyName();
				else
					out.setName(m->getName());
			}
		}
	}

	Atom Toplevel::callproperty(Atom base, Multiname* multiname, int argc, Atom* atomv, VTable* vtable)
	{
		Binding b = getBinding(vtable->traits, multiname);
		switch (b&7)
		{
		case BIND_METHOD:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callproperty method " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// force receiver == base.  if caller used OP_callproplex then receiver was null.
			atomv[0] = base;
			MethodEnv* method = vtable->methods[AvmCore::bindingToMethodId(b)];
			AvmAssert(method != NULL);
			return method->coerceEnter(argc, atomv);
		}
		case BIND_VAR:
		case BIND_CONST:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callproperty slot " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			Atom method = AvmCore::atomToScriptObject(base)->getSlotAtom(AvmCore::bindingToSlotId(b));
			return op_call(method, argc, atomv);
		}
		case BIND_GET:
		case BIND_GETSET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callproperty getter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// Invoke the getter on base
			int m = AvmCore::bindingToGetterId(b);
			MethodEnv *f = vtable->methods[m];
			Atom atomv_out[1] = { base };
			Atom method = f->coerceEnter(0, atomv_out);
			return op_call(method, argc, atomv);
		}
		case BIND_SET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callproperty setter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// read on write-only property
			throwReferenceError(kWriteOnlyError, multiname, vtable->traits);
		}
		default:
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "callproperty dynamic " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			if (AvmCore::isObject(base))
			{
				return AvmCore::atomToScriptObject(base)->callProperty(multiname, argc, atomv);
			}
			else
			{
				// primitive types are not dynamic, so we can go directly
				// to their __proto__ object
				ScriptObject* proto = toPrototype(base);
				Atom method = proto->getMultinameProperty(multiname);
				return op_call(method, argc, atomv);
			}
		}
	}

	Atom Toplevel::constructprop(Multiname* multiname, int argc, Atom* atomv, VTable* vtable)
	{
		Binding b = getBinding(vtable->traits, multiname);
		Atom obj = atomv[0];
		AvmCore* core = this->core();
		switch (b&7)
		{
		case BIND_METHOD:
		{
			// can't invoke method as constructor
			MethodEnv* env = vtable->methods[AvmCore::bindingToMethodId(b)];
			throwTypeError(kCannotCallMethodAsConstructor, core->toErrorString((AbstractFunction*)env->method));
		}
		case BIND_VAR:
		case BIND_CONST:
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "constructprop slot " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			Atom ctor = AvmCore::atomToScriptObject(obj)->getSlotAtom(AvmCore::bindingToSlotId(b));
			if (!core->istype(ctor, CLASS_TYPE) && !core->istype(ctor, FUNCTION_TYPE))
				throwTypeError(kNotConstructorError, core->toErrorString(multiname));
			return op_construct(ctor, argc, atomv);
		}
		case BIND_GET:
		case BIND_GETSET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "constructprop getter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// Invoke the getter
			int m = AvmCore::bindingToGetterId(b);
			MethodEnv *f = vtable->methods[m];
			Atom atomv_out[1] = { obj };
			Atom ctor = f->coerceEnter(0, atomv_out);
			return op_construct(ctor, argc, atomv);
		}
		case BIND_SET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "constructprop setter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// read on write-only property
			throwReferenceError(kWriteOnlyError, multiname, vtable->traits);
		}
		default:
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "constructprop dynamic " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			if ((obj&7)==kObjectType)
			{
				return AvmCore::atomToScriptObject(obj)->constructProperty(multiname, argc, atomv);
			}
			else
			{
				// primitive types are not dynamic, so we can go directly
				// to their __proto__ object
				ScriptObject* proto = toPrototype(obj);
				Atom ctor = proto->getMultinameProperty(multiname);
				return op_construct(ctor, argc, atomv);
			}
		}
	}	

	Atom Toplevel::instanceof(Atom atom, Atom ctor)
	{
		AvmCore* core = this->core();
		if ((ctor&7) != kObjectType ||
			!core->istype(ctor, core->traits.function_itraits) &&
			!core->istype(ctor, core->traits.class_itraits))
		{
			throwTypeError(kCantUseInstanceofOnNonObjectError);
		}
		// check for null before tryign to call toPrototype(atom), which will throw an error for null.
		if (AvmCore::isNull(atom))
   			return falseAtom;

		ClassClosure* c = (ClassClosure*)AvmCore::atomToScriptObject(ctor);

		ScriptObject *proto = c->prototype;
		ScriptObject *o = toPrototype(atom);

		for (; o != NULL; o = o->getDelegate())
		{
			if (o == proto)
				return trueAtom;
		}

		return falseAtom;
	}

	
	/**
	 * implements ECMA implicit coersion.  returns the coerced value,
	 * or throws a TypeError if coersion is not possible.
	 */
    Atom Toplevel::coerce(Atom atom, Traits* expected) const
    {
		Traits* actual;
		AvmCore* core = this->core();

		// these types always succeed
		if (expected == NULL)
			return atom;
		if (expected == BOOLEAN_TYPE)
			return core->booleanAtom(atom);
		if (expected == NUMBER_TYPE)
			return core->numberAtom(atom);
		if ((expected == STRING_TYPE))
			return AvmCore::isNullOrUndefined(atom) ? 0|kStringType : core->string(atom)->atom();
		if (expected == INT_TYPE)
			return core->intAtom(atom);
		if (expected == UINT_TYPE)
			return core->uintAtom(atom);
		if (expected == OBJECT_TYPE)
			return atom == undefinedAtom ? nullObjectAtom : atom;

		if (AvmCore::isNullOrUndefined(atom))
			return expected == VOID_TYPE ? undefinedAtom : nullObjectAtom;

		switch (atom&7)
		{
		case kStringType:
			actual = STRING_TYPE;
			break;

		case kBooleanType:
			actual = BOOLEAN_TYPE;
			break;

		case kDoubleType:
			actual = NUMBER_TYPE;
			break;

		case kIntegerType:
			actual = INT_TYPE;
			break;

		case kNamespaceType:
			actual = NAMESPACE_TYPE;
			break;

		case kObjectType:
			actual = AvmCore::atomToScriptObject(atom)->traits();
			break;

		default:
			// unexpected atom type
			AvmAssert(false);
			return false;
		}

		if (actual->containsInterface(expected))
		{
			return atom;
		}
		else
		{
			// failed
#ifdef AVMPLUS_VERBOSE
			//core->console << "checktype failed " << expected << " <- " << atom << "\n";
#endif
			throwTypeError(kCheckTypeFailedError, core->atomToErrorString(atom), core->toErrorString(expected));
			return atom;//unreachable
		}
    }

	void Toplevel::coerceobj(ScriptObject* obj, Traits* expected) const
	{
		if (obj && !obj->traits()->containsInterface(expected))
		{
			// failed
#ifdef DEBUGGER
			//core->console << "checktype failed " << expected << " <- " << atom << "\n";
#endif
			throwTypeError(kCheckTypeFailedError, core()->atomToErrorString(obj->atom()), core()->toErrorString(expected));
		}
	}
	
    Atom Toplevel::add2(Atom lhs, Atom rhs)
    {
		AvmCore* core = this->core();

		// do common cases first/quick:
		if (AvmCore::isNumber(lhs) && AvmCore::isNumber(rhs))
		{
			// C++ porting note. if either side is undefined, null or NaN then result must be NaN.
			// Java's + operator ensures this for double operands.
			// cn:  null should convert to 0, so I think the above comment is wrong for null.
			return core->doubleToAtom(core->number(lhs) + core->number(rhs));
		}
		else if (AvmCore::isString(lhs) || AvmCore::isString(rhs) || core->isDate(lhs) || core->isDate(rhs))
		{
 			return core->concatStrings(core->string(lhs), core->string(rhs))->atom();
		}


		// then look for the more unlikely cases
		
		// E4X, section 11.4.1, pg 53
		
		if (core->isObject (lhs) && core->isObject (rhs) && 
			   (core->isXML(lhs) || core->isXMLList(lhs)) 
			&& (core->isXML(rhs) || core->isXMLList (rhs)))
		{
			XMLListObject *l = new (core->GetGC()) XMLListObject(xmlListClass());
			l->_append (lhs);
			l->_append (rhs);
			return l->atom();
		}
		
		// to catch oddball cases like:
		//   function foo() { };
		//   foo.prototype.valueOf = function() { return new Object(); }
		//   foo.prototype.toString = function() { return 2; }
		//   print( new foo() + 33 ); // should be 35
		//
		// we need to follow the E3 spec:
		// 1. call ToPrimitive() on lhs and rhs, then
		// if L is String || R is String, concat, else add toNumber(lhs) to toNumber(rhs)

		// ToPrimitive() will call [[DefaultValue]], which calls valueOf().  If the result is
		//  a primitive, return that value else call toString() instead.

		// from E3:
		// NOTE No hint is provided in the calls to ToPrimitive in steps 5 and 6. All native ECMAScript objects except Date objects handle
		// the absence of a hint as if the hint Number were given; Date objects handle the absence of a hint as if the hint String were given.
		// Host objects may handle the absence of a hint in some other manner.
		
		Atom lhs_asPrimVal = core->primitive(lhs); // Date is handled above with the String argument case,  we don't have to check for it here.
		Atom rhs_asPrimVal = core->primitive(rhs);

		if (AvmCore::isString(lhs_asPrimVal) || AvmCore::isString(rhs_asPrimVal))
		{
			return core->concatStrings(core->string(lhs_asPrimVal), core->string(rhs_asPrimVal))->atom();
		}
		else
		{
			return core->doubleToAtom(core->number(lhs_asPrimVal) + core->number(rhs_asPrimVal));
		}
    }

    Atom Toplevel::getproperty(Atom obj, Multiname* multiname, VTable* vtable)
    {
		Binding b = getBinding(vtable->traits, multiname);
		AvmCore* core = this->core();
        switch (b&7)
        {
		case BIND_METHOD: 
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getproperty method " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			if (multiname->contains(core->publicNamespace) && isXmlBase(obj))
			{
				// dynamic props should hide declared methods
				ScriptObject* so = AvmCore::atomToScriptObject(obj);
				return so->getMultinameProperty(multiname);
			}
			// extracting a method
			MethodEnv *m = vtable->methods[AvmCore::bindingToMethodId(b)];
			return methodClosureClass->create(m, obj)->atom();
		}

        case BIND_VAR:
        case BIND_CONST:
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getproperty slot " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			int slot = AvmCore::bindingToSlotId(b);
			return AvmCore::atomToScriptObject(obj)->getSlotAtom(slot);
		}

		case BIND_NONE:
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getproperty dynamic " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			if ((obj&7) == kObjectType)
			{
				// try dynamic lookup on instance.  even if the traits are sealed,
				// we might need to search the prototype chain
				return AvmCore::atomToScriptObject(obj)->getMultinameProperty(multiname);
			}
			else
			{
				// primitive types are not dynamic, so we can go directly
				// to their __proto__ object.  but they are sealed, so fail if
				// the property is not found on the proto chain.

				ScriptObject* delegate = toPrototype(obj);
				if (delegate->isValidDynamicName(multiname))
				{
					return delegate->ScriptObject::getStringPropertyFromProtoChain(multiname->getName(), delegate, toTraits(obj));
				}
				else
				{
					throwReferenceError(kReadSealedError, multiname, toTraits(obj));
					return undefinedAtom;
				}
			}

		case BIND_GET:
		case BIND_GETSET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getproperty getter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// Invoke the getter
			int m = AvmCore::bindingToGetterId(b);
			MethodEnv *f = vtable->methods[m];
			Atom atomv_out[1] = { obj };
			return f->coerceEnter(0, atomv_out);
		}
		case BIND_SET:
		{
			#ifdef DEBUG_EARLY_BINDING
			core->console << "getproperty setter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// read on write-only property
			throwReferenceError(kWriteOnlyError, multiname, vtable->traits);
		}

		default:
	        // internal error
			AvmAssert(false);
			return undefinedAtom;
        }
    }

    void Toplevel::setproperty(Atom obj, Multiname* multiname, Atom value, VTable* vtable) const
    {
		Binding b = getBinding(vtable->traits, multiname);
		setproperty_b(obj,multiname,value,vtable,b);
	}

	void Toplevel::setproperty_b(Atom obj, Multiname* multiname, Atom value, VTable* vtable, Binding b) const
	{
        switch (b&7)
        {
		case BIND_METHOD: 
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setproperty method " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			if (multiname->contains(core()->publicNamespace) && isXmlBase(obj))
			{
				// dynamic props should hide declared methods
				ScriptObject* so = AvmCore::atomToScriptObject(obj);
				so->setMultinameProperty(multiname, value);
				return;
			}
			// trying to assign to a method.  error.
			throwReferenceError(kCannotAssignToMethodError, multiname, vtable->traits);
		}

		case BIND_CONST:
		{
			// OP_setproperty can never set a const.  initproperty must be used
			throwReferenceError(kConstWriteError, multiname, vtable->traits);
			return;
		}
		case BIND_VAR:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setproperty slot " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			int slot = AvmCore::bindingToSlotId(b);
			AvmCore::atomToScriptObject(obj)->setSlotAtom(slot, 
				coerce(value, vtable->traits->getSlotTraits(slot)));
            return;
		}

		case BIND_SET: 
		case BIND_GETSET: 
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setproperty setter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			// Invoke the setter
			uint32 m = AvmCore::bindingToSetterId(b);
			AvmAssert(m < vtable->traits->methodCount);
			MethodEnv* method = vtable->methods[m];
			Atom atomv_out[2] = { obj, value };
			method->coerceEnter(1, atomv_out);
			return;
		}
		case BIND_GET: 
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setproperty getter " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			throwReferenceError(kConstWriteError, multiname, vtable->traits);
			return;
		}

		case BIND_NONE:
		{
			#ifdef DEBUG_EARLY_BINDING
			core()->console << "setproperty dynamic " << vtable->traits << " " << multiname->getName() << "\n";
			#endif
			if (AvmCore::isObject(obj))
			{
				AvmCore::atomToScriptObject(obj)->setMultinameProperty(multiname, value);
			}
			else
			{
				// obj represents a primitive Number, Boolean, int, or String, and primitives
				// are sealed and final.  Cannot add dynamic vars to them.

				// property could not be found and created.
				throwReferenceError(kWriteSealedError, multiname, vtable->traits);
			}
			return;
		}

		default:
			// internal error if we get here
			AvmAssert(false);
        }
    }

	// E4X 12.1.1, pg 59
	Namespace* Toplevel::getDefaultNamespace()
	{
		// Walking the scope chain now would require a pointer into the local
		// variable space of the currently executing function.  Instead we save/
		// restore the defaultNamespace location as we enter/leave methods, so we
		// always can get to the current value.
		AvmCore* core = this->core();
#ifdef _DEBUG
		AvmAssert(!core->dxnsAddr || (uintptr)(*core->dxnsAddr) != 0xcccccccc);
#endif
		if (!core->dxnsAddr || !(*core->dxnsAddr))
			throwTypeError(kNoDefaultNamespaceError);
		return *core->dxnsAddr;
	}

	/**
	 * find the binding for a property given a full multiname reference.  The lookup
	 * must produce a single binding, or it's an error.  Note that the name could be
	 * bound to the same binding in multiple namespaces.
	 */
	Binding Toplevel::getBinding(Traits* traits, Multiname* ref) const
	{
		Atom b = BIND_NONE;
		if (traits && ref->isBinding())
		{
			if (!ref->isNsset())
			{
				b = traits->findBinding(ref->getName(), ref->getNamespace());
			}
			else
			{
				b = traits->findBinding(ref->getName(), ref->getNsset());
				if (b == BIND_AMBIGUOUS)
				{
					// ERROR.  more than one binding is available.  throw exception.
					throwTypeError(
									kAmbiguousBindingError, core()->toErrorString(ref));
				}
			}
		}
		return b;
	}

	Stringp Toplevel::decodeURI(Stringp uri)
	{
		AvmCore* core = this->core();
		if (!uri) uri = core->knull;
		Stringp out = decode(uri, false);
		if (!out) {
			toplevel()->uriErrorClass()->throwError(kInvalidURIError, core->toErrorString("decodeURI"));
		}
		return out;
    }

	Stringp Toplevel::decodeURIComponent(Stringp uri)
	{
		AvmCore* core = this->core();
		if (!uri) uri = core->knull;
		Stringp out = decode(uri, true);
		if (!out) {
			toplevel()->uriErrorClass()->throwError(kInvalidURIError, core->toErrorString("decodeURIComponent"));
		}
		return out;
    }

	Stringp Toplevel::encodeURI(Stringp uri)
	{
		AvmCore* core = this->core();
		if (!uri) uri = core->knull;
		Stringp out = encode(uri, false);
		if (!out) {
			toplevel()->uriErrorClass()->throwError(kInvalidURIError, core->toErrorString("encodeURI"));
		}
		return out;
    }

	Stringp Toplevel::encodeURIComponent(Stringp uri)
	{
		AvmCore* core = this->core();
		if (!uri) uri = core->knull;
		Stringp out = encode(uri, true);
		if (!out) {
			toplevel()->uriErrorClass()->throwError(kInvalidURIError, core->toErrorString("encodeURIComponent"));
		}
		return out;
    }
	
	bool Toplevel::isNaN(double n)
	{
        return MathUtils::isNaN(n);
    }

	/**
	 * isFinite(number) in section 15.1.2.5 of ecma 262 edition 3
	 *
	 * Applies ToNumber to argument then returns false if the result is NaN, Negative
	 * Infinity, or Positive Infinity, and true otherwise
	 *
	 * Special case - if isFinite is called with no args, should return false based on 
	 * section 10.1.3 that states if a function is called with less arguments then
	 * params, the variables get assigned 'undefined'.  ToNumber(undefined) returns NaN
	 * 
	 * @return true if arg is Finite, false otherwise
	 */
	bool Toplevel::isFinite(double d)
	{
		return !(MathUtils::isInfinite(d)||MathUtils::isNaN(d));
    }

	double Toplevel::parseInt(Stringp in, int radix)
	{
		AvmCore* core = this->core();
		if (!in) in = core->knull;
		const wchar *ptr = in->c_str();
		return MathUtils::parseInt(ptr, in->length(), radix, false);
    }

	double Toplevel::parseFloat(Stringp in)
	{
		double result;
		
		AvmCore* core = this->core();
		if (!in) in = core->knull;
		if (!MathUtils::convertStringToDouble(in->c_str(), in->length(), &result, false)) {
			result = MathUtils::nan();
		}
		
		return result;
    }

	Stringp Toplevel::escape(Stringp in)
	{
		AvmCore* core = this->core();

		if (!in) in = core->knull;

		const wchar *str = in->c_str();
		StringBuffer buffer(core);
		
		for (int i=0, n=in->length(); i<n; i++) {
			wchar ch = str[i];
			if (contains(unescaped, ch)) {
				buffer << ch;
			} else if (ch & 0xff00) {
				buffer << "%u";
				buffer.writeHexWord(ch);
			} else {
				buffer << '%';
				buffer.writeHexByte((uint8)ch);
			}
		}

		return core->newString(buffer.c_str());
    }

	Stringp Toplevel::escapeBytes(Stringp input)
	{
		AvmCore* core = this->core();

		UTF8String* inputUTF8 = input->toUTF8String();
		const uint8* src = (const uint8*) inputUTF8->c_str();

		StringBuffer buffer(core);
		
		for (int i=0, n=inputUTF8->length(); i<n; i++) {
			uint8 ch = src[i];
			if (contains(unescaped, ch)) {
				buffer << (wchar)ch;
			} else {
				buffer << '%';
				buffer.writeHexByte((uint8)ch);
			}
		}

		return core->newString(buffer.c_str());
    }
	
	// Helper function.
	int Toplevel::parseHexChar(wchar c)
	{
		if ('0' <= c && c <= '9') {
			return c-'0';
		}
		if ('A' <= c && c <= 'F') {
			return c-'A'+10;
		}
		if ('a' <= c && c <= 'f') {
			return c-'a'+10;
		}
		return -1;
	}

	wchar Toplevel::extractCharacter(const wchar*& src)
	{
		if (*src == '%') {
			const wchar *ptr = src;
			ptr++;
			if (*ptr == 0) {
				return *src++;
			}
			wchar value = 0;
			int len = 2;
			if (*ptr == 'u') {
				len = 4;
				ptr++;
			}
			for (int i=0; i<len; i++) {
				int v = parseHexChar(*ptr++);
				if (v < 0) {
					return *src++;
				}
				value = (wchar)((value<<4) | v);
			}
			src = ptr;
			return value;
		}
		return *src++;
	}
	
	Stringp Toplevel::unescape(Stringp in)
	{
		AvmCore* core = this->core();

		if (!in) in = core->knull;

		Stringp out = new (core->GetGC()) String(in->length());

		const wchar *src = in->c_str();
		wchar *outbuf = out->lockBuffer();
		wchar *dst = outbuf;
		const wchar *end = src + in->length();
		while (src < end) {
			*dst++ = extractCharacter(src);
		}
		*dst = 0;
		out->unlockBuffer(dst-outbuf);
		
		return out;
    }
	
	Stringp Toplevel::encode(Stringp in, bool encodeURIComponentFlag)
	{
		StringBuffer out(core());

		const wchar *src = in->c_str();
		int len = in->length();

		while (len--) {
			wchar ch = *src;

			if (contains(uriUnescaped, ch) ||
				(!encodeURIComponentFlag &&
				 contains(uriReservedPlusPound, ch)))
			{
				out << (char)ch;
				src++;
			} else {
				if (ch >= 0xDC00 && ch <= 0xDFFF) {
					return NULL;
				}
				uint32 V;
				if (ch >= 0xD800 && ch < 0xDC00) {
					if (src[1] < 0xDC00 || src[1] > 0xDFFF) {
						return NULL;
					}
					V = (ch - 0xD800) * 0x400 + (src[1] - 0xDC00) * 0x10000;
					src += 2;
				} else {
					V = ch;
					src++;
				}
				uint8 Octets[6];
				int OctetsLen = UnicodeUtils::Ucs4ToUtf8(V, Octets);
				if (!OctetsLen) {
					return NULL;
				}
				for (int i=0; i<OctetsLen; i++) {
					out << '%';
					out.writeHexByte(Octets[i]);
				}
			}
		}

		return core()->newString(out.c_str());
	}
	
	Stringp Toplevel::decode(Stringp in, bool decodeURIComponentFlag)
	{
		const wchar *chars = in->c_str();
		int length = in->length();
		wchar *out = (wchar*) core()->GetGC()->Alloc(length*2+1); // decoded result is at most length wchar chars long
		int outLen = 0;

		for (int k = 0; k < length; k++) {
			wchar C = chars[k];
			if (C == '%') {
				int start = k;
				if ((k + 2) >= length) {
					return NULL;
				}
				int v1 = parseHexChar(chars[k+1]);
				if (v1 == -1) {
					return NULL;
				}
				int v2 = parseHexChar(chars[k+2]);
				if (v2 == -1) {
					return NULL;
				}
				k += 2;
				uint8 B = (uint8)((v1<<4) | v2);
				uint32 V;
				if (!(B & 0x80)) {
					V = (wchar)B;
				} else {
					// 13. Let n be the smallest non-negative number
					//     such that (B << n) & 0x80 is equal to 0.
					int n = 1;
					while (((B<<n) & 0x80) != 0) {
						n++;
					}

					// 14. If n equals 1 or n is greater than 4,
					//     throw a URIError exception.
					if (n == 1 || n > 4) {
						return NULL;
					}
					uint8 Octets[4];
					Octets[0] = B;
					if (k + 3*(n-1) >= length) {
						return NULL;
					}
					for (int j=1; j<n; j++) {
						k++;
						if (chars[k] != '%') {
							return NULL;
						}
						int v1 = parseHexChar(chars[k+1]);
						if (v1 == -1) {
							return NULL;
						}
						int v2 = parseHexChar(chars[k+2]);
						if (v2 == -1) {
							return NULL;
						}
						B = (uint8)((v1<<4) | v2);
						
						// 23. If the two most significant bits
						//     in B are not 10, throw a URIError exception.
						if ((B&0xC0) != 0x80) {
							return NULL;
						}
						
						k += 2;
						Octets[j] = B;
					}

					// 29. Let V be the value obtained by applying the UTF-8 transformation
					// to Octets, that is, from an array of octets into a 32-bit value.
					if (!UnicodeUtils::Utf8ToUcs4(Octets, n, &V)) {
						return NULL;
					}
				}

				if (V < 0x10000) {
					// Check for reserved set
					if (!decodeURIComponentFlag) {
						if (contains(uriReservedPlusPound, V)) {
							while (start <= k) {
								out[outLen++] = chars[start++];
							}
						} else {
							out[outLen++] = (wchar)V;
						}
					} else {
						out[outLen++] = (wchar)V;
					}
				} else {
					// 31. If V is greater than 0x10FFFF, throw a URIError exception.
					if (V > 0x10FFFF) {
						return NULL;
					}
					// 32. Let L be (((V - 0x10000) & 0x3FF) + 0xDC00).
					// 33. Let H be ((((V - 0x10000) >> 10) & 0x3FF) + 0xD800).						
					uint32 L = (((V - 0x10000) & 0x3FF) + 0xDC00);
					uint32 H = ((((V - 0x10000) >> 10) & 0x3FF) + 0xD800);
					out[outLen++] = (wchar)H;
					out[outLen++] = (wchar)L;
				}
			} else {
				out[outLen++] = C;
			}
		}
		
		out[outLen] = 0;
		return new (gc()) String(out,outLen);
	}

	/*
	 * uriUnescaped is defined in Section 15.1 of the ECMA-262 specification
	 */
	const uint32 Toplevel::uriUnescaped[] = {
		0x00000000,
		0x03ff6782,
		0x87fffffe,
		0x47fffffe
	};
	
	/*
	 * uriReserved is defined in Section 15.1 of the ECMA-262 specification
	 * The '#' sign is added in accordance with the definition of
	 * the encodeURI/decodeURI functions
	 */
	const uint32 Toplevel::uriReservedPlusPound[] = {
		0x00000000,
		0xac009858,
		0x00000001,
		0x00000000
	};

	/**
	 * unescaped is a bitmap representing the set of 69 nonblank
	 * characters defined in ECMA-262 Section B.2.1 for the
	 * escape top-level function
	 */
	const uint32 Toplevel::unescaped[] = {
		0x00000000,
		0x03ffec00,
		0x87ffffff,
		0x07fffffe
	};

	bool Toplevel::isXMLName(Atom v)
	{
		return core()->isXMLName(v);
	}

	unsigned int Toplevel::readU30(const byte *&p) const
	{
		unsigned int result = AvmCore::readU30(p);
		if (result & 0xc0000000)
			throwVerifyError(kCorruptABCError);
		return result;
	}

	void Toplevel::throwVerifyError(int id) const
	{
		verifyErrorClass()->throwError(id);
	}

#ifdef DEBUGGER
	void Toplevel::throwVerifyError(int id, Stringp arg1) const
	{
		verifyErrorClass()->throwError(id, arg1);
	}

	void Toplevel::throwVerifyError(int id, Stringp arg1, Stringp arg2) const
	{
		verifyErrorClass()->throwError(id, arg1, arg2);
	}
#endif

	void Toplevel::throwTypeError(int id) const
	{
		typeErrorClass()->throwError(id);
	}

	void Toplevel::throwTypeError(int id, Stringp arg1) const
	{
		typeErrorClass()->throwError(id, arg1);
	}

	void Toplevel::throwTypeError(int id, Stringp arg1, Stringp arg2) const
	{
		typeErrorClass()->throwError(id, arg1, arg2);
	}

	void Toplevel::throwError(int id) const
	{
		errorClass()->throwError(id);
	}

	void Toplevel::throwError(int id, Stringp arg1) const
	{
		errorClass()->throwError(id, arg1);
	}

	void Toplevel::throwError(int id, Stringp arg1, Stringp arg2) const
	{
		errorClass()->throwError(id, arg1, arg2);
	}

	void Toplevel::throwArgumentError(int id) const
	{
		argumentErrorClass()->throwError(id);
	}

	void Toplevel::throwArgumentError(int id, Stringp arg1) const
	{
		argumentErrorClass()->throwError(id, arg1);
	}

	void Toplevel::throwArgumentError(int id, const char *s) const
	{
		argumentErrorClass()->throwError(id, core()->toErrorString(s));
	}

	void Toplevel::throwArgumentError(int id, Stringp arg1, Stringp arg2) const
	{
		argumentErrorClass()->throwError(id, arg1, arg2);
	}

	void Toplevel::throwRangeError(int id) const
	{
		rangeErrorClass()->throwError(id);
	}

	void Toplevel::throwRangeError(int id, Stringp arg1) const
	{
		rangeErrorClass()->throwError(id, arg1);
	}

	void Toplevel::throwRangeError(int id, Stringp arg1, Stringp arg2, Stringp arg3) const
	{
		rangeErrorClass()->throwError(id, arg1, arg2, arg3);
	}

	void Toplevel::throwReferenceError(int id, Multiname* multiname, const Traits* traits) const
	{
		referenceErrorClass()->throwError(id, core()->toErrorString(multiname), core()->toErrorString((Traits*)traits));
	}

	void Toplevel::throwReferenceError(int id, Multiname* multiname) const
	{
		referenceErrorClass()->throwError(id, core()->toErrorString(multiname));
	}
}
