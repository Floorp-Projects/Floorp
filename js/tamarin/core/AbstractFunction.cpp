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
	using namespace MMgc;

	AbstractFunction::AbstractFunction()
	{
		this->flags = 0;
		this->method_id = -1;
	}

	void AbstractFunction::initParamTypes(int count)
	{
		MMGC_MEM_TYPE(this);
		m_types = (Traits**)core()->GetGC()->Calloc(count, sizeof(Traits*), GC::kContainsPointers|GC::kZero);
	}

	void AbstractFunction::initDefaultValues(int count)
	{
		MMGC_MEM_TYPE(this);
		m_values = (Atom*)core()->GetGC()->Calloc(count, sizeof(Atom), GC::kContainsPointers|GC::kZero);
	}
	
	void AbstractFunction::setParamType(int index, Traits* t)
	{
		AvmAssert(index >= 0 && index <= param_count);
		WB(core()->GetGC(), m_types, &m_types[index], t);
	}

	void AbstractFunction::setDefaultValue(int index, Atom value)
	{
		AvmAssert(index > (param_count-optional_count) && index <= param_count);
		int i = index-(param_count-optional_count)-1;
		AvmAssert(i >= 0 && i < optional_count);
		WBATOM(core()->GetGC(), m_values, &m_values[i], value);
	}
	
	#ifdef AVMPLUS_VERBOSE
	Stringp AbstractFunction::format(const AvmCore* core) const
	{
		return core->concatStrings(name ? (Stringp)name : core->newString("?"),
			core->newString("()"));
	}
	#endif //AVMPLUS_VERBOSE

	bool AbstractFunction::makeMethodOf(Traits* traits)
	{
		if (!m_types[0])
		{
			declaringTraits = traits;
			setParamType(0, traits);
			flags |= NEED_CLOSURE;

			if (traits->final)
			{
				// all methods of a final class are final
				flags |= FINAL;
			}

			return true;
		}
		else
		{
			#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
				core()->console << "WARNING: method " << this << " was already bound to " << declaringTraits << "\n";
			#endif

			return false;
		}
	}

	void AbstractFunction::makeIntoPrototypeFunction(const Toplevel* toplevel)
	{
		if (declaringTraits == NULL)
		{
			// make sure param & return types are fully resolved.
			// this does not set the verified flag, so real verification will
			// still happen before the function runs the first time.
			resolveSignature(toplevel);

			// ftraits = new traits, extends function
			// this->declaringTraits = ftraits
			// ftraits->call = this
			// ftraits->construct = create object, call this(), return obj or result

			AvmCore* core = this->core();
			int functionSlotCount = core->traits.function_itraits->slotCount;
			int functionMethodCount = core->traits.function_itraits->methodCount;

			// type of F is synthetic subclass of Function, with a unique
			// [[call]] property and a unique scope

			Traits* ftraits = core->newTraits(core->traits.function_itraits, 1, 1,
									  sizeof(ClassClosure));
			ftraits->slotCount = functionSlotCount;
			ftraits->methodCount = functionMethodCount;
			ftraits->pool = pool;
			ftraits->final = true;
			ftraits->needsHashtable = true;
			ftraits->itraits = core->traits.object_itraits;
			this->declaringTraits = ftraits;

			ftraits->ns = core->publicNamespace;
#ifdef AVMPLUS_VERBOSE
			ftraits->name = core->internString(
					core->concatStrings(core->newString("Function-"), core->intToString(method_id))
				);
#endif

			ftraits->hashTableOffset = ftraits->sizeofInstance; 
			ftraits->setTotalSize(ftraits->hashTableOffset + sizeof(Hashtable));

			ftraits->initTables();

			AvmAssert(core->traits.function_itraits->linked);
			ftraits->linked = true;
			

#ifdef AVMPLUS_UNCHECKED_HACK
			// HACK - compiler should do this, and only to toplevel functions
			// that meet the E4 criteria for being an "unchecked function"
			// the tests below are too loose

			// if all params and return types are Object then make all params optional=undefined
			if (param_count == 0)
				flags |= IGNORE_REST;
			if (!(flags & HAS_OPTIONAL) && param_count > 0)
			{
				if (m_returnType == NULL)
				{
					for (int i=1; i <= param_count; i++)
					{
						if (m_types[i] != NULL)
							return;
					}

					// make this an unchecked function
					flags |= HAS_OPTIONAL | IGNORE_REST;
					optional_count = param_count;
					initDefaultValues(optional_count);
					for (int i=1; i <= optional_count; i++)
					{
						// since the type is object the default value is undefined.
						setDefaultValue(i, undefinedAtom);
					}
				}
			}
#endif
		}
	}
	
	/**
	 * convert native args to atoms.  argc is the number of
	 * args, not counting the instance which is arg[0].  the
	 * layout is [instance][arg1..argN]
	 */
	void AbstractFunction::boxArgs(int argc, uint32 *ap, Atom* out)
	{
		// box the typed args, up to param_count
		AvmCore* core = this->core();
		for (int i=0; i <= argc; i++)
		{
			if (i <= param_count)
			{
				Traits* t = paramTraits(i);
				AvmAssert(t != VOID_TYPE);

				if (t == NUMBER_TYPE) 
				{
					out[i] =  core->doubleToAtom(*(double *)ap);
					ap += 2;
				}
				else if (t == INT_TYPE)
				{
					out[i] = core->intToAtom((int32)*(Atom*) ap);
					ap += sizeof(Atom) / sizeof(uint32);
				}
				else if (t == UINT_TYPE)
				{
					out[i] = core->uintToAtom((uint32)*(Atom*) ap);
					ap += sizeof(Atom) / sizeof(uint32);
				}
				else if (t == BOOLEAN_TYPE)
				{
					out[i] = (*(Atom*) ap) ? trueAtom : falseAtom;
					ap += sizeof(Atom) / sizeof(uint32);
				}
				else if (!t || t == OBJECT_TYPE)
				{
					out[i] = *(Atom *) ap;
					ap += sizeof(Atom) / sizeof(uint32);
				}
				else
				{
					// it's a pointer type, possibly null

					void* p = *(void **)ap; // unknown pointer
					if (t == STRING_TYPE)
					{
						out[i] = ((Stringp)p)->atom();
					}
					else if (t == NAMESPACE_TYPE)
					{
						out[i] = ((Namespace*)p)->atom();
					}
					else 
					{
						out[i] = ((ScriptObject*)p)->atom();
					}
					ap += sizeof(void *) / sizeof(uint32);
				}
			}
			else
			{
				out[i] = *(Atom *) ap;
				ap += sizeof(Atom) / sizeof(uint32);
			}
		}
	}

	void AbstractFunction::resolveSignature(const Toplevel* toplevel)
	{
		if (!(flags & LINKED))
		{
			AvmCore* core = this->core();
			AvmAssert(info_pos != NULL);

			const byte* pos = info_pos;

			Traits* t = pool->resolveTypeName(pos, toplevel, /*allowVoid=*/true);

			m_returnType = t;

			restOffset = 0;

			// param 0 is contextual
			t = m_types[0];
			if (!t)
			{
				setParamType(0, OBJECT_TYPE);
				restOffset += sizeof(Atom);
			}
			else
			{
				if (t == NUMBER_TYPE)
					restOffset += sizeof(double);
				else
					restOffset += sizeof(Atom);
				if (t->isInterface)
					flags |= ABSTRACT_METHOD;
			}

			// param types 1..N come from abc stream
			for (int i=1, n=param_count; i <= n; i++)
			{
				t = pool->resolveTypeName(pos, toplevel);
				setParamType(i, t);
				if (t == NUMBER_TYPE)
					restOffset += sizeof(double);
				else
					restOffset += sizeof(Atom);
			}

			AvmCore::readU30(pos); // name_index;
			pos++; // skip flags

			if (flags & HAS_OPTIONAL)
			{
				AvmCore::readU30(pos); // optional_count

				initDefaultValues(optional_count);

				for (int j=0,n=optional_count; j < n; j++)
				{
					int param = param_count-optional_count+1+j;
					int index = AvmCore::readU30(pos);
					CPoolKind kind = (CPoolKind)*(pos++);

					// check that the default value is legal for the param type
					Traits* t = this->paramTraits(param);
					AvmAssert(t != VOID_TYPE);

					Atom value;
					if (t == NULL)
					{
						value = !index ? undefinedAtom : pool->getDefaultValue(toplevel, index, kind, t);
					}
					else if (t == OBJECT_TYPE)
					{
						value = !index ? nullObjectAtom : pool->getDefaultValue(toplevel, index, kind, t);
						if (value == undefinedAtom)
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else if (t == NUMBER_TYPE)
					{
						value = !index ? core->kNaN : pool->getDefaultValue(toplevel, index, kind, t);
						if (!(AvmCore::isInteger(value)||AvmCore::isDouble(value)))
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else if (t == BOOLEAN_TYPE)
					{
						value = !index ? falseAtom : pool->getDefaultValue(toplevel, index, kind, t);
						if (!AvmCore::isBoolean(value))
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else if (t == UINT_TYPE)
					{
						value = !index ? 0|kIntegerType : pool->getDefaultValue(toplevel, index, kind, t);
						if (!AvmCore::isInteger(value) && !AvmCore::isDouble(value)) 
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
						double d = AvmCore::number_d(value);
						if (d != (uint32)d) 
						{								
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else if (t == INT_TYPE)
					{
						value = !index ? 0|kIntegerType : pool->getDefaultValue(toplevel, index, kind, t);
						if (!AvmCore::isInteger(value) && !AvmCore::isDouble(value)) 
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
						double d = AvmCore::number_d(value);
						if (d != (int)d)
						{								
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else if (t == STRING_TYPE)
					{
						value = !index ? nullStringAtom : pool->getDefaultValue(toplevel, index, kind, t);
						if (!(AvmCore::isNull(value) || AvmCore::isString(value)))
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else if (t == NAMESPACE_TYPE)
					{
						value = !index ? nullNsAtom : pool->getDefaultValue(toplevel, index, kind, t);
						if (!(AvmCore::isNull(value) || AvmCore::isNamespace(value)))
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					else
					{
						// any other type: only allow null default value
						value = !index ? nullObjectAtom : pool->getDefaultValue(toplevel, index, kind, t);
						if (!AvmCore::isNull(value))
						{
							Multiname qname(t->ns, t->name);
							toplevel->throwVerifyError(kIllegalDefaultValue, core->toErrorString(&qname));
						}
					}
					
					setDefaultValue(param, value);
				}
			}

			/*
			// Don't need this for anything yet, so no point in wasting time parsing it.  Here just so we don't
			// forget about it if we add any sections after this one, and need to skip past it.  
			if( flags & AbstractFunction::HAS_PARAM_NAMES)
			{
				// AVMPlus doesn't care about the param names, just skip past them
				for( int j = 0; j < param_count; ++j )
				{
					readU30(pos);
				}
			}
			*/
			flags |= LINKED;
		}
	}

#ifdef DEBUGGER
	uint32 AbstractFunction::size() const 
	{
		uint32 size = sizeof(AbstractFunction);
		size += param_count * 2 * sizeof(Atom);
		return size;
	}
#endif
}
