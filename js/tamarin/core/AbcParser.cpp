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
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
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
    /**
     * parse a .abc file completely
     * @param code
     * @return
     */
    PoolObject* AbcParser::decodeAbc(AvmCore* core, ScriptBuffer code, 
		Toplevel* toplevel,
		Domain* domain,
		AbstractFunction *nativeMethods[],
		NativeClassInfo *nativeClasses[],
		NativeScriptInfo *nativeScripts[])
    {
		if (code.getSize() < 4)
			toplevel->throwVerifyError(kCorruptABCError);

		int version = AvmCore::readU16(&code[0]) | AvmCore::readU16(&code[2])<<16;

		#ifdef AVMPLUS_PROFILE
		DynamicProfiler::StackMark mark(OP_decode, &core->dprof);
		#endif

		#ifdef AVMPLUS_VERBOSE
		if (core->verbose)
			core->console << "major=" << (version&0xFFFF) << " minor=" << (version>>16) << "\n";
		#endif

		switch (version)
		{
		case (46<<16|16):
		{
			AbcParser parser(core, code, toplevel, domain, nativeMethods, nativeClasses, nativeScripts);
			PoolObject *pObject = parser.parse();
 			if ( !pObject )
 				toplevel->throwVerifyError( kCorruptABCError );
 			else
 				return pObject;
		}
		default:
			toplevel->throwVerifyError(kInvalidMagicError, core->toErrorString(version>>16), core->toErrorString(version&0xFFFF));
			return NULL;
		}
    }

	AbcParser::AbcParser(AvmCore* core, ScriptBuffer code, 
		Toplevel* toplevel,
		Domain* domain,
		AbstractFunction *nativeMethods[],
		NativeClassInfo *nativeClasses[],
		NativeScriptInfo *nativeScripts[]) 
		: toplevel(toplevel),
		  domain(domain),
		  instances(core->GetGC(), 0)
	{
		this->core = core;
		this->code = code;
		this->pool = NULL;
		this->version = AvmCore::readU16(&code[0]) | AvmCore::readU16(&code[2])<<16;
		this->pos = &code[4];
		this->nativeMethods = nativeMethods;
		this->nativeClasses = nativeClasses;
		this->nativeScripts = nativeScripts;

		abcStart = &code[0];
		abcEnd = &code[(int)code.getSize()];

		classCount = 0;
		metaNames = NULL;
	}

	AbcParser::~AbcParser()
	{
		if (metaNames)
			core->GetGC()->Free(metaNames);
	}

	int AbcParser::computeInstanceSize(int class_id, 
									   Traits* base) const
	{
		// If this is a native class, return the stated instance size
		NativeClassInfo* nativeEntry;
		if (nativeClasses && (nativeEntry = nativeClasses[class_id]) != 0)
		{
			return nativeEntry->sizeofInstance;
		}

		// Search the inheritance chain for any native classes.
		while (base)
		{
			if (base->sizeofInstance > sizeof(ScriptObject))
			{
				// non-Object base class uses a subclass of ScriptObject, so use that size.
				return base->sizeofInstance;
			}
			base = base->base;
		}

		// no derived native classes found
		return sizeof(ScriptObject);
	}

	Namespace* AbcParser::parseNsRef(const byte* &pc) const
	{
		uint32 index = readU30(pc);
		if (index == 0)
		{
			return NULL; // AnyNamespace
		}

		if (index >= pool->constantNsCount)
			toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantNsCount));
		return pool->cpool_ns[index];
	}

	#ifdef AVMPLUS_VERBOSE
	void AbcParser::parseTypeName(const byte* &pc, Multiname& m) const
	{
		// only save the type name for now.  verifier will resolve to traits
		uint32 index = readU30(pc);
		if (index == 0)
		{
			// type is *
			m.setNamespace(core->publicNamespace);
			m.setName(core->kAsterisk);
			return;
		}

		if (index >= pool->constantMnCount)
			toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantMnCount));

		pool->parseMultiname(m, index);
	}
	#endif

	Atom AbcParser::resolveQName(const byte* &p, Multiname &m) const
	{
		uint32 index = readU30(p);
		if (index == 0 || index >= pool->constantMnCount)
			toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantCount));

		Atom a = pool->cpool_mn[index];

		pool->parseMultiname(a, m);
		if (!m.isQName())
			toplevel->throwVerifyError(kCpoolEntryWrongTypeError, core->toErrorString(index));

		return a;
	}

	AbstractFunction *AbcParser::resolveMethodInfo(uint32 index) const
	{
		if (index >= pool->methodCount)
			toplevel->throwVerifyError(kMethodInfoExceedsCountError, core->toErrorString(index), core->toErrorString(pool->methodCount));

		AbstractFunction *f = pool->getMethodInfo(index);
		if (!f)
			toplevel->throwVerifyError(kMethodInfoOrderError, core->toErrorString(index));

		return f;
	}

	Stringp AbcParser::resolveUtf8(uint32 index) const
	{
		if (index > 0 && index < pool->constantStringCount )
		{
			return pool->cpool_string[index];
		}
		toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantStringCount));
		return NULL;
	}

	Stringp AbcParser::parseName(const byte* &pc) const
	{
		uint32 index = readU30(pc);
		if (index == 0)
			return NULL;
		return resolveUtf8(index);
	}

	PoolObject* AbcParser::parse()
	{
#ifdef FEATURE_BUFFER_GUARD // no Carbon
		TRY(this->core, kCatchAction_Rethrow)
		{
			// catches any access violation exceptions and sends control to
			// the CATCH block below.
			BufferGuard guard(_ef.jmpbuf);
			this->guard = &guard;
			
#endif // FEATURE_BUFFER_GUARD

			// constant pool
			parseCpool();

			// parse all methodInfos in one pass.  Nested functions must come before outer functions
			parseMethodInfos();

			// parse all metadataInfos - AVM+ doesn't care about this, so we are really just skipping them
			parseMetadataInfos();

			// parse classes.  base classes must come first.
			if (!parseInstanceInfos()) return NULL;

			bool first = false;
			if (CLASS_TYPE == NULL)
			{
				// if we haven't got types from the builtin file yet, do it now.
				first = true;
				core->traits.initInstanceTypes(pool);

				// register "void"
				addNamedTraits(core->publicNamespace, VOID_TYPE->name, VOID_TYPE);

				// temporary: register "Void"
				addNamedTraits(core->publicNamespace, core->constantString("Void"), VOID_TYPE);
			}

			// type information about class objects
			parseClassInfos();

			if (first)
			{
				core->traits.initClassTypes(pool);
			}

			// scripts
			if( !parseScriptInfos() ) return NULL;

			// method bodies: code, exception info, and activation traits
			parseMethodBodies();
#ifdef FEATURE_BUFFER_GUARD // no buffer guard in Carbon builds
		}
		CATCH(Exception *exception)
		{
		 	guard->disable();
			guard = NULL;
			if( exception->isValid())
				// Re-throw if exception exists
				core->throwException(exception);
			else
				// create new exception
				toplevel->throwVerifyError(kCorruptABCError);
		}
		END_CATCH
		END_TRY
#endif // FEATURE_BUFFER_GUARD

        return pool;
	}

	/**
	 * setting up a traits that extends another one.  Two parser passes are required,
	 * one for sizing the traits object and the other for allocating it and filling
	 * it in.
	 */
	Traits* AbcParser::parseTraits(Traits* base, Namespace* ns, Stringp name, AbstractFunction* script, int interfaceDelta, Namespace* protectedNamespace /*=NULL*/)
	{
		const byte* traits_pos = pos;
		int nameCount = readU30(pos);

		#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
			core->console << "        trait_count=" << nameCount << "\n";
		#endif

		Traits* traits = core->newTraits(base, nameCount, interfaceDelta, sizeof(ScriptObject));

		traits->setTraitsPos(traits_pos);
		traits->ns = ns;
		traits->name = name;
		traits->pool = pool;
		traits->protectedNamespace = protectedNamespace;
		
		// Copy protected traits from base class into new protected namespace
		if (base && base->protectedNamespace && protectedNamespace)
		{
			for (int i=0; (i = base->next(i)) != 0; )
			{
				if (base->nsAt(i) == base->protectedNamespace)
				{
					traits->add(base->keyAt(i), protectedNamespace, base->valueAt(i));
				}
			}
		}

		uint32 slotCount, methodCount;
		if (base)
		{
			slotCount = base->slotCount;
			methodCount = base->methodCount;
		}
		else
		{
			slotCount = 0;
			methodCount = 0;
		}

		bool earlySlotBinding;
		
		pool->allowEarlyBinding(base, earlySlotBinding);

        for (int i=0; i < nameCount; i++)
        {
			Multiname qn;
			resolveQName(pos, qn);
			Namespace* ns = qn.getNamespace();
			// TODO name can come out null and fire all kinds of asserts from a broken compiler
			// and crash a release build, not sure if null is valid in any cases but there's definitely
			// a missing verify error here somewhere
			Stringp name = qn.getName();
#ifdef SAFE_PARSE
			// check to see if we are trying to read past the file end or the beginning.
			if (pos < abcStart || pos >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE
            int tag = *pos++;
            TraitKind kind = (TraitKind) (tag & 0x0f);
			
			AbstractFunction *f = NULL;
			switch (kind)
            {
            case TRAIT_Slot:
            case TRAIT_Const:
            case TRAIT_Class:
			case TRAIT_Function:
			{
                uint32 slot_id = readU30(pos);
				if (!earlySlotBinding) slot_id = 0;
				if (!slot_id)
				{
					// vm assigns slot
					slot_id = slotCount++;
				}
				else
				{
					// compiler assigned slot
					if (slot_id > slotCount)
						slotCount = slot_id;
					slot_id--;
				}

				if (base && slot_id < base->slotCount)
				{
					// slots are final.
					toplevel->throwVerifyError(kIllegalOverrideError, core->toErrorString(&qn), core->toErrorString(base));
				}

				// a slot cannot override anything else.
				if (traits->get(name, ns) != BIND_NONE)
					toplevel->throwVerifyError(kCorruptABCError);
				
				if (script)
					addNamedScript(ns, name, script);

				if( kind == TRAIT_Slot || kind == TRAIT_Const)
				{
					#ifdef AVMPLUS_VERBOSE
					// type name
					Multiname typeName;
					parseTypeName(pos, typeName);

					// default value index
					int value_index = readU30(pos);
					CPoolKind value_kind;
					if( value_index )
					{
#ifdef SAFE_PARSE
						// check to see if we are trying to read past the file end or the beginning.
						if (pos < abcStart || pos >= abcEnd )
							toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE
						value_kind = (CPoolKind)*(pos++);
					}
					if (pool->verbose)
					{
						core->console << "            " << traitNames[kind]
							<< " name=" << Multiname::format(core,ns,name)
							<< " slot_id=" << slot_id
							<< " value_index=" << value_index
							<< " type=" << &typeName
							<< "\n";
					}
					#else
					readU30(pos); // type
					int value_index = readU30(pos); // default value
					if( value_index )
					{
						pos++; // byte for the value kind
					}
					#endif

					// create a binding
					// only touches hashtable
					traits->defineSlot(name, ns, 
						slot_id, kind==TRAIT_Slot ? BIND_VAR : BIND_CONST);

				}
				else if (kind == TRAIT_Class)
				{
					// get the class type
					int class_info = readU30(pos);
					if (class_info >= classCount)
						toplevel->throwVerifyError(kClassInfoExceedsCountError, core->toErrorString(class_info), core->toErrorString(classCount));

					AbstractFunction* cinit = pool->cinits[class_info];
					if (!cinit) 
						toplevel->throwVerifyError(kClassInfoOrderError, core->toErrorString(class_info));

					Traits* ctraits = cinit->declaringTraits;

					#ifdef AVMPLUS_VERBOSE
					if (pool->verbose)
					{
						core->console << "            " << traitNames[kind]
							<< " name=" << Multiname::format(core, ns, name)
							<< " slot_id=" << slot_id
							<< " type=" << ctraits
							<< "\n";
					}
					#endif

					if (script)
					{
						// promote instance type to the vm-wide type table.
						Traits* itraits = ctraits->itraits;

						// map type name to traits
						addNamedTraits(ns, name, itraits);

						if ( tag & ATTR_metadata )
						{
							itraits->setMetadataPos(pos);
						}
					}
					else
					{
						if ( tag & ATTR_metadata )
						{
							ctraits->setMetadataPos(pos);
						}
					}

					// create a binding
					// only touches hashtable
					traits->defineSlot(name, ns, slot_id, BIND_CONST);

				}
				else
				{
					AvmAssert(kind == TRAIT_Function);
					uint32 method_info = readU30(pos); // method_info

					#ifdef AVMPLUS_VERBOSE
					if (pool->verbose)
					{
						core->console << "            " << traitNames[kind]
							<< " name=" << Multiname::format(core, ns, name)
							<< " slot_id=" << slot_id
							<< " method_info=" << method_info
							<< "\n";
					}
					#endif

					AbstractFunction* f = resolveMethodInfo(method_info);
					f->makeIntoPrototypeFunction(toplevel);

					// create binding
					traits->defineSlot(name, ns, slot_id, BIND_VAR);
				}
	            break;
			}
			case TRAIT_Getter:
			case TRAIT_Setter:
            case TRAIT_Method:
			{
				int earlyDispId = readU30( pos );
                (void)earlyDispId;
				uint32 method_info = readU30(pos);

				#ifdef AVMPLUS_VERBOSE
				if (pool->verbose)
				{
					core->console << "            " << traitNames[kind]
						<< " name=" << Multiname::format(core, ns, name)
						<< " disp_id=" << earlyDispId << " (ignored)"
					    << " method_info=" << method_info
						<< " attr=" << ((tag&ATTR_final)?"final":"virtual");
					if (tag&ATTR_override)
						core->console << "|override";
					core->console << "\n";
				}
				#endif

				f = resolveMethodInfo(method_info);

				#ifdef AVMPLUS_VERBOSE
				Stringp s1 = traits->format(core);
				Stringp s2 = core->newString(kind == TRAIT_Method ? "/" : (kind == TRAIT_Getter ? "/get " : "/set "));
				Stringp s3 = Multiname::format(core,ns,name);
				Stringp s4 = core->concatStrings(s2,s3);
				f->name = core->concatStrings(s1, s4);
				//delete s1 - can't delete, it may bet the traits name string;
				delete s2;
				//delete s3 - can't delete, it may be the qname name string;
				delete s4;
				#endif

				// since this function is ref'ed here, we know the receiver type.
				if (!f->makeMethodOf(traits))
				{
					toplevel->throwVerifyError(kCorruptABCError);
				}

				if (tag & ATTR_final)
					f->flags |= AbstractFunction::FINAL;

				if (tag & ATTR_override)
					f->flags |= AbstractFunction::OVERRIDE;
				
				Binding baseBinding = traits->getOverride(ns, name, tag, toplevel);

				// issue can you ever import a getter/setter?
				// is it okay to call this more than once on the same
				// name/script pair?

				// only export one name for an accessor 
				if (script && !domain->getNamedScript(name,ns))
					addNamedScript(ns, name, script);

				if (kind == TRAIT_Method)
				{
					if (baseBinding == BIND_NONE)
					{
						traits->defineSlot(name, ns, methodCount, BIND_METHOD);
						// accessors require 2 vtable slots, methods only need 1.
						methodCount ++;
					}
					else if (AvmCore::isMethodBinding(baseBinding))
					{
						// something got overridden, need new name entry for this subclass
						// but keep the existing disp_id
						int disp_id = AvmCore::bindingToMethodId(baseBinding);
						traits->defineSlot(name, ns, disp_id, BIND_METHOD);
					}
					else
					{
						toplevel->throwVerifyError(kCorruptABCError);
					}
				}
				else if (kind == TRAIT_Getter)
				{
					// if nothing in base class, look for existing binding on this class,
					// in case setter has already been defined.
					if (baseBinding == BIND_NONE)
						baseBinding = traits->get(name, ns);

					if (baseBinding == BIND_NONE)
					{
						traits->defineSlot(name, ns, methodCount, BIND_GET);
						// accessors require 2 vtable slots, methods only need 1.
						methodCount += 2;
					}
					else if (AvmCore::isAccessorBinding(baseBinding))
					{
						// something maybe got overridden, need new name entry for this subclass
						// but keep the existing disp_id
						int disp_id = urshift(baseBinding,3);
						int bkind = baseBinding & 7;
						if (bkind == BIND_SET)
							bkind = BIND_GETSET;
						traits->defineSlot(name, ns, disp_id, bkind);
					}
					else
					{
						toplevel->throwVerifyError(kCorruptABCError);
					}
				}
				else // TRAIT_Setter
				{
					// if nothing in base class, look for existing binding on this class,
					// in case getter has already been defined.
					if (baseBinding == BIND_NONE)
						baseBinding = traits->get(name, ns);

					if (baseBinding == BIND_NONE)
					{
						traits->defineSlot(name, ns, methodCount, BIND_SET);
						// accessors require 2 vtable slots, methods only need 1.
						methodCount += 2;
					}
					else if (AvmCore::isAccessorBinding(baseBinding))
					{
						// something maybe got overridden, need new name entry for this subclass
						// but keep the existing disp_id
						// both get & set bindings use the get id.  set_id = get_id + 1.
						int disp_id = urshift(baseBinding,3);
						int bkind = baseBinding & 7;
						if (bkind == BIND_GET) 
							bkind = BIND_GETSET;
						traits->defineSlot(name, ns, disp_id, bkind);
					}
					else
					{
						toplevel->throwVerifyError(kCorruptABCError);
					}
				}
                break;
			}

			default:
				// unsupported traits type
				toplevel->throwVerifyError(kUnsupportedTraitsKindError, core->toErrorString(kind));
            }

            if ( tag & ATTR_metadata )
            {
                int metadataCount = readU30(pos);
				for( int metadata = 0; metadata < metadataCount; ++metadata)
				{
					int index = readU30(pos);
					Stringp name = metaNames[index];
					if (name == core->kNeedsDxns && f != NULL)
						f->flags |= AbstractFunction::NEEDS_DXNS;
#ifdef AVMPLUS_VERBOSE
					if (name == kVerboseVerify && f != NULL)
						f->flags |= AbstractFunction::VERBOSE_VERIFY;
					if (pool->verbose)
						core->console << "            [" << metaNames[index] << "]\n";
#endif
				}
            }
        }

		traits->slotCount = slotCount;
		traits->methodCount = methodCount;
		return traits;
	}

	void AbcParser::parseMethodInfos()
    {
        int methodCount = readU30(pos);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
			core->console << "method_count=" << methodCount << "\n";
#endif

		int size = methodCount == 0 ? 1 : methodCount;

		if (size > (abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		MMGC_MEM_TYPE(pool);
		pool->methods.ensureCapacity(size);
		pool->methodCount = methodCount;

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		const byte* startpos = pos;
#endif

		for (int i=0; i < methodCount; i++)
        {
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
			// MethodInfo
			int param_count = readU30(pos);

			const byte* info_pos = pos;

			#ifdef AVMPLUS_VERBOSE
			Multiname returnTypeName;
			parseTypeName(pos, returnTypeName);
			if (pool->verbose)
			{
				core->console << "    " << offset << ":method["<<i<<"]\n"
					<< "        returnType=" << &returnTypeName << "\n"
					<< "        param_count=" << param_count 
				    << "\n";
			}
			#else
			readU30(pos);// return type name
			#endif

			for( int j = 1; j <= param_count; ++j)
			{
				#ifdef AVMPLUS_VERBOSE
				Multiname multiname;
				parseTypeName(pos, multiname);
				if (pool->verbose)
					core->console << "            paramType["<<j<<"]="<< &multiname << "\n";
				#else
				readU30(pos);
				#endif
			}

            uint32 name_index = readU30(pos);
            (void)name_index;
#ifdef SAFE_PARSE
			// check to see if we are trying to read past the file end or the beginning.
			if (pos < abcStart || pos >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE
			int flags = *pos++;

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "        name_index=" << name_index;
				if (name_index > 0 && name_index < pool->constantStringCount)
					core->console << " \"" << pool->cpool_string[name_index] << "\"";
				core->console << "\n        flags=" << flags << "\n";
			}
#endif

			AbstractFunction *info;			
			if (!(flags & AbstractFunction::NATIVE))
			{
				MethodInfo *methodInfo = new (core->GetGC()) MethodInfo();
				
				#ifdef AVMPLUS_VERBOSE
				if (name_index != 0)
					methodInfo->name = resolveUtf8(name_index);
				else
					methodInfo->name = core->concatStrings(core->newString("MethodInfo-"), core->intToString(i));
				#endif
				info = methodInfo;
			}
			else
			{
				if (!nativeMethods)
				{
					toplevel->throwVerifyError(kIllegalNativeMethodError);
				}
				info = nativeMethods[i];

				// If this assert hits, you're missing a native method.  Method "i"
				// corresponds to the function of the same number in 
				// source\avmglue\avmglue.h if you're running the Flash player.
				AvmAssertMsg(info != 0, "missing native method decl");

				// todo assert that the .abc signature matches the C++ code?
				info->flags |= AbstractFunction::ABSTRACT_METHOD;
			}

			info->info_pos = info_pos;
			info->pool = pool;
			info->method_id = i;
			info->param_count = param_count;
			info->initParamTypes(param_count+1);
			info->flags |= flags;

			if (flags & AbstractFunction::HAS_OPTIONAL)
			{
				int optional_count = readU30(pos);

				info->optional_count = optional_count;
				for( int j = 0; j < optional_count; ++j)
				{
					readU30(pos);
					++pos; // Kind bytes for each default value
				}

				if (optional_count > param_count)
				{
					// cannot have more optional params than total params
					toplevel->throwVerifyError(kCorruptABCError);
				}
			}

			if( flags & AbstractFunction::HAS_PARAM_NAMES)
			{
				// AVMPlus doesn't care about the param names, just skip past them
				for( int j = 0; j < info->param_count; ++j )
				{
					readU30(pos);
				}
			}

			// save method info pointer.  we will verify code later.
			pool->methods.set(i, info);
		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.methodsSize += (pos-startpos);
		#endif
    }
	

	void AbcParser::parseMetadataInfos()
    {
        int metadataCount = readU30(pos);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
			core->console << "metadata_count=" << metadataCount << "\n";
#endif

		if (metadataCount > (abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		pool->metadata_infos.ensureCapacity(metadataCount);
		pool->metadataCount = metadataCount;

		if (metadataCount > 0)
		{
			metaNames = (Stringp*) core->GetGC()->Calloc(metadataCount, sizeof(Stringp), MMgc::GC::kContainsPointers);

#ifdef AVMPLUS_VERBOSE
			kVerboseVerify = core->constantString("VerboseVerify");
#endif

			for (int i=0; i < metadataCount; i++)
			{
				pool->metadata_infos.set(i, pos);
				// MetadataInfo
				uint32 index = readU30(pos);
				Stringp name = resolveUtf8(index);
				// constant pool names are stuck and always reachable via PoolObject, DRC or WB
				metaNames[i] = name;

#ifdef AVMPLUS_VERBOSE
				if (pool->verbose) 
					core->console << "    " << name;
				int values_count = readU30(pos);
				if (values_count > 0)
				{
					if (pool->verbose) 
						core->console << "(";
					for(int q = 0; q < values_count; ++q)
					{
						int a = readU30(pos);
						int b = readU30(pos);
						if (pool->verbose) 
						{
							core->console << a << "," << b;
							if (q+1 < values_count)
								core->console << " ";
						}
					}
					if (pool->verbose)
						core->console << ")";
				}
				if (pool->verbose)
					core->console << "\n";
				
#else	// AVMPLUS_VERBOSE
				
				// MetadataInfo
				int values_count = readU30(pos);
				for(int q = 0; q < values_count; ++q)
				{
					readU30(pos);
					readU30(pos);
				}

#endif  // AVMPLUS_VERBOSE
			}
		}

#ifdef AVMPLUS_VERBOSE
//StaticProfiler::methodsSize += (pos-startpos);
#endif
    }

    void AbcParser::parseMethodBodies()
    {
        int bodyCount = readU30(pos);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
			core->console << "bodies_count=" << bodyCount << "\n";
#endif

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		const byte* startpos = pos;
#endif

		for (int i=0; i < bodyCount; i++)
        {
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif

			uint32 method_info = readU30(pos);
			AbstractFunction* info = resolveMethodInfo(method_info);

			const byte *body_pos = pos;

            int max_stack = readU30(pos);
            (void)max_stack;
            
            int local_count = readU30(pos);

			if (local_count < info->param_count+1)
			{
				// must have enough locals to hold all parameters including this
				toplevel->throwVerifyError(kCorruptABCError);
			}

			// TODO change file format, just want local max_scope
			int init_scope_depth = readU30(pos);
			(void)init_scope_depth;
			
			int max_scope_depth = readU30(pos);
			(void)max_scope_depth;

			int code_length = readU30(pos);


			if (code_length <= 0) 
			{
				toplevel->throwVerifyError(kInvalidCodeLengthError, core->toErrorString(code_length));
			}

			// check to see if we are trying to jump past the file end or the beginning.
			if ( pos < abcStart || pos+code_length >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
            pos += code_length;

            int exception_count = readU30(pos);

			#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":method["<<method_info<<"] max_stack=" << max_stack
					<< " local_count=" << local_count 
					<< " init_scope_depth=" << init_scope_depth 
					<< " max_scope_depth=" << max_scope_depth
					<< " code_length=" << code_length
					<< " exception_count=" << exception_count
				    << "\n";
			}
			#endif

			if (exception_count != 0) 
			{
				info->flags |= AbstractFunction::HAS_EXCEPTIONS;
				for (int i=0; i<exception_count; i++) 
				{
					// this will be parsed when method is verified.
					// see AbstractFunction::resolveSignature

					#ifdef AVMPLUS_VERBOSE
					int from = readU30(pos);
					int to = readU30(pos);
					int target = readU30(pos);
					Multiname typeName;
					parseTypeName(pos, typeName);

					Multiname qn;
					uint32 name_index = (version != (46<<16|15)) ? readU30(pos) : 0;
					if (name_index != 0)
					{
						if (name_index >= pool->constantMnCount)
							toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(name_index), core->toErrorString(pool->constantCount));
						pool->parseMultiname(qn, name_index);
					}

					if (pool->verbose)
					{
						core->console << "            exception["<<i<<"] from="<< from
							<< " to=" << to
							<< " target=" << target 
							<< " type=" << &typeName
							<< " name=";
						if (name_index)
						{
							core->console << &qn;
						}
						else
						{
							core->console << "(none)";
						}
						core->console << "\n";
					}
					#else
					readU30(pos); // from
					readU30(pos); // to
					readU30(pos); // target
					readU30(pos); // type name
					if (version != (46<<16|15))
					{
						readU30(pos); // variable name
					}
					#endif
				}
			}

			if (info->hasMethodBody())
			{
				MethodInfo *methodInfo = (MethodInfo*) info;

				// Interface methods should not have bodies
				if (info->declaringTraits && info->declaringTraits->isInterface)
				{
					toplevel->throwVerifyError(kIllegalInterfaceMethodBodyError, core->toErrorString(info));
				}

#ifdef DEBUGGER
				methodInfo->local_count = local_count;
				methodInfo->codeSize = code_length;
				methodInfo->max_scopes = max_scope_depth - init_scope_depth;
#endif

				// if non-zero, we have a duplicate method body - throw a verify error
				if (methodInfo->body_pos)
				{
					toplevel->throwVerifyError(kDuplicateMethodBodyError, core->toErrorString(info));
				}

				methodInfo->body_pos = body_pos;

				// there will be a traits_count here, even if NEED_ACTIVATION is not
				// set.  So we parse the same way all the time.  We could reduce file size and
				// memory by ommitting the count + traits comletely.

				const byte* traits_pos = pos;
				int nameCount = readU30(pos);
				if ((methodInfo->flags & AbstractFunction::NEED_ACTIVATION) || nameCount > 0)
				{
					pos = traits_pos;
					// activation traits are raw types, not subclasses of object.  this is
					// okay because they aren't accessable to the programming model.
					#ifdef AVMPLUS_VERBOSE
					methodInfo->activationTraits = parseTraits(NULL, 
						core->publicNamespace, 
						core->internString(methodInfo->name),
						0, 0);
					#else
					methodInfo->activationTraits = parseTraits(NULL, NULL, NULL, false, 0);
					#endif
					methodInfo->activationTraits->final = true;
				}
				else
				{
					// TODO remove this assert once abc format is adjusted
					AvmAssert(nameCount == 0);
				}
			}
			else
			{
				// native methods should not have bodies!
				toplevel->throwVerifyError(kIllegalNativeMethodBodyError, core->toErrorString(info));
			}
		}

		#ifdef AVMPLUS_PROFILE
		core->sprof.bodiesSize += (pos-startpos);
		#endif
    }

    void AbcParser::parseCpool()
    {
		pool = new (core->GetGC()) PoolObject(core, code, pos);
		pool->domain = domain;
		pool->isBuiltin = (nativeMethods != NULL);

		uint32 int_count = readU30(pos);
		// sanity check to prevent huge allocations
		if (int_count > (uint32)(abcEnd - pos)) 
			toplevel->throwVerifyError(kCorruptABCError);

		List<int,LIST_NonGCObjects>& cpool_int = pool->cpool_int;
		cpool_int.ensureCapacity(int_count);
		pool->constantIntCount = int_count;

#ifdef AVMPLUS_VERBOSE
		pool->verbose = core->verbose;
#endif

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		const byte* startpos = pos;
#endif

		for(uint32 i = 1; i < int_count; ++i)
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
			// S32 value
			cpool_int.set(i, readS32(pos));
#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_int["<<i<<"]="
					<<constantNames[CONSTANT_Int] << " ";
				core->console << cpool_int[i] << "\n";
			}
#endif

		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolIntSize = (pos-startpos);
		#endif

		uint32 uint_count = readU30(pos);
		if (uint_count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		List<uint32,LIST_NonGCObjects>& cpool_uint = pool->cpool_uint;
		cpool_uint.ensureCapacity(uint_count);
		pool->constantUIntCount = uint_count;

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		startpos = pos;
#endif

		for(uint32 i = 1; i < uint_count; ++i)
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
			// U32 value
			cpool_uint.set(i, (unsigned)readS32(pos));

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_uint["<<i<<"]="
					<<constantNames[CONSTANT_UInt] << " ";
				core->console << (double)cpool_uint[i];
				core->console << "\n";
			}
#endif

		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolUIntSize = (pos-startpos);
		#endif

		uint32 double_count = readU30(pos);
		if (double_count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		List<double*, LIST_GCObjects>& cpool_double = pool->cpool_double;
		cpool_double.ensureCapacity(double_count);
		pool->constantDoubleCount = double_count;

#if defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		startpos = pos;
#endif

		for(uint32 i = 1; i < double_count; ++i)
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
			// double value
			union {
				double value;
				sint64 bits;
				#ifdef AVMPLUS_ARM
				uint32 words[2];
				#endif
			} u;
			u.bits = readS64(pos);
			#ifdef AVMPLUS_ARM
			uint32 t = u.words[0];
			u.words[0] = u.words[1];
			u.words[1] = t;
			#endif
			cpool_double.set(i, (double*)(core->allocDouble(u.value)&~7));
#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_double["<<i<<"]="
					<<constantNames[CONSTANT_Double] << " ";
				core->console << *cpool_double[i];
				core->console << "\n";
			}
#endif

		}

		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolDoubleSize = (pos-startpos);
		#endif

		uint32 string_count = readU30(pos);
		if (string_count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		List<Stringp, LIST_RCObjects> &cpool_string = pool->cpool_string;
		MMGC_MEM_TYPE(pool);
		cpool_string.ensureCapacity(string_count);
		pool->constantStringCount = string_count;

#if defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		startpos = pos;
#endif

		for(uint32 i = 1; i < string_count; ++i)
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif

			// number of characters
			// todo - is compiler emitting no. of chars or no. of bytes?
			int len = readU30(pos);

			// check to see if we are trying to read past the file end or the beginning.
			if (pos < abcStart || pos+len >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
			// don't need to create an atom for this now, because
			// each caller will take care of it.
			Stringp s = core->internAllocUtf8(pos, len);
#ifdef MMGC_DRC
			// MIR skips WB on string constants so make them sticky
			s->Stick();
#endif
			cpool_string.set(i, s);
			pos += len;

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_string["<<i<<"]="
					<<constantNames[CONSTANT_Utf8] << " ";
				core->console << core->format(cpool_string[i]->atom());
				core->console << "\n";
			}
#endif

		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolStrSize = (pos-startpos);
		#endif

		uint32 ns_count = readU30(pos);
		if (ns_count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		List<Namespace*, LIST_RCObjects> &cpool_ns = pool->cpool_ns;

		MMGC_MEM_TYPE(pool);
		cpool_ns.ensureCapacity(ns_count);
		pool->constantNsCount = ns_count;

#if defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		startpos = pos;
#endif
		for( uint32 i = 1; i < ns_count; ++i )
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
#ifdef SAFE_PARSE
			// check to see if we are trying to read past the file end or the beginning.
			if ( pos < abcStart || pos >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif // SAFE_PARSE
			CPoolKind kind = (CPoolKind) *(pos++);
			switch(kind)
			{
				case CONSTANT_Namespace: 
                case CONSTANT_PackageNamespace:
                case CONSTANT_PackageInternalNs:
                case CONSTANT_ProtectedNamespace:
			    case CONSTANT_ExplicitNamespace:					
                case CONSTANT_StaticProtectedNs:
				{
					uint32 index = readU30(pos);
                    Namespace::NamespaceType type = Namespace::NS_Public;
                    switch(kind)
                    {
                    case CONSTANT_PackageInternalNs:
                        type = Namespace::NS_PackageInternal;
                        break;
                    case CONSTANT_ProtectedNamespace:
                        type = Namespace::NS_Protected;
                        break;
                    case CONSTANT_ExplicitNamespace:
                        type = Namespace::NS_Explicit;
						break;
                    case CONSTANT_StaticProtectedNs:
                        type = Namespace::NS_StaticProtected;
                        break;
                    }

                    if (index)
					{
						Stringp uri = resolveUtf8(index);
						cpool_ns.set(i, core->internNamespace(core->newNamespace(uri, type)));
					}
					else
					{
						// issue this looks wrong.  should uri be ""?
						Atom uri = undefinedAtom;
						cpool_ns.set(i, core->internNamespace(core->newNamespace(uri, type)));
					}
					break;
				}

				case CONSTANT_PrivateNs: 
				{
					uint32 index =  readU30(pos);
					Stringp uri = index ? resolveUtf8(index) : (Stringp)core->kEmptyString;
                    Namespace* ns = new (core->GetGC()) Namespace(nullStringAtom, uri, Namespace::NS_Private);
					cpool_ns.set(i, ns);
					break;
				}
				default:
				{
					toplevel->throwVerifyError(kCpoolEntryWrongTypeError, core->toErrorString(i));
				}
			}
#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_ns["<<i<<"]="
					<<constantNames[kind] << " ";
				core->console << core->format(cpool_ns[i]->atom());
				core->console << "\n";
			}
#endif
		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolNsSize = (pos-startpos);
		#endif


		uint32 ns_set_count = readU30(pos);
		if (ns_set_count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		List<NamespaceSet*, LIST_GCObjects>& cpool_ns_set = pool->cpool_ns_set;
		cpool_ns_set.ensureCapacity(ns_set_count);
		pool->constantNsSetCount = ns_set_count;

#if defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		startpos = pos;
#endif

		for( uint32 i = 1; i < ns_set_count; ++i)
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
			uint32 ns_count = readU30(pos);
			
			if (ns_count > (uint32)(abcEnd - pos))
				toplevel->throwVerifyError(kCorruptABCError);
			NamespaceSet* namespace_set = core->newNamespaceSet(ns_count);

			Namespace** nss = namespace_set->namespaces;
			for(uint32 j=0; j < ns_count; ++j)
			{
				Namespace* ns = parseNsRef(pos);
				if (!ns)
					toplevel->throwVerifyError(kIllegalNamespaceError);
				//namespace_set->namespaces[j] = ns;
				WBRC(core->GetGC(), namespace_set, &nss[j], ns);
			}
			cpool_ns_set.set(i, namespace_set);

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_ns_set["<<i<<"]="
					<<constantNames[CONSTANT_NamespaceSet] << " ";
				core->console << cpool_ns_set[i]->format(core);
				core->console << "\n";
			}
#endif
		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolNsSetSize = (pos-startpos);
		#endif

		uint32 mn_count = readU30(pos);
		if (mn_count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		// TODO: why Atom?  its actually a list of positions
		List<Atom,LIST_NonGCObjects>& cpool_mn = pool->cpool_mn;
		MMGC_MEM_TYPE(pool);
		cpool_mn.ensureCapacity(mn_count);
		pool->constantMnCount = mn_count;

#if defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		startpos = pos;
#endif
		for(uint32 i = 1; i < mn_count; ++i )
		{
#ifdef AVMPLUS_VERBOSE
			int offset = pos-startpos;
#endif
#ifdef SAFE_PARSE
			// check to see if we are trying to read past the file end or the beginning.
			if ( pos < abcStart || pos >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif // SAFE_PARSE
			CPoolKind kind = (CPoolKind) *(pos++);
			switch(kind)
			{
			case CONSTANT_Qname: 
			case CONSTANT_QnameA:
			{
				// U16 namespace_index
				// U16 name_index
				// parse a multiname with one namespace (aka qname)
				cpool_mn.set(i, pool->posToAtom(pos-1));
				parseNsRef(pos);
				parseName(pos);
				break;
			}

			case CONSTANT_RTQname:
			case CONSTANT_RTQnameA: 
			{
				// U16 name_index
				// parse a multiname with just a name; ns fetched at runtime
				cpool_mn.set(i, pool->posToAtom(pos-1));
				parseName(pos);
				break;
			}
			case CONSTANT_RTQnameL:
			case CONSTANT_RTQnameLA:
			{
				cpool_mn.set(i, pool->posToAtom(pos-1));
				break;
			}

			case CONSTANT_Multiname:
			case CONSTANT_MultinameA:
			{
				cpool_mn.set(i, pool->posToAtom(pos-1));
   				
				parseName(pos);
				
				uint32 index = readU30(pos);

				if (!index || index >= pool->constantNsSetCount)
					toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantNsSetCount));

				// If it is in the range of Namespace Sets then it must be a namespace set/
				break;
			}

			case CONSTANT_MultinameL:
			case CONSTANT_MultinameLA:
			{
				cpool_mn.set(i, pool->posToAtom(pos-1));
   				
				uint32 index = readU30(pos);

				if (!index || index >= pool->constantNsSetCount)
					toplevel->throwVerifyError(kCpoolIndexRangeError, core->toErrorString(index), core->toErrorString(pool->constantNsSetCount));

				// If it is in the range of Namespace Sets then it must be a namespace set.

				break;
			}
			
			default:
				toplevel->throwVerifyError(kCpoolEntryWrongTypeError, core->toErrorString(i));
			}
#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << offset << ":" << "cpool_mn["<<i<<"]="
					<<constantNames[kind] << " ";
				Multiname name;
				pool->parseMultiname(name, i);
				core->console << &name;
				core->console << "\n";
			}
#endif
		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.cpoolMnSize = (pos-startpos);
		#endif

    }

	void AbcParser::addNamedTraits(Namespace* ns, Stringp name, Traits* itraits)
	{
		if (!ns->isPrivate() && !domain->namedTraits->get(name, ns))
		{
			domain->namedTraits->add(name, ns, (Atom)itraits);
		}
		else
		{
			// duplicate class
			//Multiname qname(ns,name);
			//toplevel->definitionErrorClass()->throwError(kRedefinedError, core->toErrorString(&qname));
		}
	}
	
	void AbcParser::addNamedScript(Namespace* ns, Stringp name, AbstractFunction* script)
	{
		AbstractFunction* s = (AbstractFunction*) domain->namedScripts->get(name, ns);
		if (!s)
		{
			if(ns->isPrivate())
			{
				pool->addPrivateNamedScript(name, ns, script);
			}
			else
			{
				domain->namedScripts->add(name, ns, (Atom)script);
			}
		}
		else
		{
			// duplicate definition
			//Multiname qname(ns, name);
			//toplevel->definitionErrorClass()->throwError(kRedefinedError, core->toErrorString(&qname));
		}
	}

	bool AbcParser::parseScriptInfos()
	{
		/*
			U16 script_count
			ScriptInfo[script_count]
			{
				U16 init_index			// method_info index of init function
				Traits script_traits    // traits for the global object of this package
			}
		*/

		uint32 count = readU30(pos);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
		{
			core->console << "script_count=" << count << "\n";
		}
#endif

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		const byte* startpos = pos;
#endif

		if (count == 0)
		{
			return true;
		}

		if (count > (uint32)(abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		pool->scripts.ensureCapacity(count);
		pool->scriptCount = count;

		// make global objects subclasses of Object

		for (uint32 i=0; i < count; i++)
		{
#ifdef AVMPLUS_VERBOSE
			const byte* script_pos = pos;
#endif

			int init_index = readU30(pos);

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "    " << (int)(script_pos-startpos) << ":script[" << i << "]"
					<< " init_index=" << init_index
					<< "\n";
			}
#endif
			AbstractFunction* script = resolveMethodInfo(init_index);
			AvmAssert(script->declaringTraits == NULL);

			if (script->declaringTraits != NULL)
			{
				// method has already been bound to a different type.  Can't bind it twice because
				// it can only have one environment, for its scope chain and super references.
				toplevel->throwVerifyError(kAlreadyBoundError, core->toErrorString(script), core->toErrorString((Traits*)script->declaringTraits));
			}

			Traits* traits = parseTraits(OBJECT_TYPE, 
				core->publicNamespace, core->kglobal,
				script, 0);

			if( !traits ) return false; // parseTraits failed

			traits->final = true;
			
			// global object, make it dynamic
			NativeScriptInfo* nativeEntry;
			if (nativeScripts && (nativeEntry=nativeScripts[i]) != 0)
			{
				traits->sizeofInstance = nativeEntry->sizeofInstance;
				traits->setNativeScriptInfo(nativeEntry);
				AvmAssert(traits->sizeofInstance >= sizeof(ScriptObject));
			}
			else
			{
				traits->sizeofInstance = sizeof(ScriptObject);
			}
			traits->needsHashtable = true;

			script->makeMethodOf(traits);
			traits->init = script;

			#ifdef AVMPLUS_VERBOSE
			script->name = core->concatStrings(traits->format(core), core->newString("$init"));
			#endif

            #if defined(AVMPLUS_INTERP) && defined(AVMPLUS_MIR)
			if (!core->forcemir)
			{
				// suggest that we don't jit the $init methods
				script->flags |= AbstractFunction::SUGGEST_INTERP;
			}
			#endif /* AVMPLUS_INTERP */

			pool->scripts.set(i, script);

			// initial scope chain is []
			traits->scope = ScopeTypeChain::create(core->GetGC(),NULL,0);
		}
		#ifdef AVMPLUS_PROFILE
		core->sprof.scriptsSize = (pos-startpos);
		#endif

		return true;
	}

	// helper for interface flattening
	void AbcParser::addTraits(Hashtable *ht, Traits *traits, Traits *baseTraits)
	{
		if(!baseTraits || !baseTraits->containsInterface(traits))
		{
			ht->add((Atom)traits, traits);			
			Traitsp *interfaces = traits->getInterfaces();
			for (int i=0, n=traits->interfaceCapacity; i < n; i++) {
				Traits* t = interfaces[i];
				if (t != NULL && t->isInterface) {
					addTraits(ht, t, baseTraits);
				}			
			}
		}
	}

	bool AbcParser::parseInstanceInfos()
    {
        classCount = readU30(pos);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
			core->console << "class_count=" << classCount <<"\n";
#endif

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		const byte* startpos = pos;
#endif

		if (classCount == 0)
		{
			return true;
		}

		if (classCount > (abcEnd - pos))
			toplevel->throwVerifyError(kCorruptABCError);

		// allocate room for class infos early, to handle nested classes
		pool->cinits.ensureCapacity(classCount);
		pool->classCount = classCount;

		instances.ensureCapacity(classCount);

        for (int i=0; i < classCount; i++)
        {
			#ifdef AVMPLUS_VERBOSE
            const byte* classpos = pos;
			#endif // AVMPLUS_VERBOSE

            // CONSTANT_QName name of this class
			Namespace* ns;
			Stringp name;

			Multiname qname;
			resolveQName(pos, qname);
			ns = qname.getNamespace();
			name = qname.getName();

			// resolving base class type means class heirarchy must be a Tree
			Traits* baseTraits = pool->resolveTypeName(pos, toplevel);

			if (baseTraits && baseTraits->final ||
				CLASS_TYPE != NULL && baseTraits == CLASS_TYPE ||
				FUNCTION_TYPE != NULL && baseTraits == FUNCTION_TYPE)
			{
				// error - attempt to extend final class
				#ifdef AVMPLUS_VERBOSE
				if (pool->verbose)
					core->console << &qname << " can't extend final class " << baseTraits << "\n";
				#endif
				toplevel->throwVerifyError(kCannotExtendFinalClass, core->toErrorString(&qname));
			}

			if (baseTraits && baseTraits->isInterface)
			{
				// error, can't extend interface
				toplevel->throwVerifyError(kCannotExtendError, core->toErrorString(&qname), core->toErrorString(baseTraits));
			}

            // read flags:	bit 0: sealed
			//				bit 1: final
			//              bit 2: interface
			//              bit 3: protected
#ifdef SAFE_PARSE
			// check to see if we are trying to read past the file end or the beginning.
			if ( pos < abcStart || pos >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif // SAFE_PARSE
			int flags = *pos++;

			// read protected namespace
			Namespace *protectedNamespace = NULL;
			if (flags & 8)
			{
				protectedNamespace = parseNsRef(pos);
			}
			
            int interfaceCount = readU30(pos);
			int interfaceDelta = 0;
			const byte* interfacePos = pos;

			if(interfaceCount)
			{
				Hashtable *ht = new (core->GetGC()) Hashtable(core->GetGC(), interfaceCount*2);
				for( int x = 0; x < interfaceCount; ++ x )
				{
					Traits *t = pool->resolveTypeName(pos, toplevel);

					if (!t || !t->isInterface)
					{
						// error, can't extend interface
						toplevel->throwVerifyError(kCannotImplementError, core->toErrorString(&qname), core->toErrorString(t));
					}
					
					addTraits(ht, t, baseTraits);
				}
				interfaceDelta = ht->getSize();
				delete ht;
			}

			// TODO make sure the inheritance is legal.  
			//  - can't override final members
			//  - overrides agree with base class signature
			
            uint32 iinit_index = readU30(pos);
			AbstractFunction *iinit = resolveMethodInfo(iinit_index);

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				// TODO:  fixup this math here, since the 2's are all wrong
				core->console
					<< "    " << (int)(classpos-startpos) << ":instance[" << i << "]"
					<< " " << &qname;

				if (baseTraits)
					core->console << " extends " << baseTraits;

				core->console
					<< " interface_count=" << interfaceCount
					<< " iinit_index=" << iinit_index
					<< "\n";
			}
#endif

			#ifdef AVMPLUS_VERBOSE
			iinit->name = core->concatStrings(Multiname::format(core,ns,name), core->newString("$iinit"));
			#endif

			Traits* itraits = parseTraits(baseTraits, ns, name, 0, interfaceDelta, protectedNamespace);
			if( !itraits ) return false;
			if (!baseTraits && core->traits.object_itraits == NULL)
			{
				// save object traits
				core->traits.object_itraits = itraits;
				itraits->isObjectType = true;
			}

			// AS3 language decision: dynamic is not inherited.
			itraits->needsHashtable = (flags&1) == 0;
			itraits->final   = (flags&2) != 0;
			itraits->sizeofInstance = computeInstanceSize(i, baseTraits);

			if (flags & 4)
			{
				itraits->isInterface = true;

				if (itraits->slotCount != 0)
				{
					toplevel->throwVerifyError(kIllegalSlotError, core->toErrorString(itraits));
				}

				// interface base must be *
				if (baseTraits)
				{
					// error, can't extend this type
					toplevel->throwVerifyError(kCannotExtendError, core->toErrorString(&qname), core->toErrorString(baseTraits));
				}
			}

			if (iinit->declaringTraits != NULL)
			{
				// method has already been bound to a different type.  Can't bind it twice because
				// it can only have one environment, for its scope chain and super references.
				toplevel->throwVerifyError(kAlreadyBoundError, core->toErrorString(iinit), core->toErrorString((Traits*)iinit->declaringTraits));
			}

			iinit->makeMethodOf(itraits);
			itraits->init = iinit;

			// parse the interfaces.  resolving type refs here means interface heirarchy must be a DAG.
			for (int j=0, n=interfaceCount; j < n; j++)
			{
				Traits *interfaceTraits = pool->resolveTypeName(interfacePos, toplevel);

				itraits->addInterface(interfaceTraits);
				#ifdef AVMPLUS_VERBOSE
				if (pool->verbose)
					core->console << "        interface["<<j<<"]=" << interfaceTraits <<"\n";
				#endif
			}
						
			AvmAssert(itraits->interfaceSlop() == 0);

			instances.set(i, itraits);

			if (pool->getTraits(name, ns, false) == NULL)
			{
				pool->addNamedTraits(name, ns, itraits);
			}
			else
			{
				// error, can't redefine a class or interface
				//toplevel->definitionErrorClass()->throwError(kRedefinedError, core->toErrorString(&qname));
			}
        }

		#ifdef AVMPLUS_PROFILE
		core->sprof.instancesSize = (pos-startpos);
		#endif

		return true;
    }

	void AbcParser::parseClassInfos()
    {
		if (classCount == 0)
		{
			return;
		}

#if defined(AVMPLUS_PROFILE) || defined(AVMPLUS_VERBOSE) || defined(DEBUGGER)
		const byte* startpos = pos;
#endif

        for (int i=0; i < classCount; i++)
        {
            // CONSTANT_Multiname name of this class
			Traits* itraits = instances[i];
			Namespace* ns = itraits->ns;
			Stringp name = itraits->name;

#ifdef AVMPLUS_VERBOSE
			const byte* class_pos = pos;
#endif

			uint32 cinit_index = readU30(pos);
            AbstractFunction *cinit = resolveMethodInfo(cinit_index);

#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console
					<< "    " << (int)(class_pos-startpos) << ":class[" << i << "]"
					<< " " << ns << "::" << name;

				core->console
					<< " cinit_index=" << cinit_index
					<< "\n";
			}
#endif

			#ifdef AVMPLUS_VERBOSE
			Stringp cinitName = core->concatStrings(name, core->newString("$cinit"));
			cinit->name = Multiname::format(core,ns,cinitName);
			#endif

			Traits* ctraits = parseTraits(CLASS_TYPE, 
				ns, core->internString(core->concatStrings(name, core->newString("$"))), 0, 0, itraits->protectedNamespace);

			NativeClassInfo *nativeEntry;
			if (nativeClasses && (nativeEntry=nativeClasses[i]) != 0)
			{
				ctraits->sizeofInstance = nativeEntry->sizeofClass;
				ctraits->setNativeClassInfo(nativeEntry);
			}
			else
			{
				ctraits->sizeofInstance = sizeof(ClassClosure);
			}

			AvmAssert(cinit->declaringTraits == NULL);

			if (cinit->declaringTraits != NULL)
			{
				// method has already been bound to a different type.  Can't bind it twice because
				// it can only have one environment, for its scope chain and super references.
				toplevel->throwVerifyError(kAlreadyBoundError, core->toErrorString(cinit), core->toErrorString((Traits*)cinit->declaringTraits));
			}
			
			cinit->makeMethodOf(ctraits);
			ctraits->init = cinit;
			ctraits->itraits = itraits;
			ctraits->final = true;
			ctraits->needsHashtable = true;

            #if defined(AVMPLUS_INTERP) && defined(AVMPLUS_MIR)
			if (!core->forcemir)
			{
				// suggest that we don't jit the class initializer
				cinit->flags |= AbstractFunction::SUGGEST_INTERP;
			}
			#endif /* AVMPLUS_INTERP */

			pool->cinits.set(i, cinit);
        }

		#ifdef AVMPLUS_PROFILE
		core->sprof.classesSize = (pos-startpos);
		#endif
    }

	unsigned int AbcParser::readU30(const byte *&p) const
	{
#ifdef SAFE_PARSE
		// We have added kBufferPadding bytes to the end of the main swf buffer.
		// Why?  Here we can read from 1 to 5 bytes.  If we were to
		// put the required safety checks at each byte read, we would slow
		// parsing of the file down.  With this buffer, only one check at the
		// top of this function is necessary. (we will read on into our own memory)
		if ( p >= abcEnd || p < abcStart )
			toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE
		return toplevel->readU30(p);
	}

}
