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
	using namespace MMgc;

	BEGIN_NATIVE_CLASSES(AvmCore)
		NATIVE_CLASS(abcclass_Object,				ObjectClass,	ScriptObject)
		NATIVE_CLASS(abcclass_Class,				ClassClass,		ClassClosure)
		NATIVE_CLASS(abcclass_Namespace,			NamespaceClass,	Namespace)
		NATIVE_CLASS(abcclass_Function, 			FunctionClass,	ClassClosure)
		NATIVE_CLASS(abcclass_builtin_as_0_MethodClosure, 			MethodClosureClass,	MethodClosure)

		NATIVE_CLASS(abcclass_Error,				ErrorClass,	ErrorObject)

		// The Error subclasses are included in the NATIVE_CLASS map
		// so that their [[Call]] action is mapped to [[Construct]]
		NATIVE_CLASS(abcclass_DefinitionError,      NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_EvalError,            NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_RangeError,           NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_ReferenceError,       NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_SecurityError,        NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_SyntaxError,          NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_TypeError,            NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_URIError,             NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_VerifyError,          NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_UninitializedError,   NativeErrorClass, ErrorObject)
		NATIVE_CLASS(abcclass_ArgumentError,        NativeErrorClass, ErrorObject)

		NATIVE_CLASS(abcclass_String,				StringClass,	String)
		NATIVE_CLASS(abcclass_Boolean,			BooleanClass,	bool)
		NATIVE_CLASS(abcclass_Number,				NumberClass,	double)
		NATIVE_CLASS(abcclass_int,				IntClass,		int)
		NATIVE_CLASS(abcclass_uint,				UIntClass,		uint32)
		NATIVE_CLASS(abcclass_Math,				MathClass,		double)
		NATIVE_CLASS(abcclass_Array,				ArrayClass,		ArrayObject)
		NATIVE_CLASS(abcclass_RegExp,				RegExpClass,	RegExpObject)
		NATIVE_CLASS(abcclass_Date,				DateClass,		DateObject)

		// E4X
		NATIVE_CLASS(abcclass_XML,				XMLClass,		XMLObject)
		NATIVE_CLASS(abcclass_XMLList,			XMLListClass,	XMLListObject)
		NATIVE_CLASS(abcclass_QName,			QNameClass,		QNameObject)
	END_NATIVE_CLASSES()

	// don't need toplevel here because we create it manually
	BEGIN_NATIVE_SCRIPTS(AvmCore)
		NATIVE_SCRIPT(0, Toplevel)
	END_NATIVE_SCRIPTS()

	AvmCore::AvmCore(GC *g) : GCRoot(g), console(NULL), mirBuffers(g, 4), gcInterface(g)
    {
		// sanity check for all our types
		AvmAssert (sizeof(int8) == 1);
		AvmAssert (sizeof(uint8) == 1);		
		AvmAssert (sizeof(int16) == 2);
		AvmAssert (sizeof(uint16) == 2);
		AvmAssert (sizeof(int32) == 4);
		AvmAssert (sizeof(uint32) == 4);
		AvmAssert (sizeof(int64) == 8);
		AvmAssert (sizeof(uint64) == 8);
		AvmAssert (sizeof(sintptr) == sizeof(void *));
		AvmAssert (sizeof(uintptr) == sizeof(void *));
		#ifdef AVMPLUS_64BIT
		AvmAssert (sizeof(sintptr) == 8);
		AvmAssert (sizeof(uintptr) == 8);		
		#else
		AvmAssert (sizeof(sintptr) == 4);
		AvmAssert (sizeof(uintptr) == 4);		
		#endif	
			
		#ifdef DEBUGGER	
		sampling = false;
		autoStartSampling = false;
		#endif

		// set default mode flags
		#ifdef AVMPLUS_VERBOSE
		verbose = false;
		#endif

		#ifdef AVMPLUS_INTERP
		#ifdef AVMPLUS_64BIT // turbo disabled by default since MIR not working yet
		turbo = false;
		#else
 		turbo = true;
		#endif
		#endif

		#ifdef AVMPLUS_VERIFYALL
		verifyall = false;
		#endif

		#ifdef AVMPLUS_MIR

			#ifdef AVMPLUS_INTERP
			// forcemir flag forces use of MIR instead of interpreter
			forcemir = false;
			#endif
	
			cseopt = true;
			dceopt = true;

		    #if defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
    		sse2 = true;
			#endif

			#ifdef AVMPLUS_VERBOSE
			bbgraph = false;
			#endif

		#endif // AVMPLUS_MIR

		#ifdef DEBUGGER	
		allocationTracking = false;
		#endif

		interrupts = false;

		gcInterface.SetCore(this);
		resources          = NULL;
		xmlEntities        = NULL;
		exceptionFrame     = NULL;
		builtinPool        = NULL;
		builtinDomain      = NULL;

		GetGC()->SetGCContextVariable (MMgc::GC::GCV_AVMCORE, this);

		minstack           = 0;
		
		#ifdef DEBUGGER
		langID			   = -1;
		debugger           = NULL;
		profiler		   = NULL;
		callStack          = NULL;
		stackTraces        = NULL;
		numTraces          = 0;
		samples            = NULL;
		takeSample         = false;
		samplingNow        = false;
		numSamples         = 0;
		fakeMethodInfos    = 0;
        #endif /* DEBUGGER */

		interrupted        = false;

		codeContextAtom    = CONTEXT_NONE;
		dxnsAddr		   = NULL;
		
		strings			= NULL;
		numStrings		= 0;
		namespaces		= NULL;
		numNamespaces	= 0;
		stringCount		= 0;
		deletedCount	= 0;
		nsCount			= 0;

		numStrings = 1024; // power of 2
		strings = new DRC(Stringp)[numStrings];
		memset(strings, 0, numStrings*sizeof(Stringp));

		numNamespaces = 1024;  // power of 2
		namespaces = new DRC(Namespacep)[numNamespaces];
		memset(namespaces, 0, numNamespaces*sizeof(Namespace*));

		console.setCore(this);
		
        kEmptyString = constantString("");
		kconstructor = constantString("constructor");
        kundefined = constantString("undefined");
        knull = constantString("null");
        ktrue = constantString("true");
        kfalse = constantString("false");
		ktoString = constantString("toString");
		ktoLocaleString = constantString("toLocaleString");
		kvalueOf = constantString("valueOf");
		klength = constantString("length");

        kobject = constantString("object");
        kboolean = constantString("boolean");
        knumber = constantString("number");
        kstring = constantString("string");
        kxml = constantString("xml");
		kfunction = constantString("function");
		kglobal = constantString("global");
		kcallee = constantString("callee");

		kuri = constantString("uri");
		kprefix = constantString("prefix");
		kNaN = doubleToAtom(MathUtils::nan());
		kNeedsDxns = constantString("NeedsDxns");
		kAsterisk = constantString("*");

		for (int i = 0; i < 128; i++)
		{
			char singleChar = (char)i;
			// call newString() with an explicit length of 1; required
			// when singleChar==0, because in that case we need a string
			// which is a single character with value 0
			cachedChars[i] = internString(newString(&singleChar, 1));
		}

		booleanStrings[0] = kfalse;
        booleanStrings[1] = ktrue;

		// create public namespace 
		publicNamespace = internNamespace(newNamespace(kEmptyString));

		#if defined(AVMPLUS_MIR) && defined(AVMPLUS_VERBOSE)
		codegenMethodNames = CodegenMIR::initMethodNames(this);
		#endif

		#ifdef FEATURE_JNI
		java = NULL;
		#endif
	}

	AvmCore::~AvmCore()
	{		
#ifdef DEBUGGER
		stopSampling();
#endif

		// Free the numbers and strings tables
		delete [] strings;
		if (GetGC()) 
		{
			GetGC()->SetGCContextVariable(GC::GCV_AVMCORE, NULL);
		}

		#if defined(AVMPLUS_MIR) && defined(AVMPLUS_VERBOSE)
		delete codegenMethodNames;
		#endif

		strings = NULL;

		delete [] namespaces;
		namespaces = NULL;

		// free all the mir buffers
		while(mirBuffers.size() > 0)
			mirBuffers.removeFirst()->free();
	}

	void AvmCore::initBuiltinPool()
	{
		using namespace NativeID;

		// stack allocated array of pointers
		AbstractFunction* nativeMethods[builtin_abc_method_count];
		NativeClassInfo* nativeClasses[builtin_abc_class_count];
		NativeScriptInfo* nativeScripts[builtin_abc_script_count];

		memset(nativeMethods, 0, sizeof(AbstractFunction*)*builtin_abc_method_count);
		memset(nativeClasses, 0, sizeof(NativeClassInfo*)*builtin_abc_class_count);
		memset(nativeScripts, 0, sizeof(NativeScriptInfo*)*builtin_abc_script_count);

		initNativeTables(classEntries, scriptEntries,
			nativeMethods, nativeClasses, nativeScripts);

		ScriptBuffer code = ScriptBuffer(new (GetGC()) ReadOnlyScriptBufferImpl (builtin_abc_data, builtin_abc_length));

		builtinDomain = new (GetGC()) Domain(this, NULL);
		
		builtinPool = parseActionBlock(code, 0, NULL,
									   builtinDomain,
									   nativeMethods,
									   nativeClasses,
									   nativeScripts);

		// whack the the non-interruptable bit on all builtin functions
		for(int i=0, size=builtinPool->methods.size(); i<size; i++)
			builtinPool->methods[i]->flags |= AbstractFunction::NON_INTERRUPTABLE;

		for(int i=0, size=builtinPool->cinits.size(); i<size; i++)
			builtinPool->cinits[i]->flags |= AbstractFunction::NON_INTERRUPTABLE;

		for(int i=0, size=builtinPool->scripts.size(); i<size; i++)
			builtinPool->scripts[i]->flags |= AbstractFunction::NON_INTERRUPTABLE;

#ifdef DEBUGGER
		// sampling can begin now, requires builtinPool
		initSampling();
#endif
	}
	
	Toplevel* AvmCore::initTopLevel()
	{
		Toplevel* toplevel = NULL;
		handleActionPool(builtinPool, NULL, toplevel, NULL);
		return toplevel;
	}

	ScriptEnv* AvmCore::prepareActionPool(PoolObject* pool,
										  DomainEnv* domainEnv,
										  Toplevel*& toplevel,
	                                      CodeContext* codeContext)
	{
		AvmAssert(pool != NULL);

		// get the main entry point and its global traits
		if (pool->scriptCount == 0)
		{
			toplevel->verifyErrorClass()->throwError(kMissingEntryPointError);
		}

		if (domainEnv == NULL)
		{
			// create a new DomainEnv based on the builtinDomain
			domainEnv = new (GetGC()) DomainEnv(this, builtinDomain, NULL);
		}

		AbcEnv* abcEnv = new (GetGC(), AbcEnv::calcExtra(pool)) AbcEnv(pool, domainEnv, codeContext);

		// entry point is the last script in the file
		Traits* mainTraits = pool->scripts[pool->scriptCount-1]->declaringTraits;

		// ISSUE can we just make this the public namespace?
		ScopeChain* emptyScope = ScopeChain::create(GetGC(), mainTraits->scope, NULL, newNamespace(kEmptyString));

		VTable* object_vtable;
		if (toplevel == NULL)
		{
			// create a temp object vtable to use, since the real one isn't created yet
			// later, in OP_newclass, we'll replace with the real Object vtable, so methods
			// of Object and Class have the right scope.

			object_vtable = newVTable(traits.object_itraits, NULL, emptyScope, abcEnv, NULL);
			object_vtable->resolveSignatures();
			mainTraits->sizeofInstance = getToplevelSize();
		}
		else
		{
			object_vtable = toplevel->object_vtable;
		}

		// global objects are subclasses of Object
		VTable* mainVTable = newVTable(mainTraits, object_vtable, emptyScope, abcEnv, toplevel);
		ScriptEnv* main = new (GetGC()) ScriptEnv(mainTraits->init, mainVTable);
		mainVTable->init = main;

		if (toplevel == NULL)
		{
			mainVTable->resolveSignatures();
			main->global = toplevel = createToplevel(mainVTable);

			// save toplevel since it was initially null 
			mainVTable->toplevel = toplevel;
			object_vtable->toplevel = toplevel;
			domainEnv->setToplevel(toplevel);

			// create temporary vtable for Class, so we have something for OP_newclass
			// to use when it creates Object$ and Class$.  once that happens, we replace
			// with the real Class$ vtable.
			toplevel->class_vtable = newVTable(traits.class_itraits, 
				object_vtable, emptyScope, abcEnv, toplevel);
			toplevel->class_vtable->resolveSignatures();

			traits.function_itraits->resolveSignatures(toplevel);
		}

		exportDefs(mainTraits, main);

		// prepare the remaining scriptEnv's
		for (int i=0, n=pool->scriptCount-1; i < n; i++)
		{
			AbstractFunction* script = pool->scripts[i];

			Traits* scriptTraits = script->declaringTraits;
			// [ed] 3/24/06 why do we really care if a script is dynamic or not?
			//AvmAssert(scriptTraits->needsHashtable);

			VTable* scriptVTable = newVTable(scriptTraits, object_vtable, emptyScope, abcEnv, toplevel);
			ScriptEnv* scriptEnv = new (GetGC()) ScriptEnv(scriptTraits->init, scriptVTable);
			scriptVTable->init = scriptEnv;
			exportDefs(scriptTraits, scriptEnv);
		}

		return main;
	}

	Atom AvmCore::handleActionPool(PoolObject* pool,
										DomainEnv* domainEnv,
										Toplevel* &toplevel,
	                                    CodeContext* codeContext)
	{
		bool createdToplevel = (toplevel == NULL);

		ScriptEnv* main = prepareActionPool(pool, domainEnv, toplevel, codeContext);

		if (!createdToplevel)
		{
			main->initGlobal();
		}
		
		Atom argv[1] = { main->global->atom() };
		Atom result = 0; // init to zero to make GCC happy
		#ifndef DEBUGGER
		result = main->coerceEnter(0, argv);
		#else
		TRY(this, kCatchAction_Rethrow)
		{
			result = main->coerceEnter(0, argv);
			#ifdef AVMPLUS_PROFILE
			if (dprof.dprofile)
				dprof.mark((AbcOpcode)0);
			#endif
		}
		CATCH(Exception *exception)
		{
			#ifdef AVMPLUS_PROFILE
			if (dprof.dprofile)
				dprof.mark((AbcOpcode)0);
			#endif
			// Re-throw exception
			throwException(exception);
		}
		END_CATCH
		END_TRY
		#endif

		return result;
	}
	
	void AvmCore::exportDefs(Traits* scriptTraits, ScriptEnv* scriptEnv)
	{
		DomainEnv* domainEnv = scriptEnv->domainEnv();
		AbcEnv* abcEnv = scriptEnv->abcEnv();

		// iterate thru each of the definitions exported by this script
		int i=0;
		while((i=scriptTraits->next(i)) != 0)
		{
			// don't need to check for DELETED because we never remove trait bindings
			AvmAssert(scriptTraits->keyAt(i) != NULL);
			Stringp name = scriptTraits->keyAt(i);
			Namespace* ns = scriptTraits->nsAt(i);
				
			// not already in the table then export it 
			// otherwise we keep the first one that was encountered.
			if (!ns->isPrivate())
			{				
				if (!domainEnv->namedScripts->get(name, ns))
				{
					// add ns/name to global table
					// ISSUE should we filter out Object traits and/or private members?
					#ifdef AVMPLUS_VERBOSE
					if (scriptTraits->pool->verbose)
						console << "exporting " << ns << "::" << name << "\n";
					#endif
					domainEnv->namedScripts->add(name, ns, (Binding)scriptEnv);
				}
			}
			else
			{
				if (!abcEnv->privateScriptEnvs.get(name, ns))
				{
					abcEnv->privateScriptEnvs.add(name, ns, (Binding) scriptEnv);
				}
			}
		}
	}

	PoolObject* AvmCore::parseActionBlock(ScriptBuffer code,
										  int /*start*/,
										  Toplevel* toplevel,
										  Domain* domain,
										  AbstractFunction *nativeMethods[],
										  NativeClassInfo *nativeClasses[],
										  NativeScriptInfo *nativeScripts[])
	{
		// parse constants and attributes.
		PoolObject* pool = AbcParser::decodeAbc(this,
												code,
												toplevel,
												domain,
												nativeMethods,
												nativeClasses,
												nativeScripts);

        #ifdef DEBUGGER
		if (debugger) {
			debugger->processAbc(pool, code);
		}
        #endif /* DEBUGGER */

		return pool;
	}
	
    Atom AvmCore::handleActionBlock(ScriptBuffer code,
										 int start,
										 DomainEnv* domainEnv,
										 Toplevel* &toplevel,
										 AbstractFunction *nativeMethods[],
										 NativeClassInfo *nativeClasses[],
										 NativeScriptInfo *nativeScripts[],
										 CodeContext *codeContext)
    {
		resources = new (GetGC()) Hashtable(GetGC());

		// have we parsed this before?
		PoolObject* pool;
        Atom resourceAtom = resources->get(start+1);
        if (resourceAtom != undefinedAtom) 
		{
			pool = (PoolObject*) resourceAtom;
		} 
		else 
		{
			Domain* domain = domainEnv ? domainEnv->getDomain() : builtinDomain;
			
			// parse constants and attributes.
			pool = parseActionBlock(code,
									start,
									toplevel,
									domain,
									nativeMethods,
									nativeClasses,
									nativeScripts);
			if (pool != NULL)
			{
				resources->put(start+1, pool);
			}
		}

		return handleActionPool(pool, domainEnv, toplevel, codeContext);
	}

/*
11.9.3 The Abstract Equality Comparison Algorithm
The comparison x == y, where x and y are values, produces true or false. Such a comparison is performed as
follows:
E4X - 0a. if x is XMLList, call [[Equals]] method of x with argument y and return
E4X - 0b. if y is XMLList, call [[Equals]] method of y with argument x and return
1. If Type(x) is different from Type(y), go to step 14.
E4X - 1a. If x is XML and y is XML
	if x.class = text/attribute && y.class == text/attribute, do string compare
	else to x.[[Equals]](y)
E4X - 1b If x and y are QName
    return uri=uri and localname==localName
E4X - 1c if x and y are Namespaces
	return uri == uri
2. If Type(x) is Undefined, return true.
3. If Type(x) is Null, return true.
4. If Type(x) is not Number, go to step 11.
5. If x is NaN, return false.
6. If y is NaN, return false.
7. If x is the same number value as y, return true.
8. If x is +0 and y is -0, return true.
9. If x is -0 and y is +0, return true.
10. Return false.
11. If Type(x) is String, then return true if x and y are exactly the same sequence of characters (same length and
same characters in corresponding positions). Otherwise, return false.
12. If Type(x) is Boolean, return true if x and y are both true or both false. Otherwise, return false.
13. Return true if x and y refer to the same object or if they refer to objects joined to each other (section 13.1.2).
Otherwise, return false.

// Different types below here
E4X 14a. if (x is xml) and x.hasSimpleContent = true or (y is xml) and y.hasSimlpeContent = true
	return the results of tostring(x) and tostring(y)

14. If x is null and y is undefined, return true.
15. If x is undefined and y is null, return true.
16. If Type(x) is Number and Type(y) is String,
return the result of the comparison x == ToNumber(y).
17. If Type(x) is String and Type(y) is Number,
return the result of the comparison ToNumber(x) == y.
18. If Type(x) is Boolean, return the result of the comparison ToNumber(x) == y.
19. If Type(y) is Boolean, return the result of the comparison x == ToNumber(y).
20. If Type(x) is either String or Number and Type(y) is Object,
return the result of the comparison x == ToPrimitive(y).
21. If Type(x) is Object and Type(y) is either String or Number,
return the result of the comparison ToPrimitive(x) == y.
22. Return false.
	*/

    Atom AvmCore::eq(Atom lhs, Atom rhs)
    {
		if (isNull(lhs)) lhs = 0;
		if (isNull(rhs)) rhs = 0;

		int ltype = lhs & 7;
        int rtype = rhs & 7;

		// See E4X 11.5.1, pg 53.  
		if ((ltype == kObjectType) && (isXMLList(lhs)))
			return atomToXMLList (lhs)->_equals (rhs);
		else if ((rtype == kObjectType) && (isXMLList(rhs)))
			return atomToXMLList (rhs)->_equals (lhs);

        if (ltype == rtype)
        {
            // same type
            switch (ltype)
            {
			case 0:
			case kSpecialType:
				return trueAtom;
            case kStringType:
				if (lhs == rhs) return trueAtom;
				return (*atomToString(lhs) == *atomToString(rhs)) ? trueAtom : falseAtom;
            case kBooleanType:
			case kIntegerType:
				return lhs == rhs ? trueAtom : falseAtom;
			case kNamespaceType:
				// E4X 11.5.1, pg 53
				return atomToNamespace(lhs)->equalTo(atomToNamespace(rhs))? trueAtom : falseAtom;
			case kObjectType:
			{
				// E4X 11.5.1, pg 53
				if (lhs == rhs)
					return trueAtom;
				if (isXML(lhs) && isXML(rhs))
				{
					XMLObject *x = atomToXMLObject (lhs);
					XMLObject *y = atomToXMLObject (rhs);
					if ((((x->getClass() & (E4XNode::kText | E4XNode::kCDATA | E4XNode::kAttribute))) && y->hasSimpleContent()) ||
						(((y->getClass() & (E4XNode::kText | E4XNode::kCDATA | E4XNode::kAttribute))) && x->hasSimpleContent()))
					{
						return ((*string(lhs) == *string(rhs)) ? trueAtom : falseAtom);
					}	
					else
					{
						return x->getNode()->_equals (this, y->getNode());
					}
				}
				else if (isQName(lhs) && isQName(rhs))
				{
					QNameObject *qn1 = atomToQName (lhs);
					QNameObject *qn2 = atomToQName (rhs);
					return (((qn1->getURI() == qn2->getURI()) && (qn1->getLocalName() == qn2->getLocalName()))? trueAtom : falseAtom);
				}
				else
				{
					return falseAtom;
				}
			}
			case kDoubleType:
                // C++ portability note -- if either arg is NaN, java == returns false, which matches ECMA.
                return atomToDouble(lhs) == atomToDouble(rhs) ? trueAtom : falseAtom;
            }
        }
		else
		{
			if (isNullOrUndefined(lhs) && isNullOrUndefined(rhs))
				return trueAtom;
            if (ltype == kIntegerType && rtype == kDoubleType)
				return ((double)(lhs>>3)) == atomToDouble(rhs) ? trueAtom : falseAtom;
            if (ltype == kDoubleType && rtype == kIntegerType)
                return atomToDouble(lhs) == ((double)(rhs>>3)) ? trueAtom : falseAtom;

			// 16. If Type(x) is Number and Type(y) is String,
			// return the result of the comparison x == ToNumber(y).
            if (isNumber(lhs) && isString(rhs))
                return eq(lhs, doubleToAtom(number(rhs)));
			
			// 17. If Type(x) is String and Type(y) is Number,
			// return the result of the comparison ToNumber(x) == y.
            if (isString(lhs) && isNumber(rhs))
                return eq(doubleToAtom(number(lhs)), rhs);

			// E4X 11.5.1, step 4.  Placed slightly lower then in the spec
			// to handle quicker cases earlier.  No cases above should be comparing
			// an object to a non-object
			if (((ltype == kObjectType) && isXML(lhs) && atomToXMLObject(lhs)->hasSimpleContent()) || 
				((rtype == kObjectType) && isXML(rhs) && atomToXMLObject(rhs)->hasSimpleContent()))
			{
				return ((*string(lhs) == *string(rhs)) ? trueAtom : falseAtom);
			}

			// 18. If Type(x) is Boolean, return the result of the comparison ToNumber(x) == y.
            if (ltype == kBooleanType)
                return eq(lhs&~7|kIntegerType, rhs);  // equal(toInteger(lhs), rhs)
			
			// 19. If Type(y) is Boolean, return the result of the comparison x == ToNumber(y).
            if (rtype == kBooleanType)
                return eq(lhs, rhs&~7|kIntegerType);  // equal(lhs, toInteger(rhs))

			// 20. If Type(x) is either String or Number and Type(y) is Object,
			// return the result of the comparison x == ToPrimitive(y).

            if ((isString(lhs) || isNumber(lhs)) && rtype == kObjectType)
				return eq(lhs, atomToScriptObject(rhs)->defaultValue());

			// 21. If Type(x) is Object and Type(y) is either String or Number,
			// return the result of the comparison ToPrimitive(x) == y.
            if ((isString(rhs) || isNumber(rhs)) && ltype == kObjectType)
				return eq(atomToScriptObject(lhs)->defaultValue(), rhs);
        }
		return falseAtom;
    }

    /**
     * this is the abstract relational comparison algorithm according to ECMA 262 11.8.5
     * @param lhs
     * @param rhs
     * @return trueAtom, falseAtom, or undefinedAtom
     */
    Atom AvmCore::compare(Atom lhs, Atom rhs)
    {
        // fixme - toprimitive must take number hint, so "7" becomes 7
		if ((lhs&7)==kIntegerType && (rhs&7)==kIntegerType)
		{
			// fast path for integers
			return lhs < rhs ? trueAtom : falseAtom;
		}

		lhs = primitive(lhs);
        rhs = primitive(rhs);
		
		if (isString(lhs) && isString(rhs))
        {
            // string compare. todo optimize!
			return *string(lhs) < *string(rhs) ? trueAtom : falseAtom;
        }

		// numeric compare
        double dx = number(lhs);
        double dy = number(rhs);
        if (MathUtils::isNaN(dx)) return undefinedAtom;
        if (MathUtils::isNaN(dy)) return undefinedAtom;
        return dx < dy ? trueAtom : falseAtom;
    }

    Atom AvmCore::stricteq(Atom lhs, Atom rhs)
    {
		if (isNull(lhs)) return isNull(rhs) ? trueAtom : falseAtom;
		if (isNull(rhs)) return isNull(lhs) ? trueAtom : falseAtom;

        int ltype = lhs & 7;
        int rtype = rhs & 7;
        if (ltype == rtype)
        {
            // same type
            switch (ltype)
            {
            case kSpecialType:
				return trueAtom; // undefined is the only kSpecialType atom 
            case kStringType:
                return (lhs==rhs || *string(lhs) == *string(rhs)) ? trueAtom : falseAtom;
            case kBooleanType:
            case kIntegerType:
            case kNamespaceType:
				return lhs == rhs ? trueAtom : falseAtom;
            case kObjectType:
			{
				if (lhs == rhs)
					return trueAtom;
				if (isXML(lhs) && isXML(rhs))
				{
					E4XNode *lhn = atomToXML (lhs);
					E4XNode *rhn = atomToXML (rhs);
					return ((lhn == rhn) ? trueAtom : falseAtom);
				}
				return falseAtom;
			}	
            case kDoubleType:
                // C++ portability note -- if either arg is NaN, java == returns false, which matches ECMA.
                return atomToDouble(lhs) == atomToDouble(rhs) ? trueAtom : falseAtom;
            }
        }
		// Sometimes ints can hide in double atoms (neg zero for one)
		else if ((ltype == kIntegerType) && (rtype == kDoubleType) || 
			(rtype == kIntegerType) && (ltype == kDoubleType))
		{
			return number(lhs) == number(rhs) ? trueAtom : falseAtom;
		}

        return falseAtom;
    }

	/**
	 * throwException will throw an exception.  This function
	 * never returns.  The exception to be thrown is specified 
	 * by exception.
	 */
	void AvmCore::throwException(Exception *exception)
	{
		#ifdef DEBUGGER
		if (debugger && !(exception->flags & Exception::SEEN_BY_DEBUGGER))
		{
			// I'm going to set the SEEN_BY_DEBUGGER flag now, before calling
			// filterException(), just to avoid reentrancy problems (we don't
			// want to end up back here with the same Exception object and get
			// stuck in an infinite loop).
			exception->flags |= Exception::SEEN_BY_DEBUGGER;

			if (!willExceptionBeCaught(exception))
			{
				// filterException() returns 'true' if it somehow let the user know
				// about the exception, 'false' if it ignored the exception.
				if (debugger->filterException(exception))
					exception->flags |= Exception::SEEN_BY_DEBUGGER;
				else
					exception->flags &= ~Exception::SEEN_BY_DEBUGGER;
			}
			else
			{
				exception->flags &= ~Exception::SEEN_BY_DEBUGGER;
			}
		}
		#endif

		// exceptionFrame should not be NULL; if it is,
		// you are missing a TRY/CATCH block around
		// a call to an AVM+ method that throws an
		// exception.
		AvmAssert(exceptionFrame != NULL);
		exceptionFrame->throwException(exception);
	}
	
	/**
	 * throwAtom will throw an exception.  This function
	 * never returns.  The exception to be thrown is specified
	 * by atom.
	 */
	void AvmCore::throwAtom(Atom atom)
	{
		throwException(new (GetGC()) Exception(atom
#ifdef DEBUGGER
			, this
#endif
			));
	}
	
#ifdef DEBUGGER
	bool AvmCore::willExceptionBeCaught(Exception* exception)
	{
		ExceptionFrame* ef;

		for (ef = exceptionFrame; ef != NULL; ef = ef->prevFrame)
		{
			switch (ef->catchAction)
			{
				// The CATCH block will consume any exception that occurs, and will not treat it as
				// an error, so exceptions should not be reported to the debugger.
				case kCatchAction_Ignore:
					return true;

				// The CATCH block will rethrow any exception that occurs; so, we will 'continue',
				// which will take us back to the 'for' loop to keep going up the exception stack,
				// until we find a frame with some other value.
				case kCatchAction_Rethrow:
					continue; // return to the 'for' loop

				// The CATCH block will treat any exception that occurs as an error -- probably by
				// calling uncaughtException, but possibly by some other means.  So, exceptions
				// should be reported to the debugger.
				case kCatchAction_ReportAsError:
					return false;

				// This is the unfortunate, but rare, case where we can't tell in advance how the
				// CATCH block will handle exceptions.  In this case, we wil not report the
				// exception to the debugger.
				case kCatchAction_Unknown:
					return true;

				// The CATCH block will walk up the exception frames that were defined in
				// ActionScript code -- e.g. "try { } catch (e:Error) { }" -- looking for one
				// which catch this exception.  So, we will do the same thing, and only report
				// the error to the debugger if there is no catch block for it.
				case kCatchAction_SearchForActionScriptExceptionHandler:
				{
					CallStackNode* callStackNode;

					// Walk all the way up the stack, one frame at a time, looking for
					// one which will catch this exception.
					for (callStackNode = callStack; callStackNode; callStackNode = callStackNode->next)
					{
						MethodInfo* info = (MethodInfo*) callStackNode->info;
						if (info->exceptions != NULL && callStackNode->eip && *callStackNode->eip)
						{
							// Check if this particular frame of the callstack
							// is going to catch the exception.
							if (findExceptionHandlerNoRethrow(info, *callStackNode->eip, exception) != NULL)
								return true;
						}
					}

					// No ActionScript handler was found, so the exception is going to be
					// re-thrown; so, 'continue' will get us back to the 'for' loop
					continue;
				}

				default:
					AvmAssert(false);
					break;
			}
		}

		return false;
	}

	/**
	 * Returns LANG_xx enum
	 */
	int AvmCore::determineLanguage()
	{
		if (langID < 0)
		{
			// @todo system probe to determine language, then return one of our known enums
			langID = LANG_en;
		}
		return langID;
	}

	String* AvmCore::findErrorMessage(int errorID,
									  int* mapTable,  /* 2 ints per entry i, i+1 */
									  const char** errorTable,
									  int numErrors)
	{
		// Above that, we must binary search.
		int lo = 0;
		int hi = numErrors-1;

		while (lo <= hi) {
			int pivot = (lo+hi)>>1;
			int testID = mapTable[2*pivot];
			if (testID == errorID) {
				lo = pivot;
				break;
			} else if (errorID < testID) {
				hi = pivot-1;
			} else {
				lo = pivot+1;
			}
		}

		int index = mapTable[2*lo+1];
		int id = mapTable[2*lo];

		if (id == errorID) {
			return newString(errorTable[index]);
		} else {
			return NULL;
		}
	}
#endif
	
	String* AvmCore::getErrorMessage(int errorID)
	{
		Stringp buffer = newString("Error #");
		buffer = concatStrings(buffer, internInt(errorID));

        #ifdef DEBUGGER
		Stringp out = findErrorMessage(errorID,
									   errorMappingTable,
									   errorConstants[determineLanguage()],
									   kNumErrorConstants);
		if (out) 
		{
			buffer = concatStrings(buffer, newString(": "));
			buffer = concatStrings(buffer, out);
		}
		else
		{
			AvmAssertMsg(0, "errorID not found in the message table; check ErrorConstants.cpp you may need to regenerate it");
		}
		#endif

		return buffer;
	}

	String* AvmCore::atomToErrorString(Atom a)
	{
		String* out = NULL;
	#ifdef DEBUGGER
		out = this->format(a);
	#else 
		(void)a;
		out = kEmptyString;
	#endif /* DEBUGGER */
		return out;
	}

	String* AvmCore::toErrorString(int d)
	{
		String* s = NULL;
	#ifdef DEBUGGER
		wchar buffer[256];
		buffer[255] = '\0';
		int len;
		if (MathUtils::convertIntegerToString(d, buffer, len)) 
			s = this->newString(buffer);
		else
			s = kEmptyString;
	#else
		s = kEmptyString;
		(void)d;
	#endif /* DEBUGGER */
		return s;
	}

	String* AvmCore::toErrorString(const char* s)
	{
		String* out = NULL;
	#ifdef DEBUGGER
		if (s)
			out = this->newString(s);
		else
			out = kEmptyString;
	#else
		out = kEmptyString;
		(void)s;
	#endif /* DEBUGGER */
		return out;
	}

	String* AvmCore::toErrorString(const wchar* s)
	{
		String* out = NULL;
	#ifdef DEBUGGER
		if (s)
			out = this->newString(s);
		else
			out = kEmptyString;
	#else
		out = kEmptyString;
		(void)s;
	#endif /* DEBUGGER */
		return out;
	}

	String* AvmCore::toErrorString(AbstractFunction* m)
	{
		String* s = NULL;
	#ifdef DEBUGGER
		if (m) 
			s = m->format(this);
		else
			s = kEmptyString;
	#else
		s = kEmptyString;
		(void)m;
	#endif /* DEBUGGER */
		return s;
	}

	String* AvmCore::toErrorString(Multiname* n)
	{
		String* s = NULL;
	#ifdef DEBUGGER
		if (n) 
			s = n->format(this, Multiname::MULTI_FORMAT_NAME_ONLY);
		else
			s = kEmptyString;
	#else
		s = kEmptyString;
		(void)n;
	#endif /* DEBUGGER */
		return s;
	}

	String* AvmCore::toErrorString(Namespace* ns)
	{
		String* s = NULL;
	#ifdef DEBUGGER
		if (ns) 
			s = ns->format(this);
		else
			s = kEmptyString;
	#else
		s = kEmptyString;
		(void)ns;
	#endif /* DEBUGGER */
		return s;
	}

	String* AvmCore::toErrorString(Traits* t)
	{
		#ifndef DEBUGGER
		(void)t;
		return kEmptyString;
		#else
		if (!t)
		{
			return newString("*");
		}

		String* s = NULL;
		if (t->base == traits.class_itraits)
		{
			t = t->itraits;
			s = newString("class ");
		}
		else
		{
			s = kEmptyString;
		}

		if (t->ns != NULL && t->ns != publicNamespace)
			s = concatStrings(s, concatStrings(toErrorString(t->ns), newString("."))); 

		if (t->name)
			s = concatStrings(s, t->name);
		else
			s = concatStrings(s, newString("(null)"));
		return s;
		#endif /* DEBUGGER */
	}

	String* AvmCore::formatErrorMessageV( int errorID, Stringp arg1, Stringp arg2, Stringp arg3)
	{
		Stringp out = NULL;

		Stringp errorMessage = getErrorMessage(errorID);
		if (errorMessage)
		{
			#ifdef DEBUGGER
			UTF8String *errorUTF8 = errorMessage->toUTF8String();		
			const char *format = errorUTF8->c_str();
			
			// This block is enclosed in {} to force
			// StringBuffer destructor to unwind.
			{
				StringBuffer buffer(this);
				buffer.formatP( format, arg1, arg2, arg3);
				out = newString(buffer.c_str());
			}
			#else	
			/** 
			 * We can skip all the above processing since getErrorMessage() only 
			 * returns the error code.  Likewise the toErrorString() methods return kEmptyString
			 */
			(void)arg1; 
			(void)arg2; 
			(void)arg3; 
			out = errorMessage;
			#endif /* DEBUGGER*/
		}
		else
		{
			AvmAssertMsg(errorMessage != NULL, "contract with getErrorMessage() broken, we should always get a string!");
			out = kEmptyString;
		}
		return out;
	}
		
	void AvmCore::throwErrorV(ClassClosure *type, int errorID, Stringp arg1, Stringp arg2, Stringp arg3)
	{
		Stringp out = formatErrorMessageV( errorID, arg1, arg2, arg3);

		#ifdef DEBUGGER
		if (type == NULL)
		{
			// print the error message, because we're still bootstrapping
			// and the exception type is not yet defined
			console << out << "\n";
		}
		#endif

		Atom args[3] = { nullObjectAtom, out->atom(), intToAtom(errorID) };
		throwAtom(type->construct(2, args));
	}
	
    Atom AvmCore::booleanAtom(Atom atom)
    {
		if (!AvmCore::isNullOrUndefined(atom))
		{
			switch (atom&7)
			{
			case kIntegerType:
				{
					int i = atom>>3;
					return urshift(i|-i,28)&~7 | kBooleanType;
				}
			case kBooleanType:
				return atom;
			case kObjectType:
			case kNamespaceType:
				return isNull(atom) ? falseAtom : trueAtom;
			case kStringType:
				if (isNull(atom)) return falseAtom;
				return (atomToString(atom)->length() > 0) ? trueAtom : falseAtom;
			default:
				{
					double d = atomToDouble(atom);
					return !MathUtils::isNaN(d) && d != 0.0 ? trueAtom : falseAtom;
				}
			}
		}
		else
		{
			return falseAtom;
		}
    }

	int AvmCore::boolean(Atom atom)
    {
		if (!AvmCore::isNullOrUndefined(atom))
		{
			switch (atom&7)
			{
			case kIntegerType:
			case kBooleanType:
				return (atom & ~7) != 0;
			case kObjectType:
			case kNamespaceType:
				return !isNull(atom);
			case kStringType:
				return !isNull(atom) && atomToString(atom)->length() > 0;
			default:
				{
					double d = atomToDouble(atom);
					return !MathUtils::isNaN(d) && d != 0.0;
				}
			}
		}
		else
		{
			return false;
		}
    }
	
	/*
		ToPrimitive:

		Return a default value for the Object. The default value of an object is
		retrieved by calling the internal [[DefaultValue]] method of the object,
		passing the optional hint PreferredType. The behaviour of the
		[[DefaultValue]] method is defined by this specification for all native
		ECMAScript objects (section 8.6.2.6).
	*/
	Atom AvmCore::primitive(Atom atom)
	{
		return isObject(atom) ? atomToScriptObject(atom)->defaultValue() : atom;
	}

    Atom AvmCore::numberAtom(Atom atom)
    {
		if (!isNull(atom))
		{
			double value;
			switch (atom&7)
			{
			case kSpecialType:
				return kNaN;
			case kStringType:
				value = atomToString(atom)->toNumber();
				break;
			default:
				AvmAssert(false);
			case kBooleanType:
				return (atom&~7) | kIntegerType;
			case kDoubleType:
			case kIntegerType:
				return atom;
			case kNamespaceType:
				// return ToNumber(namespace->uri)
				value = number(atomToNamespace(atom)->getURI()->atom());
				break;
			case kObjectType:
				value = number(atomToScriptObject(atom)->defaultValue());
				break;
			}
			return doubleToAtom(value);
		}
		else
		{
			return 0 | kIntegerType;
		}
    }
	
    double AvmCore::number(Atom atom) const
    {
		int kind = atom&7;

		if (kind == kIntegerType)
			return (double) ((sint32)atom>>3);
		if (kind == kDoubleType)
			return atomToDouble(atom);

		if (!isNull(atom))
		{
			switch (kind)
			{
			case kStringType:
				return atomToString(atom)->toNumber();
			case kSpecialType:
				return atomToDouble(kNaN);
			case kBooleanType:
				return (double) ((sint32)atom>>3);
			case kNamespaceType:
				return number(atomToNamespace(atom)->getURI()->atom());
			default: // number
			case kObjectType:
				return number(atomToScriptObject(atom)->defaultValue());
			}
		}
		else
		{
			// ES3 9.3, toNumber(null) == 0
			return 0;
		}
    }

    Stringp AvmCore::intern(Atom atom)
    {
		if (!isNull(atom))
		{
			switch (atom&7)
			{
			case kBooleanType:
				return booleanStrings[atom>>3];
			case kStringType:
				return internString(atom);
			case kNamespaceType:
				return atomToNamespace(atom)->getURI();
			case kSpecialType:
				return kundefined;
			case kObjectType:
				return intern(atomToScriptObject(atom)->toString());
			case kIntegerType:
				return internInt(atom>>3);
			case kDoubleType:
			default: // number
				return internDouble(atomToDouble(atom));
			}
		}
		else
		{
			return knull;
		}
    }

	Namespace* AvmCore::internNamespace(Namespace* ns)
	{
		if (ns->isPrivate())
		{
			// only intern namespaces with interned uri's.  this means anonymous
			// namespaces with null prefixes are always unique and can't be shared.
			return ns;
		}

		int i = findNamespace(ns);
		if (namespaces[i] == NULL)
		{
			// first time we've seen this namespace.  intern int
			nsCount++;
			namespaces[i] = ns;
			return ns;
		}

		// found the namespace, indexing by URI. return the interned copy.
		return namespaces[i];
	}

#ifdef AVMPLUS_VERBOSE
	/* static */
	void AvmCore::formatMultiname(PrintWriter& out, uint32 index, PoolObject* pool)
	{
		if (index > 0 && index <= pool->constantMnCount)
		{
			Multiname name;
			pool->parseMultiname(name, index);
			out << &name;
		}
		else
		{
			out << "invalid multiname index " << index;
		}
	}

    void AvmCore::formatOpcode(PrintWriter& buffer, const byte *pc, AbcOpcode opcode, int off, PoolObject* pool)
    {
		pc++;
		switch (opcode)
		{
		case OP_debugfile:
		case OP_pushstring:
			{
				buffer << opNames[opcode];
				uint32 index = readU30(pc);
				String *s = format(pool->cpool_string[index]->atom());
				if (index < pool->cpool_string.size())
					buffer << " " << s;
				break;
			}
		case OP_pushint:
			{
				buffer << opNames[opcode];
				uint32 index = readU30(pc);
				if (index < pool->cpool_int.size())
					buffer << " " << pool->cpool_int[index];
				break;
			}
		case OP_pushuint:
			{
				buffer << opNames[opcode];
				uint32 index = readU30(pc);
				if (index < pool->cpool_uint.size())
					buffer << " " << (double)pool->cpool_uint[index];
				break;
			}
		case OP_pushdouble:
			{
				buffer << opNames[opcode];
				uint32 index = readU30(pc);
				if (index < pool->cpool_double.size())
					buffer << " " << *pool->cpool_double[index];
				break;
			}
		case OP_pushnamespace:
			{
				buffer << opNames[opcode];
				uint32 index = readU30(pc);
				if (index < pool->cpool_ns.size())
                {
					buffer << " " << pool->cpool_ns[index]->getURI();
                }
				break;
			}
		case OP_getsuper: 
		case OP_setsuper: 
		case OP_getproperty: 
		case OP_setproperty: 
		case OP_initproperty: 
		case OP_findpropstrict: 
		case OP_findproperty:
		case OP_finddef:
		case OP_deleteproperty: 
		case OP_istype: 
		case OP_coerce: 
		case OP_astype: 
			{
				buffer << opNames[opcode] << " ";
				formatMultiname(buffer, readU30(pc), pool);
				break;
			}
		case OP_callproperty:
		case OP_callpropvoid:
		case OP_callproplex:
		case OP_callsuper:
		case OP_callsupervoid:
			{
				uint32 index = readU30(pc);
				int argc = readU30(pc);
				buffer << opNames[opcode] << " ";
				formatMultiname(buffer, index, pool);
				buffer << " " << argc;
				break;
			}
		case OP_callstatic:
		case OP_newfunction:
			{
				int method_id = readU30(pc);
				AbstractFunction* f = pool->methods[method_id];
				buffer << opNames[opcode] << " method_id=" << method_id;
				if (opcode == OP_callstatic)
				{
					buffer << " argc=" << (int)readU30(pc); // argc
				}
				if (f->name)
					buffer << " " << f->name;
				else
					buffer << " null";
				break;
			}

		case OP_newclass: 
			{
				AbstractFunction* c = pool->cinits[readU30(pc)];
				buffer << opNames[opcode] << " " << c;
				break;
			}
		case OP_lookupswitch:
			{
				int target = off + readS24(pc);
				pc += 3;
				int maxindex = readU30(pc);
				buffer << opNames[opcode] << " default:" << target << " maxcase:"<<maxindex;
				for (int i=0; i <= maxindex; i++)
				{
					target = off + readS24(pc);
					pc += 3;
					buffer << " " << target;
				}
				break;
			}
			break;

      case OP_ifnlt:
      case OP_ifnle:
      case OP_ifngt:
      case OP_ifnge:         
		case OP_jump:
		case OP_iftrue:
		case OP_iffalse:
		case OP_ifeq:
		case OP_ifge:
		case OP_ifgt:
		case OP_ifle:
		case OP_iflt:
		case OP_ifne:
		case OP_ifstricteq:
		case OP_ifstrictne:
			{
				int imm24 = 0, imm8 = 0;
				unsigned int imm30 = 0, imm30b = 0;
				const byte* p2 = pc-1;
				readOperands(p2, imm30, imm24, imm30b, imm8);
				int insWidth = p2-pc;

				int target = off + insWidth + imm24 + 1;
				buffer << opNames[opcode] << " " << (double)target;
				break;
			}
		default:
			switch (opOperandCount[opcode])
			{
			default:
				buffer << opNames[opcode];
				break;
			case 1:
				{
					buffer << opNames[opcode]
						<< ' '
						<< (int)readU30(pc);
				}
				break;
			case 2:
				{
					int first = readU30(pc);
					int second = readU30(pc);
					buffer << opNames[opcode]
						<< ' '
						<< first
						<< ' '
						<< second;
				}
				break;
			}
		}
    }
#endif

	ExceptionHandler* AvmCore::beginCatch(ExceptionFrame *ef,
		MethodInfo *info, sintptr pc, Exception *exception)
	{
		ef->beginCatch();
		ExceptionHandler* handler = findExceptionHandler(info,pc,exception);
		ef->beginTry(this);
		return handler;
	}

	ExceptionHandler* AvmCore::findExceptionHandler(MethodInfo *info,
												    sintptr pc,
												    Exception *exception)
	{
		ExceptionHandler* handler = findExceptionHandlerNoRethrow(info, pc, exception);
		if (handler)
			return handler;

		// We don't have a matching exception.
		throwException(exception);
		return NULL;// not reached
	}

	ExceptionHandler* AvmCore::findExceptionHandlerNoRethrow(MethodInfo *info,
															 sintptr pc,
															 Exception *exception)
	{
		// If this exception is an EXIT_EXCEPTION, it cannot
		// be caught by AS code.  Exit immediately.
		if (exception->flags & Exception::EXIT_EXCEPTION)
		{
			return NULL;
		}
		
		// Search the exception table for a catch clause
		// such that pc is between "from" and "to" and
		// the thrown atom matches the required type.

		// if no handler found, re-throw the exception from here

		//[ed] we only call this from methods with catch blocks, when exceptions != NULL
		AvmAssert(info->exceptions != NULL);

		int exception_count = info->exceptions->exception_count;
		ExceptionHandler* handler = info->exceptions->exceptions;
		Atom atom = exception->atom;
		
		while (--exception_count >= 0) 
		{
			if (pc >= handler->from &&
				pc <  handler->to)
			{
				// verifier makes sure type is valid, resolves to Traits*
				if (istype(atom, handler->traits)) 
				{
					#ifdef AVMPLUS_VERBOSE
					if (verbose)
					{
						console << "enter " << info << " catch " << handler->traits << '\n';
					}
					#endif // DEBUGGER

					return handler;
				}
			}
			handler++;
		}

		// We don't have a matching exception.
		return NULL;
	}

	void AvmCore::increment_d(Atom *ap, int delta)
	{
		AvmAssert(isNumber(*ap));
		if (isInteger(*ap))
			*ap = intToAtom(delta+((sint32)*ap>>3));
		else
			*ap = doubleToAtom(atomToDouble(*ap)+delta);
	}

	void AvmCore::increment_i(Atom *ap, int delta)
	{
		switch (*ap & 7)
		{
		case kBooleanType:
		case kIntegerType:
			*ap = intToAtom(delta+((sint32)*ap>>3));
			return;
		case kDoubleType:
			*ap = intToAtom((int)((sint32)atomToDouble(*ap)+delta));
			return;
        default:
			*ap = intToAtom(integer(*ap)+delta);
			return;
		}
	}

    bool AvmCore::istype(Atom atom, Traits* itraits)
    {
		if (!itraits)
			return true;

		if (isNull(atom))
			return itraits == traits.null_itraits;

		Traits* lhs;

		switch (atom&7)
		{
		case kNamespaceType:
			lhs = traits.namespace_itraits;
			break;

		case kStringType:
			lhs = traits.string_itraits;
			break;

		case kBooleanType:
			lhs = traits.boolean_itraits;
			break;

		case kIntegerType:
			// ISSUE need special support for number value ranges
			if (itraits == traits.number_itraits)
				return true;

			lhs = traits.int_itraits;
			if (itraits == traits.uint_itraits)
			{
				return (atom>>3) >= 0;
			}
			break;

		case kDoubleType:
			lhs = traits.number_itraits;
			// ISSUE there must be a better way...
			if (itraits == traits.int_itraits)
			{
				double d = atomToDouble(atom);
				int i = MathUtils::real2int(d);
				return d == (double)i;
			}
			if (itraits == traits.uint_itraits)
			{
				double d = atomToDouble(atom);
				// ISSUE use real2int?
				unsigned i = (unsigned)d;
				return d == (double)i;
			}
			break;

		case kSpecialType:
			return itraits == traits.void_itraits;

		case kObjectType: {
			lhs = atomToScriptObject(atom)->traits();
			break;
		}

		default:
			// unexpected atom type
			AvmAssert(false);
			return false;
		}

		return lhs->containsInterface(itraits)!=0;
    }

	Stringp AvmCore::coerce_s(Atom atom)
	{
		if (isNullOrUndefined(atom))
			return NULL;
		return string(atom);
	}

    Stringp AvmCore::string(Atom atom)
    {
		if (!isNull(atom))
		{
			switch (atom&7)
			{
			case kNamespaceType:
				return atomToNamespace(atom)->getURI();
			case kObjectType:
				return string(atomToScriptObject(atom)->toString());
			case kStringType:
				return atomToString(atom);
			case kSpecialType:
				return kundefined;
			case kBooleanType:
				return booleanStrings[atom>>3];
			case kIntegerType:
				return intToString (int(atom>>3));
			case kDoubleType:
			default: // number
				return doubleToString(atomToDouble(atom));
			}
		}
		else
		{
			return knull;
		}
    }

#ifdef AVMPLUS_PROFILE
	void AvmCore::dump()
	{
		sprof.dump(console);
		dprof.dump(console);
	}
#endif


	void AvmCore::setConsoleStream(OutputStream *stream)
	{
		console.setOutputStream(stream);
	}

	void AvmCore::registerNatives(NativeTableEntry *nativeMap, AbstractFunction *nativeMethods[])
	{
		while (nativeMap->method_id != -1)
		{
			AbstractFunction *f = NULL;

			switch (nativeMap->type)
			{
			case NativeTableEntry::kNativeMethod:
				f = new (GetGC()) NativeMethod(nativeMap->flags, (NativeMethod::Handler)nativeMap->handler);
				break;
			case NativeTableEntry::kNativeMethod1:
				f = new (GetGC()) NativeMethod(nativeMap->flags, (NativeMethod::Handler)nativeMap->handler, nativeMap->cookie);
				break;
			case NativeTableEntry::kNativeMethodV:
				f = new (GetGC()) NativeMethodV((NativeMethodV::Handler32)nativeMap->handler, nativeMap->flags);
				break;			
			case NativeTableEntry::kNativeMethodV1:
				f = new (GetGC()) NativeMethodV((NativeMethodV::Handler32)nativeMap->handler, nativeMap->cookie, nativeMap->flags);
				break;
			default:
				AvmAssert(false);
			}
            				
			// if we overwrite a native method mapping, something is hosed
			AvmAssert(nativeMethods[nativeMap->method_id]==NULL);
			nativeMethods[nativeMap->method_id] = f;
			nativeMap++;
		}
	}

	void AvmCore::initNativeTables(NativeClassInfo* classEntry,
								  NativeScriptInfo* scriptEntry,
								  AbstractFunction *nativeMethods[],
								  NativeClassInfo *nativeClasses[],
								  NativeScriptInfo *nativeScripts[])
	{
		while (classEntry->class_id != -1)
		{
			nativeClasses[classEntry->class_id] = classEntry;
			registerNatives(classEntry->nativeMap, nativeMethods);
			classEntry++;
		}

		while (scriptEntry->script_id != -1)
		{
			nativeScripts[scriptEntry->script_id] = scriptEntry;
			registerNatives(scriptEntry->nativeMap, nativeMethods);
			scriptEntry++;
		}
	}

	bool AvmCore::isXML (Atom atm) 
	{
		if (!isObject(atm))
			return false;

		AvmAssert (!traits.xml_itraits || traits.xml_itraits->final);
		Traits *lhs = atomToScriptObject(atm)->traits();
		return (lhs == traits.xml_itraits);
	}

	bool AvmCore::isDate(Atom atm)
	{
		if (!isObject(atm))
			return false;

		AvmAssert (!traits.date_itraits || traits.date_itraits->final);
		Traits *lhs = atomToScriptObject(atm)->traits();
		return (lhs == traits.date_itraits);
	}

	bool AvmCore::isXMLList (Atom atm) 
	{
		if (!isObject(atm))
			return false;

		AvmAssert (!traits.xmlList_itraits || traits.xmlList_itraits->final);
		Traits *lhs = atomToScriptObject(atm)->traits();
		return (lhs == traits.xmlList_itraits);
	}

	bool AvmCore::isQName (Atom atm)
	{
		if (!isObject(atm))
			return false;

		AvmAssert (!traits.qName_itraits || traits.qName_itraits->final);
		Traits *lhs = atomToScriptObject(atm)->traits();
		return (lhs == traits.qName_itraits);
	}

	bool AvmCore::isDictionary (Atom atm)
	{
		return isObject(atm) && atomToScriptObject(atm)->vtable->traits->isDictionary;
	}

	// Tables are from http://www.w3.org/TR/2004/REC-xml-20040204/#NT-NameChar
	// E4X 13.1.2.1, pg 63
	/* BaseChar = */
	wchar letterTable[] = {
		0x0041, 0x005A,
		0x0061, 0x007A,
		0x00C0, 0x00D6,
		0x00D8, 0x00F6,
		0x00F8, 0x00FF,
		0x0100, 0x0131,
		0x0134, 0x013E,
		0x0141, 0x0148,
		0x014A, 0x017E,
		0x0180, 0x01C3,
		0x01CD, 0x01F0,
		0x01F4, 0x01F5,
		0x01FA, 0x0217,
		0x0250, 0x02A8,
		0x02BB, 0x02C1,
		0x0386, 0x0386, // single
		0x0388, 0x038A,
		0x038C, 0x038C, // single
		0x038E, 0x03A1,
		0x03A3, 0x03CE,
		0x03D0, 0x03D6,
		0x03DA, 0x03DA, // single
		0x03DC, 0x03DC, // single
		0x03DE, 0x03DE, // single
		0x03E0, 0x03E0, // single
		0x03E2, 0x03F3,
		0x0401, 0x040C,
		0x040E, 0x044F,
		0x0451, 0x045C,
		0x045E, 0x0481,
		0x0490, 0x04C4,
		0x04C7, 0x04C8,
		0x04CB, 0x04CC,
		0x04D0, 0x04EB,
		0x04EE, 0x04F5,
		0x04F8, 0x04F9,
		0x0531, 0x0556,
		0x0559, 0x0559, // single
		0x0561, 0x0586,
		0x05D0, 0x05EA,
		0x05F0, 0x05F2,
		0x0621, 0x063A,
		0x0641, 0x064A,
		0x0671, 0x06B7,
		0x06BA, 0x06BE,
		0x06C0, 0x06CE,
		0x06D0, 0x06D3,
		0x06D5, 0x06D5, // single
		0x06E5, 0x06E6,
		0x0905, 0x0939,
		0x093D, 0x093D, // single
		0x0958, 0x0961,
		0x0985, 0x098C,
		0x098F, 0x0990,
		0x0993, 0x09A8,
		0x09AA, 0x09B0,
		0x09B2, 0x09B2, // single
		0x09B6, 0x09B9,
		0x09DC, 0x09DD,
		0x09DF, 0x09E1,
		0x09F0, 0x09F1,
		0x0A05, 0x0A0A,
		0x0A0F, 0x0A10,
		0x0A13, 0x0A28,
		0x0A2A, 0x0A30,
		0x0A32, 0x0A33,
		0x0A35, 0x0A36,
		0x0A38, 0x0A39,
		0x0A59, 0x0A5C,
		0x0A5E, 0x0A5E, // single
		0x0A72, 0x0A74,
		0x0A85, 0x0A8B,
		0x0A8D, 0x0A8D, // single
		0x0A8F, 0x0A91,
		0x0A93, 0x0AA8,
		0x0AAA, 0x0AB0,
		0x0AB2, 0x0AB3,
		0x0AB5, 0x0AB9,
		0x0ABD, 0x0ABD, // single
		0x0AE0, 0x0AE0, // single
		0x0B05, 0x0B0C,
		0x0B0F, 0x0B10,
		0x0B13, 0x0B28,
		0x0B2A, 0x0B30,
		0x0B32, 0x0B33,
		0x0B36, 0x0B39,
		0x0B3D, 0x0B3D, // single
		0x0B5C, 0x0B5D,
		0x0B5F, 0x0B61,
		0x0B85, 0x0B8A,
		0x0B8E, 0x0B90, 
		0x0B92, 0x0B95,
		0x0B99, 0x0B9A,
		0x0B9C, 0x0B9C, // single
		0x0B9E, 0x0B9F,
		0x0BA3, 0x0BA4,
		0x0BA8, 0x0BAA,
		0x0BAE, 0x0BB5,
		0x0BB7, 0x0BB9,
		0x0C05, 0x0C0C,
		0x0C0E, 0x0C10,
		0x0C12, 0x0C28,
		0x0C2A, 0x0C33,
		0x0C35, 0x0C39,
		0x0C60, 0x0C61,
		0x0C85, 0x0C8C,
		0x0C8E, 0x0C90,
		0x0C92, 0x0CA8,
		0x0CAA, 0x0CB3,
		0x0CB5, 0x0CB9,
		0x0CDE, 0x0CDE, // single
		0x0CE0, 0x0CE1,
		0x0D05, 0x0D0C,
		0x0D0E, 0x0D10,
		0x0D12, 0x0D28,
		0x0D2A, 0x0D39,
		0x0D60, 0x0D61,
		0x0E01, 0x0E2E,
		0x0E30, 0x0E30, //single
		0x0E32, 0x0E33,
		0x0E40, 0x0E45,
		0x0E81, 0x0E82,
		0x0E84, 0x0E84, // single
		0x0E87, 0x0E88,
		0x0E8A, 0x0E8A, // single
		0x0E8D, 0x0E8D, // single
		0x0E94, 0x0E97,
		0x0E99, 0x0E9F,
		0x0EA1, 0x0EA3,
		0x0EA5, 0x0EA5, // single
		0x0EA7, 0x0EA7, // single
		0x0EAA, 0x0EAB,
		0x0EAD, 0x0EAE,
		0x0EB0, 0x0EB0, // single
		0x0EB2, 0x0EB3,
		0x0EBD, 0x0EBD, // single
		0x0EC0, 0x0EC4,
		0x0F40, 0x0F47,
		0x0F49, 0x0F69,
		0x10A0, 0x10C5,
		0x10D0, 0x10F6,
		0x1100, 0x1100, // single
		0x1102, 0x1103,
		0x1105, 0x1107,
		0x1109, 0x1109, // single
		0x110B, 0x110C,
		0x110E, 0x1112,
		0x113C, 0x113C, // single
		0x113E, 0x113E, // single
		0x1140, 0x1140, // single
		0x114C, 0x114C, // single
		0x114E, 0x114E, // single
		0x1150, 0x1150, // single
		0x1154, 0x1155,
		0x1159, 0x1159, // single
		0x115F, 0x1161,
		0x1163, 0x1163, // single
		0x1165, 0x1165, // single
		0x1167, 0x1167, // single
		0x1169, 0x1169, // single
		0x116D, 0x116E,
		0x1172, 0x1173,
		0x1175, 0x1175, // single
		0x119E, 0x119E, // single
		0x11A8, 0x11A8, // single
		0x11AB, 0x11AB, // single
		0x11AE, 0x11AF,
		0x11B7, 0x11B8,
		0x11BA, 0x11BA, // single
		0x11BC, 0x11C2,
		0x11EB, 0x11EB, // single
		0x11F0, 0x11F0, // single
		0x11F9, 0x11F9, // single
		0x1E00, 0x1E9B,
		0x1EA0, 0x1EF9,
		0x1F00, 0x1F15,
		0x1F18, 0x1F1D,
		0x1F20, 0x1F45,
		0x1F48, 0x1F4D,
		0x1F50, 0x1F57,
		0x1F59, 0x1F59, // single
		0x1F5B, 0x1F5B, // single
		0x1F5D, 0x1F5D, // single
		0x1F5F, 0x1F7D,
		0x1F80, 0x1FB4,
		0x1FB6, 0x1FBC,
		0x1FBE, 0x1FBE, // single
		0x1FC2, 0x1FC4,
		0x1FC6, 0x1FCC,
		0x1FD0, 0x1FD3,
		0x1FD6, 0x1FDB,
		0x1FE0, 0x1FEC,
		0x1FF2, 0x1FF4,
		0x1FF6, 0x1FFC,
		0x2126, 0x2126, // single
		0x212A, 0x212B,
		0x212E, 0x212E, // single
		0x2180, 0x2182,
		0x3041, 0x3094,
		0x30A1, 0x30FA,
		0x3105, 0x312C,
		0xAC00, 0xD7A3,
		//[86]   	Ideographic	   ::=   	
		0x4E00, 0x9FA5,
		0x3007, 0x3007, // single
		0x3021, 0x3029
		};

	bool AvmCore::isLetter (wchar c)
	{
		int x = sizeof(letterTable) / (sizeof(wchar));
		for (int i = 0; i < x; i += 2)
		{
			if (c >= letterTable[i] && c <= letterTable[i+1])
				return true;
		}
		return false;
	}

//[87]   	CombiningChar	   ::=   	
	wchar combiningCharTable[] = {
		0x0300, 0x0345,
		0x0360, 0x0361,
		0x0483, 0x0486,
		0x0591, 0x05A1,
		0x05A3, 0x05B9,
		0x05BB, 0x05BD,
		0x05BF, 0x05BF, // single
		0x05C1, 0x05C2,
		0x05C4, 0x05C4, // single
		0x064B, 0x0652,
		0x0670, 0x0670, // single
		0x06D6, 0x06DC,
		0x06DD, 0x06DF,
		0x06E0, 0x06E4,
		0x06E7, 0x06E8,
		0x06EA, 0x06ED,
		0x0901, 0x0903,
		0x093C, 0x093C, // single
		0x093E, 0x094C,
		0x094D, 0x094D, // single
		0x0951, 0x0954,
		0x0962, 0x0963,
		0x0981, 0x0983,
		0x09BC, 0x09BC, // single 
		0x09BE, 0x09BE, // single 
		0x09BF, 0x09BF, // single 
		0x09C0, 0x09C4,
		0x09C7, 0x09C8,
		0x09CB, 0x09CD,
		0x09D7, 0x09D7, // single 
		0x09E2, 0x09E3,
		0x0A02, 0x0A02, // single 
		0x0A3C, 0x0A3C, // single 
		0x0A3E, 0x0A3E, // single 
		0x0A3F, 0x0A3F, // single 
		0x0A40, 0x0A42,
		0x0A47, 0x0A48,
		0x0A4B, 0x0A4D,
		0x0A70, 0x0A71,
		0x0A81, 0x0A83,
		0x0ABC, 0x0ABC, // single 
		0x0ABE, 0x0AC5,
		0x0AC7, 0x0AC9,
		0x0ACB, 0x0ACD,
		0x0B01, 0x0B03,
		0x0B3C, 0x0B3C, // single 
		0x0B3E, 0x0B43,
		0x0B47, 0x0B48,
		0x0B4B, 0x0B4D,
		0x0B56, 0x0B57,
		0x0B82, 0x0B83,
		0x0BBE, 0x0BC2,
		0x0BC6, 0x0BC8,
		0x0BCA, 0x0BCD,
		0x0BD7, 0x0BD7, // single 
		0x0C01, 0x0C03,
		0x0C3E, 0x0C44,
		0x0C46, 0x0C48,
		0x0C4A, 0x0C4D,
		0x0C55, 0x0C56,
		0x0C82, 0x0C83,
		0x0CBE, 0x0CC4,
		0x0CC6, 0x0CC8,
		0x0CCA, 0x0CCD,
		0x0CD5, 0x0CD6,
		0x0D02, 0x0D03,
		0x0D3E, 0x0D43,
		0x0D46, 0x0D48,
		0x0D4A, 0x0D4D,
		0x0D57, 0x0D57, // single 
		0x0E31, 0x0E31, // single 
		0x0E34, 0x0E3A,
		0x0E47, 0x0E4E,
		0x0EB1, 0x0EB1, // single 
		0x0EB4, 0x0EB9,
		0x0EBB, 0x0EBC,
		0x0EC8, 0x0ECD,
		0x0F18, 0x0F19,
		0x0F35, 0x0F35, // single 
		0x0F37, 0x0F37, // single 
		0x0F39, 0x0F39, // single 
		0x0F3E, 0x0F3E, // single 
		0x0F3F, 0x0F3F, // single 
		0x0F71, 0x0F84,
		0x0F86, 0x0F8B,
		0x0F90, 0x0F95,
		0x0F97, 0x0F97, // single 
		0x0F99, 0x0FAD,
		0x0FB1, 0x0FB7,
		0x0FB9, 0x0FB9, // single 
		0x20D0, 0x20DC,
		0x20E1, 0x20E1, // single 
		0x302A, 0x302F,
		0x3099, 0x3099, // single 
		0x309A, 0x309A // single 
		};
	bool AvmCore::isCombiningChar (wchar c)
	{
		int x = sizeof(combiningCharTable) / (sizeof(wchar));
		for (int i = 0; i < x; i += 2)
		{
			if (c >= combiningCharTable[i] && c <= combiningCharTable[i+1])
				return true;
		}
		return false;
	}

//[88]   	Digit	   ::=   	
	wchar digitTable[] = {
		0x0030, 0x0039,
		0x0660, 0x0669,
		0x06F0, 0x06F9,
		0x0966, 0x096F,
		0x09E6, 0x09EF,
		0x0A66, 0x0A6F,
		0x0AE6, 0x0AEF,
		0x0B66, 0x0B6F,
		0x0BE7, 0x0BEF,
		0x0C66, 0x0C6F,
		0x0CE6, 0x0CEF,
		0x0D66, 0x0D6F,
		0x0E50, 0x0E59,
		0x0ED0, 0x0ED9,
		0x0F20, 0x0F29};

	bool AvmCore::isDigit (wchar c)
	{
		int x = sizeof(digitTable) / (sizeof(wchar));
		for (int i = 0; i < x; i += 2)
		{
			if (c >= digitTable[i] && c <= digitTable[i+1])
				return true;
		}
		return false;
	}

	wchar extenderTable[] = {
		0x00B7, 0x00B7, // single 
		0x02D0, 0x02D0, // single 
		0x02D1, 0x02D1, // single 
		0x0387, 0x0387, // single 
		0x0640, 0x0640, // single 
		0x0E46, 0x0E46, // single 
		0x0EC6, 0x0EC6, // single 
		0x3005, 0x3005, // single 
		0x3031, 0x3035, 
		0x309D, 0x309E,
		0x30FC, 0x30FE};
	bool AvmCore::isExtender (wchar c)
	{
		int x = sizeof(extenderTable) / (sizeof(wchar));
		for (int i = 0; i < x; i += 2)
		{
			if (c >= extenderTable[i] && c <= extenderTable[i+1])
				return true;
		}
		return false;
	}

	bool AvmCore::isXMLName(Atom arg)
	{
		if (isNullOrUndefined(arg))
			return false;

		Stringp p = string(arg);

		// http://www.w3.org/TR/2004/REC-xml-20040204/#NT-NameChar

		// Name is (Letter | _ or :) followed by arbitrary number of NameChar

		if (!p->length())
			return false;

		// According to the Mozilla testcase...
		// e4x excludes ':'

		wchar c = (*p)[0];
		if (!isLetter (c) && c != '_' /*&& c != ':'*/)
			return false;		

		for (int i = 1; i < p->length(); i++)
		{
			wchar c = (*p)[i];

			if (isDigit(c) || isLetter(c) || (c == '.') || (c == '-') || (c == '_') || /*(c != ':') ||*/
				isCombiningChar (c) || isExtender(c))
				continue;

			return false;
		}

		return true;
	}

	Stringp AvmCore::ToXMLString (Atom a)
	{
		if (!isNull(a))
		{
			switch (a&7)
			{
			case kStringType:
				return EscapeElementValue (string(a), true);
			case kObjectType:
			case kNamespaceType:
				if (isXML(a))
				{
					XMLObject *x = atomToXMLObject (a);
					return x->toXMLString();
				}
				else if (isXMLList(a))
				{
					XMLListObject *x = atomToXMLList (a);
					return x->toXMLString();
				}
				else
				{
					// !!@ to primitive (hint string first)
					// !!@ namespace case falls into this as well
					return EscapeElementValue (string(a), true);
				}
				break;
			case kSpecialType:
				return kundefined;
			case kIntegerType:
			case kBooleanType:
			case kDoubleType:
			default:
				return string(a);
			}
		}
		else
		{
			return knull;
		}
	}

	Stringp AvmCore::EscapeElementValue (const Stringp s, bool removeLeadingTrailingWhitespace)
	{
		StringBuffer output(this);

		int i = 0;
		int last = s->length() - 1;
		if (removeLeadingTrailingWhitespace)
		{
			// finding trailing whitespace
			while (last >= 0)
			{
				if (!String::isSpace ((*s)[last]))
					break;

				last--;
			}

			if (last < 0)
				return kEmptyString;

			// skip leading whitespace
			for (i = 0; i <= last; i++)
			{
				if (!String::isSpace ((*s)[i]))
					break;
			}
		}

		while (i <= last)
		{
			switch ((*s)[i])
			{
			case '<':
				output << "&lt;";
				break;
			case '>':
				output << "&gt;";
				break;
			case '&':
				output << "&amp;";
				break;
			default:
				output << ((*s)[i]);
			}

			i++;
		}

		return newString (output.c_str());
	}

	Stringp AvmCore::EscapeAttributeValue (Atom v)
	{
		StringBuffer output(this);

		Stringp s = string (v);

		for (int i = 0; i < s->length(); i++)
		{
			switch ((*s)[i])
			{
			case '"':
				output << "&quot;";
				break;
			case '<':
				output << "&lt;";
				break;
			case '&':
				output << "&amp;";
				break;
			case 0x000a:
				output << "&#xA;";
				break;
			case 0x000d:
				output << "&#xD;";
				break;
			case 0x0009:
				output << "&#x9;";
				break;
			default:
				output << ((*s)[i]);
			}
		}

		return newString (output.c_str());
	}

	XMLObject *AvmCore::atomToXMLObject (Atom atm) 
	{
		if (!isXML (atm))
			return 0;

		return (XMLObject*)(atomToScriptObject(atm));
	}

	E4XNode *AvmCore::atomToXML (Atom atm) 
	{
		if (!isXML (atm))
			return 0;

		return ((XMLObject*)(atomToScriptObject(atm)))->getNode();
	}

	XMLListObject *AvmCore::atomToXMLList (Atom atm) 
	{
		if (!isXMLList (atm))
			return 0;

		return (XMLListObject*)(atomToScriptObject(atm));
	}

	QNameObject *AvmCore::atomToQName (Atom atm) 
	{
		if (!isQName (atm))
			return 0;

		return (QNameObject*)(atomToScriptObject(atm));
	}

	Stringp AvmCore::_typeof (Atom arg)
	{
		if (!isNull(arg))
		{
			switch (arg&7)
			{
			default:
			case kObjectType:
				if (isXML (arg) || isXMLList(arg))
				{
					return kxml;
				}
				else if (isFunction(arg))
				{
					return kfunction; // No special type code for functions, but we need to
									//  special case to return 'function' here.
				}
				else
				{
					return kobject;
				}

			case kBooleanType:
				return kboolean;

			case kIntegerType:
			case kDoubleType:
				return knumber;

			case kSpecialType:
				return kundefined;

			case kStringType:
				return kstring;

			case kNamespaceType:
				return kobject;
			}
		}
		else
		{
			// typeof(null) = "object"
			return kobject;
		}
	}

	size_t AvmCore::getToplevelSize() const
	{
		return sizeof(Toplevel);
	}
	
	Toplevel* AvmCore::createToplevel(VTable *vtable)
	{
		return new (GetGC(), vtable->getExtraSize()) Toplevel(vtable, NULL);
	}

	void AvmCore::presweep()
	{
        // clear out the string table
		{
			for (int i=0, n=numStrings; i < n; i++)
			{
				if (strings[i] > AVMPLUS_STRING_DELETED && !GetGC()->GetMark(strings[i]))
				{
					strings[i] = AVMPLUS_STRING_DELETED;
					deletedCount++;
					stringCount--;
				}
			}
		}


		// do the same for the namespaces
		{
			bool rehashFlag = false;
			for (int i=0, n=numNamespaces; i < n; i++)
			{
				if (namespaces[i] != NULL && !GetGC()->GetMark(namespaces[i]))
				{
					rehashFlag = true;
					namespaces[i] = NULL;
				}
			}

			// if any interned strings were freed, rehash the intern table
			// todo - make this less aggressive
			if (rehashFlag)
				rehashNamespaces(numNamespaces);
		}

#ifdef DEBUGGER
		if(stackTraces)
		{
			uint32 n = GetGC()->Size(stackTraces)/sizeof(StackTrace*);
			n = 1<<FindOneBit(n);
			bool rehash= false;
			for (uint32 i=0; i<n; i++)
			{
				if(stackTraces[i] && !GetGC()->GetMark(stackTraces[i]))
				{
					rehash = true;
					stackTraces[i] = NULL;
				}
			}
			if(rehash)
				rehashTraces(n);
		}
#endif
    }

	bool wcharEquals(const wchar *s1, const wchar *s2)
	{
		while (*s1) {
			if (*s1 != *s2) {
				return false;
			}
			s1++;
			s2++;
		}
		return true;
	}

	int hashString(const wchar *ptr, int len)
	{
		int hashCode = 0;
		while (len--) {
			hashCode = (hashCode >> 28) ^ (hashCode << 4) ^ *ptr++;
		}
		return hashCode;
	}

    int AvmCore::findString(const wchar *s, int len)
    {
        int m = numStrings;
		// 80% load factor
		if (5*(stringCount+deletedCount+1) > 4*m) {
			if (2*stringCount > m) // 50%
			    rehashStrings(m = m << 1);
			else
				rehashStrings(m);
		}

        // compute the hash function
        int hashCode = hashString(s, len);

		int bitMask = m - 1;

        // find the slot to use
        int i = (hashCode&0x7FFFFFFF) & bitMask;
        int n = 7;
		Stringp k;
		if (!deletedCount)
		{
			while ((k=strings[i]) != NULL && !k->FastEquals(s,len)) {
				i = (i + (n++)) & bitMask; // quadratic probe
			}
		}
		else
		{
			int iFirstDeletedSlot = -1;
			while ((k=strings[i]) != NULL)
			{
				if (k == AVMPLUS_STRING_DELETED)
				{
					if (iFirstDeletedSlot == -1)
					{
						iFirstDeletedSlot = i;
					}
				}
				else if (k->FastEquals (s, len))
				{
					return i;
				}
				i = (i + (n++)) & bitMask; // quadratic probe
			}

			if ((k == NULL) && (iFirstDeletedSlot != -1))
				return iFirstDeletedSlot;

		}
        return i;
    }

	/**
	 * namespace hash search.  interned namespaces are as unique as their
	 * uri.  We assume uri's are already interned, so interning a namespace
	 * is quick because uri's can be compared quickly.
	 */
	int AvmCore::findNamespace(const Namespace* ns)
	{
        int m = numNamespaces;
		// 80% load factor
        if (nsCount*5 >= 4*m) {
            rehashNamespaces(m = m << 1);
		}

        // compute the hash function
		int hashCode = ((uintptr)ns->getURI())>>3;

		int bitMask = m - 1;

        // find the slot to use
        int i = (hashCode&0x7FFFFFFF) & bitMask;
        int n = 7;
		Namespace* k;
        while ((k=namespaces[i]) != NULL && k->m_uri != ns->m_uri ) {
            i = (i + (n++)) & bitMask; // quadratic probe
		}
        return i;
	}

    /**
     * intern the given string atom which has already been allocated
     * @param atom
     * @return
     */
    Stringp AvmCore::internString(Stringp o)
    {
		if (o->isInterned())
			return o;

		int i = findString(o->c_str(), o->length());
		Stringp other;
		if ((other=strings[i]) <= AVMPLUS_STRING_DELETED)
		{
			if (other == AVMPLUS_STRING_DELETED)
			{
				deletedCount--;
				AvmAssert(deletedCount >= 0);
			}
			stringCount++;
			strings[i] = o;
			o->setInterned(this);
			return o;
		}
		else
		{
			return other;
		}
    }

	Stringp AvmCore::internString(Atom atom)
    {
		AvmAssert(isString(atom));
		Stringp s = atomToString(atom);
		return s->isInterned() ? s : internString(s);
    }

    Stringp AvmCore::internInt(int value)
    {
		wchar buffer[65];
		int len;
		MathUtils::convertIntegerToString(value, buffer, len);
		return internAlloc(buffer, len);

		// This optimized routine below works fine and is faster than calling
		// convertIntegerToString but with our support of integer keys in our
		// HashTables, this routine is no longer on a critical path.  Save some
		// code by leaving it disabled unless we can show a performance gain by
		// using it.
#if 0
		// optimized case of MathUtils;:convertIntegerToString

		if ((uint32)value == 0x80000000) // MathUtils::convertIntegerToString doesn't deal with this number because you can't negate it.
		{
			UnicodeUtils::Utf8ToUtf16((uint8*)"-2147483648", 12, buffer, 24);
			return internAlloc(buffer, 11);
		}

		wchar *src = &buffer[39];
		*src-- = '\0';

		if (value == 0)
		{
			*src-- = '0';
		}
		else
		{
			uint32 uVal;
			bool negative = (value < 0);
			if (negative)
				value = -value;

			uVal = (uint32)value;

			while (uVal != 0)
			{
				uint32 j = uVal;
				uVal = uVal / 10;
				j -= (uVal * 10);

				*src-- = (j + '0');
			}

			if (negative)
				*src-- = '-';
		}

		return internAlloc(src + 1, &buffer[39] - src - 1);
#endif
    }
	Stringp AvmCore::internUint32 (uint32 ui)
	{ 
		if (ui & 0x80000000)
			return internDouble(ui);
		else
			return internInt((int)ui);  
	} 

    Stringp AvmCore::internDouble(double d)
    {
		wchar buffer[256];
		int len;
		MathUtils::convertDoubleToString(d, buffer, len);
		return internAlloc(buffer, len);
    }

	Stringp AvmCore::internAllocUtf8(const byte *cs, int len8)
	{
		int len16 = UnicodeUtils::Utf8Count((const uint8*)cs, len8);
		Stringp s = new (GetGC()) String((const char *)cs, len8, len16);

		int i = findString(s->c_str(), len16);
		Stringp other;
		if ((other=strings[i]) <= AVMPLUS_STRING_DELETED)
		{
			if (other == AVMPLUS_STRING_DELETED)
			{
				deletedCount--;
				AvmAssert(deletedCount >= 0);
			}

			stringCount++;
			strings[i] = s;
			s->setInterned(this);
			return s;
		}
		else
		{
			// we know the internal buf has not yet been aliased, so it's safe to explicitly free here.
			delete s;
			return other;
		}
	}

	
	Stringp AvmCore::internAllocAscii(const char *cs)
	{
		int len = strlen(cs);
		wchar buffer[256];

		AvmAssert(len < 256);
		
		for(int j=0; j<len; j++)
		{
			buffer[j] = cs[j];
		}
		buffer[len] = 0;

		int i = findString(buffer, len);
		Stringp s;
		if ((s=strings[i]) <= AVMPLUS_STRING_DELETED)
		{
			if (s == AVMPLUS_STRING_DELETED)
			{
				deletedCount--;
				AvmAssert(deletedCount >= 0);
			}
			s = new (GetGC()) String(buffer, len);
			stringCount++;
			strings[i] = s;
			s->setInterned(this);
		}
		return s;
	}


	Stringp AvmCore::internAlloc(const wchar *s, int len)
	{
        int i = findString(s, len);
		Stringp other;
        if ((other=strings[i]) <= AVMPLUS_STRING_DELETED)
        {
			if (other == AVMPLUS_STRING_DELETED)
			{
				deletedCount--;
				AvmAssert(deletedCount >= 0);
			}
            
#ifdef DEBUGGER			
			DRC(Stringp) *oldStrings = strings;
#endif

			other = new (GetGC()) String(s,len);
			
#ifdef DEBUGGER
			// re-find if String ctor caused rehash
			if(strings != oldStrings)
				i = findString(s, len);
#endif
			strings[i] = other;
			stringCount++;
			other->setInterned(this);
		}
		return other;
	}

	void AvmCore::rehashStrings(int newlen)
    {
        // rehash

		DRC(Stringp) *oldStrings = strings;
		int oldStringCount = numStrings;

		strings = new DRC(Stringp)[newlen];
		memset(strings, 0, newlen*sizeof(Stringp));
		numStrings = newlen;

#ifdef _DEBUG // debug sanity checks
		int oldDeletedCount = deletedCount;
		int computedDeleteCount = 0;
		int computedStringCount = 0;
#endif

		deletedCount = 0;

		// Inlined and optimized our findString routine.  We know that there
		// are no duplicated strings in our intern string table so we don't 
		// need to call Equals.  All we need to do is find the first blank
		// spot available.
        int m = numStrings;
		int bitMask = m - 1;

        for (int i=0; i < oldStringCount; i++)
        {
			Stringp o = oldStrings[i];
            if (o > AVMPLUS_STRING_DELETED)
            {
				// compute the hash function
				int hashCode = hashString(o->c_str(), o->length());

				// find the slot to use
				int j = (hashCode&0x7FFFFFFF) & bitMask;
				int n = 7;
				while (strings[j] != NULL) {
					j = (j + (n++)) & bitMask; // quadratic probe
				}

                strings[j] = o;
#ifdef _DEBUG
				computedStringCount++;
#endif
            }
#ifdef _DEBUG
			else if (o == AVMPLUS_STRING_DELETED)
			{
				computedDeleteCount++;
			}
#endif
        }

#ifdef _DEBUG
		AvmAssert(computedStringCount == stringCount);
		AvmAssert(oldDeletedCount == computedDeleteCount);
#endif

		// Clear oldStrings so it can be collected.
		delete [] oldStrings;
    }

	void AvmCore::rehashNamespaces(int newlen)
    {
        // rehash

		DRC(Namespacep) *old = namespaces;
		int oldCount = numNamespaces;

		namespaces = new DRC(Namespacep)[newlen];
		memset(namespaces, 0, newlen*sizeof(Namespace*));
		numNamespaces = newlen;
		
        for (int i=0; i < oldCount; i++)
        {
			Namespace* o = old[i];
            if (o != NULL)
                namespaces[findNamespace(o)] = o;
        }

		// Clear old namespaces table so it can be collected.
		delete [] old;
    }
		
	ScriptBufferImpl* AvmCore::newScriptBuffer(size_t size)
	{
		return new (GetGC(), size) BasicScriptBufferImpl(size);
	}
	
	VTable* AvmCore::newVTable(Traits* traits, VTable* base, ScopeChain* scope,
		AbcEnv* abcEnv, Toplevel* toplevel)
	{
		size_t extraSize = sizeof(MethodEnv*)*(traits->methodCount > 0 ? traits->methodCount-1 : 0);
		return new (GetGC(), extraSize) VTable(traits, base, scope, abcEnv, toplevel);
	}

	RegExpObject* AvmCore::newRegExp(RegExpClass* regexpClass,
								  Stringp pattern,
								  Stringp options)
	{
		return new (GetGC(), regexpClass->ivtable()->getExtraSize()) RegExpObject(regexpClass,
														 pattern, options);
	}
	
	ScriptObject* AvmCore::newObject(VTable *vtable, ScriptObject *delegate)
	{
		return new (GetGC(), vtable->getExtraSize()) ScriptObject(vtable, delegate);
	}

	ScriptObject* AvmCore::newActivation(VTable *vtable, ScriptObject *delegate)
	{
		ScriptObject* obj = new (GetGC(), vtable->getExtraSize()) ScriptObject(vtable, delegate);
		if(vtable->init)
		{
			Atom argv[1];
			argv[0] = obj->atom(); // new object is receiver
			MethodEnv* init = vtable->init;
			init->coerceEnter(0, argv);
		}
		return obj;
	}

    Namespace* AvmCore::newNamespace(Atom prefix, Atom uri, Namespace::NamespaceType type)
	{
		// E4X - this is 13.2.3, step 3 - prefix IS specified

		Atom p;
		Stringp u;
		if (isQName (uri) && !isNull(atomToQName (uri)->getURI()))
		{
			u = atomToString(atomToQName (uri)->getURI());
		}
		else
		{
			u = internString (string (uri));
		}
		if (u == kEmptyString)
		{
			if (prefix == undefinedAtom)
				p = kEmptyString->atom();
			else if (!string (prefix)->length())
				p = kEmptyString->atom();
			else
			{
				// !!@ throw correct type error
				//typeErrorClass()->throwError(kConvertUndefinedToObjectError);
				return NULL;
			}
		}
		else if (prefix == undefinedAtom)
		{
			p = undefinedAtom;
		}
		else if (prefix != kEmptyString->atom() && !isXMLName (prefix))
		{
			p = undefinedAtom;
		}
		else
		{
			p = internString (string (prefix))->atom();
		}

        return new (GetGC()) Namespace(p, u, type);
	}

	Namespace* AvmCore::newNamespace(Atom uri, Namespace::NamespaceType type)
	{
		// prefix and uri must be interned!
		// E4X - this is 13.2.2, step 3 - "prefix not specified"

		if (isNamespace (uri))
		{
			Namespace *ns = atomToNamespace (uri);
			return new (GetGC()) Namespace (ns->getPrefix(), ns->getURI(), type);
		}
		else if (isObject(uri) && isQName (uri) && !isNull(atomToQName (uri)->getURI()))
		{
			return new (GetGC()) Namespace (undefinedAtom, atomToString(atomToQName (uri)->getURI()), type);
		}
		else
		{
			Stringp u = internString (string (uri));
			Atom prefix = (u == kEmptyString) ? kEmptyString->atom() : undefinedAtom;
			return new (GetGC()) Namespace (prefix, u, type);
		}
	}

	Namespace* AvmCore::newNamespace(Stringp uri, Namespace::NamespaceType type)
	{
		uri = internString(uri);
		Atom prefix = (uri == kEmptyString) ? kEmptyString->atom() : undefinedAtom;
		return new (GetGC()) Namespace(prefix, uri, type);
	}

	NamespaceSet* AvmCore::newNamespaceSet(int nsCount)
	{
		size_t extra = (nsCount >= 1 ? nsCount-1 : 0)*sizeof(Atom);
		return new (GetGC(), extra) NamespaceSet(nsCount);
	}

	Atom AvmCore::uintToAtom(uint32 n)
	{
		// As kIntegerType is signed, we can only represent a 28-bit uint in it
		if (!(n&0xF0000000)) {
			return (n<<3) | kIntegerType;
		} else {
			return allocDouble(n);
		}
	}
			
	Atom AvmCore::intToAtom(int n)
	{
		// handle integer values w/out allocation
		int i29 = n << 3;
		if ((i29>>3) == n)
		{
			return i29 | kIntegerType;
		}
		else 
		{
			return allocDouble(n);
		}
	}

#if defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
	// ignore warning that inline asm disables global optimization in this function
	#ifdef _MSC_VER
	#pragma warning(disable: 4740) 
	#endif
	Atom AvmCore::doubleToAtom_sse2(double n)
	{
		#ifdef AVMPLUS_PROFILE
		if (dprof.dprofile)
			DynamicProfiler::StackMark mark(OP_doubletoatom, &dprof);
		#endif

		// handle integer values w/out allocation
		// this logic rounds in the wrong direction for E3, but
		// we never use a rounded value, only cleanly converted values.
		#ifdef WIN32 
		int id3;
		_asm {
			movsd xmm0,n
			cvttsd2si ecx,xmm0
			shl ecx,3		// id<<3
			mov eax,ecx
			sar ecx,3		// id>>3
			cvtsi2sd xmm1,ecx
			ucomisd xmm0,xmm1
            jne d2a_alloc   // < or >
			jp  d2a_alloc   // unordered
			mov id3,eax
		}


		if (id3 != 0 || !MathUtils::isNegZero(n))
		{
			return id3 | kIntegerType;
		}
		else
		{
			_asm d2a_alloc:
			return allocDouble(n);
		}
		#elif defined(_MAC) && (defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64))
		int id = _mm_cvttsd_si32(_mm_set_sd(n));
		// MacTel is luckily always using SSE2, there
		// are no intrinsics to check for unordered 
		// mode here using any of the _mm_ucominXXX
		// instructions
		if (((id<<3)>>3) == n) {
			// make sure its not -0
			if (id == 0 && MathUtils::isNegZero(n)) {
				return allocDouble(n);
			} else {
				return (id<<3) | kIntegerType;
			}
		}
		return allocDouble(n);
		#elif AVMPLUS_LINUX
		int id3;
		asm("movups %1, %%xmm0;"
			"cvttsd2si %%xmm0, %%ecx;"
			"shl $0x3, %%ecx;"
			"mov %%ecx, %%eax;"
			"sar $0x3, %%ecx;"
			"cvtsi2sd %%ecx, %%xmm1;"
			"ucomisd %%xmm1, %%xmm0;"
			"jne d2a_alloc;"
			"jp d2a_alloc;"
			"movl %%eax, %0" : "=r" (id3) : "m" (n));

		if (id3 != 0 || !MathUtils::isNegZero(n))
		{
			return id3 | kIntegerType;
		}

		asm("d2a_alloc:");
		#endif
		return allocDouble(n);
	}
#endif

	Atom AvmCore::doubleToAtom(double n)
	{
		#ifdef AVMPLUS_PROFILE
		if (dprof.dprofile)
			DynamicProfiler::StackMark mark(OP_doubletoatom, &dprof);
		#endif

		// There is no need for special logic for NaN or +/-Inf since we don't
        // ever test for those values in coreplayer.  As far as we're concerned
        // they are regular numeric values.

		// handle integer values w/out allocation
		#ifdef WIN32
		// this logic rounds in the wrong direction for E3, but
		// we never use a rounded value, only cleanly converted values.
		int id;
		_asm {
			fld [n];
			fistp [id];
		}
		#elif defined(_MAC) && (defined (AVMPLUS_IA32) || defined(AVMPLUS_AMD64))
		int id = _mm_cvttsd_si32(_mm_set_sd(n));
		#else
		int id = MathUtils::real2int(n);
		#endif

		// make sure n is integer value that fits in 29 bits
		if (((id<<3)>>3) == n)
		{
			// make sure its not -0
			if (id == 0 && MathUtils::isNegZero(n))
				return allocDouble(n);
			else
				return (id<<3) | kIntegerType;
		}
		else
		{
			return allocDouble(n);
		}
	}

#ifdef AVMPLUS_VERBOSE
    /**
     * format the value of an atom for debugging.  This is here so that we can
     * more eazily take care of pointer swizzling when called from ScriptObject.format().
     * @param atom
     * @return
     */
    Stringp AvmCore::format(Atom atom)
    {
		if (!isNull(atom))
		{
			switch (atom&7)
			{
			default:
			case kNamespaceType:
				return atomToNamespace(atom)->format(this);
			case kObjectType:
				return atomToScriptObject(atom)->format(this);
			case kStringType:
				{
					Stringp quotes = newString("\"");
					return concatStrings(quotes,
						concatStrings(atomToString((atom&~7)==0 ? kEmptyString->atom() : atom),
													quotes));
				}
			case kSpecialType:
				return kundefined;
			case kBooleanType:
				return booleanStrings[atom>>3];
			case kIntegerType:
				return intToString((int)(atom>>3));
			case kDoubleType:
				AvmAssert(atom != kDoubleType); // this would be a null pointer to double
				return doubleToString(atomToDouble(atom));
			}
		}
		else
		{
			return knull;
		}
    }

	Stringp AvmCore::formatAtomPtr(Atom atom)
	{
		wchar buffer[256];
		int len;
		MathUtils::convertIntegerToString((int)atom, buffer, len, 16);
		return new (GetGC()) String(buffer, len);
	}
#endif

#ifdef DEBUGGER
	int AvmCore::sizeInternedTables()
	{
		int size = 0;

		//add size of strings array
		size += numStrings * sizeof(Stringp);
		for (int i = 0; i < numStrings; i++)
		{
			if (strings[i] > AVMPLUS_STRING_DELETED)
			{
				size += profiler->computeStringSize(strings[i]);
			}
		}

		// add size of namespaces array
		size += numNamespaces * sizeof(Namespace*);
		for (int i=0; i < numNamespaces; i++)
		{
			if (namespaces[i] != NULL)
			{
				if (namespaces[i]->m_prefix != undefinedAtom)
				{
					size += profiler->computeStringSize(atomToString(namespaces[i]->m_prefix));
				}
				size += profiler->computeStringSize(namespaces[i]->getURI());
			}
		}

		return size;
	}

	void AvmCore::sample()
	{
		if(!AvmCore::sampling) return;
		if(!callStack)
		{
			return;
		}

		uint64 time = GC::GetPerformanceCounter();

		size_t sampleSize = sizeof(uint64) + sizeof(uint32) + 
			callStack->depth * (sizeof(void*) + sizeof(void*) + sizeof(int));

		if(currentSample + sampleSize > samples->uncommitted())
		{
			samples->grow();
			if(currentSample + sampleSize > samples->uncommitted())
			{
				// exhausted buffer
				return;
			}
		}

		CallStackNode *csn = callStack;
		byte *p = currentSample;
		*(uint64*)p = time;
		p += sizeof(uint64);
		*(uint32*)p = callStack->depth;
		p += sizeof(uint32);
		while(csn)
		{
			// FIXME: this is essentially a 12 byte copy, faster way?
			*(AbstractFunction**)p = csn->info;
			p += sizeof(AbstractFunction*);
			*(Stringp*)p = csn->filename;
			p += sizeof(Stringp);
			*(int*)p = csn->linenum;
			p += sizeof(int);
			csn = csn->next;
		}
		currentSample = p;
		numSamples++;		
		takeSample = 0;			
	}

	void AvmCore::clearSamples()
	{
		currentSample = samples->start();
		numSamples = 0;
	}

	void AvmCore::startSampling()
	{
		if(!AvmCore::sampling) return;
		if(samplingNow)
			return;
		numSamples = 0;
		samplingNow = true;
		if(!samples->start())
			currentSample = samples->reserve(512 * 1024 * 1024);
		timerHandle = OSDep::startIntWriteTimer(1, &takeSample);
	}

	void AvmCore::stopSampling()
	{
		if(!AvmCore::sampling) return;
		if(!samplingNow)
			return;
		OSDep::stopTimer(timerHandle);
		currentSample = samples->start();
		samplingNow = false;
	}

	void AvmCore::initSampling()
	{
		if(AvmCore::sampling)
		{
			samples = new (GetGC()) GrowableBuffer(GetGC()->GetGCHeap());
			fakeMethodInfos = new (GetGC()) Hashtable(GetGC());
		}
		if(AvmCore::sampling || allocationTracking)
		{
			stackTraces = (StackTrace**)GetGC()->Alloc(sizeof(StackTrace*) * 1024, GC::kZero);
		}

		if(AvmCore::sampling && autoStartSampling)
		{
			startSampling();
		}
	}

#endif

    /**
     * traits with base traits.  These should only be used
	 * for Object instance traits, and activation objects (e.g. global scope)
	 * that are not normally accessable to users.
     */
	Traits* AvmCore::newTraits(Traits *base,
							int nameCount,
							int interfaceDelta,
							size_t objectSize)
	{
		int interfaceCount = interfaceDelta;
		if (base)
			interfaceCount += base->interfaceCount+1; // +1 b/c base is added
		int interfaceCapacity = MathUtils::nextPowerOfTwo((5*interfaceCount >> 2) + 1);
		size_t extra = interfaceCapacity*sizeof(Traits*);
		Traits* traits = new (GetGC(), extra) Traits(this, base,
											  nameCount,
											  interfaceCount,
											  interfaceCapacity,
											  objectSize);
		return traits;
	}
	
	Stringp AvmCore::newString(const char *s) const
	{
		int len = String::Length(s);
		int utf16len = UnicodeUtils::Utf8ToUtf16((const uint8*)s, len, NULL, 0);
		return new (GetGC()) String(s, len, utf16len);
	}

	Stringp AvmCore::newString(const char *s, int len) const
	{
		int utf16len = UnicodeUtils::Utf8ToUtf16((const uint8*)s, len, NULL, 0);
		return new (GetGC()) String(s, len, utf16len);
	}

	Stringp AvmCore::newString(const wchar *s) const
	{
		int len = String::Length(s);
		return new (GetGC()) String(s, len);
	}

	Stringp AvmCore::concatStrings(Stringp s1, Stringp s2) const
	{
		if (!s1) s1 = knull;
		if (!s2) s2 = knull;
		if (s1->length() == 0) {
			return s2;
		} else if (s2->length() == 0) {
			return s1;
		}
		return new (GetGC()) String(s1, s2);
	}

	Stringp AvmCore::intToString(int value)
	{
		wchar buffer[65];
		int len;
		MathUtils::convertIntegerToString(value, buffer, len);
		return new (GetGC()) String(buffer, len);
	}

	Stringp AvmCore::uintToString(uint32 value)
	{
		wchar buffer[65];
		int len;
		if (value <= 0x7FFFFFFF)
			MathUtils::convertIntegerToString(value, buffer, len);
		else
			MathUtils::convertDoubleToString(value, buffer, len);
		return new (GetGC()) String(buffer, len);
	}

	Stringp AvmCore::doubleToString(double d)
	{
		wchar buffer[256];
		int len;
		MathUtils::convertDoubleToString(d, buffer, len, MathUtils::DTOSTR_NORMAL,15);
		return new (GetGC()) String(buffer, len);
	}

	#ifdef DEBUGGER
	StackTrace* AvmCore::newStackTrace()
	{
		int depth = callStack ? callStack->depth : 0;
		int extra = depth > 0 ? sizeof(StackTrace::Element) * (depth-1) : 0;
		StackTrace* stackTrace = new (GetGC(), extra) StackTrace();
		if (stackTrace)
		{
			stackTrace->depth = depth;
			CallStackNode *curr = callStack;
			StackTrace::Element *element = stackTrace->elements;
			while (curr) {
				element->info     = curr->info;
				element->filename = curr->filename;
				element->linenum  = curr->linenum;
				element++;
				curr = curr->next;
			}
		}
		return stackTrace;
	}

	void AvmCore::rehashTraces(int newSize)
	{
		uint32 oldNumTraces = numTraces;
		numTraces = 0;
		StackTrace **oldStackTraces = stackTraces;
		stackTraces = (StackTrace**)GetGC()->Calloc(newSize, sizeof(StackTrace*), GC::kZero);

		uint32 bitMask = newSize-1;

		for(uint32 i=0, n=oldNumTraces; i<n; i++)
		{
			StackTrace *t = oldStackTraces[i];
			if(t) 
			{
				uint32 j = (StackTrace::hashCode(t->elements, t->depth)&0x7FFFFFFF) & bitMask;
				uint32 n = 7;
				while (stackTraces[j] != NULL) {
					j = (j + (n++)) & bitMask; // quadratic probe
				}
				stackTraces[j] = t;
			}
		}
		numTraces = oldNumTraces;
	}

	int AvmCore::findTrace(void /*StackTrace::Element*/ *ve, int depth)
	{
		StackTrace::Element *e = (StackTrace::Element*)ve;
		uint32 sizeTraces = GetGC()->Size(stackTraces)/sizeof(StackTrace*);
		uint32 shift = FindOneBit(sizeTraces);
		uint32 bitMask = (1<<shift)-1;
		if(numTraces*4 > bitMask*3)
		{
			rehashTraces((bitMask+1)*2);
			sizeTraces = GetGC()->Size(stackTraces)/sizeof(StackTrace*);
			shift = FindOneBit(sizeTraces);
			bitMask = (1<<shift)-1;
		}

		uintptr hashCode = StackTrace::hashCode(e, depth);
		uint32 j = (hashCode&0x7FFFFFFF) & bitMask;
		uint32 n = 7;
		while (stackTraces[j] != NULL && !stackTraces[j]->equals(e,depth)) {
			j = (j + (n++)) & bitMask; // quadratic probe
		}
		return j;
	}


	StackTrace* AvmCore::getStackTrace(void /*StackTrace::Element*/ *ve, int depth)
	{
		StackTrace::Element *e = (StackTrace::Element*)ve;
		int i = findTrace(e, depth);
		StackTrace *t = stackTraces[i];
		if(t)
			return t;

		int extra = depth > 0 ? sizeof(StackTrace::Element) * (depth-1) : 0;
		t = new (GetGC(), extra) StackTrace();
		t->depth = depth;
		memcpy(t->elements, e, extra + sizeof(StackTrace::Element));
		stackTraces[i] = t;
		numTraces++;
		return t;
	}

	StackTrace* AvmCore::getStackTrace()
	{
		if(callStack)
		{
			StackTrace::Element *elements = (StackTrace::Element*)alloca(callStack->depth * sizeof(StackTrace::Element));
			StackTrace::Element *e = elements;
			CallStackNode *curr = callStack;
			while (curr) {
				e->info     = curr->info;
				e->filename = curr->filename;
				e->linenum  = curr->linenum;
				e++;
				curr = curr->next;
			}

			int i = findTrace(elements, callStack->depth);
			StackTrace *t = stackTraces[i];
			if(t)
				return t;
			StackTrace *st = newStackTrace();
			stackTraces[i] = st;
			numTraces++;
			return st;
		}
		return NULL;
	}

	#ifdef _DEBUG
	void AvmCore::dumpStackTrace()
	{
		StringBuffer buffer(this);		
		buffer << "Stack Trace:\n" << newStackTrace()->format(this) << '\n';
		AvmDebugMsg(false, buffer.c_str());
	}
	#endif

	/*static*/
	void AvmCore::chargeAllocation(Atom a)
	{
		GC *gc = GC::GetGC((const void*)a);
		AvmCore *core = (AvmCore*)gc->GetGCContextVariable(GC::GCV_AVMCORE);
		if(!core->callStack || !core->callStack->env)
			return;

		Toplevel *toplevel = core->callStack->env->toplevel();
		ClassClosure *clazz = NULL;
		switch(a&7)
		{
		case kStringType:
			clazz = toplevel->stringClass;
			break;
		case kNamespaceType:
			clazz = toplevel->namespaceClass;
			break;
		}
		if(clazz)
		{
			clazz->addInstance(a);
		}
	}
    #endif /* DEBUGGER */

	int AvmCore::integer(Atom atom) const
	{
		if ((atom & 7) == kIntegerType || (atom&7) == kBooleanType) {
			return atom >> 3;
		} else {
			// TODO optimize the code below.
			double d = number(atom);
			return integer_d(d);
		}
	}

	// static

	int AvmCore::integer_d(double d)
	{
		// Try a simple case first to see if we have a in-range float value

#ifdef WIN32 // should be any intel build
		// WIN32's real2int returns 0x80000000 if d is not in a valid integer range
		int id = MathUtils::real2int (d);
		if (id != 0x80000000) 
			return id;
#endif

		return doubleToInt32(d);
	}

#if defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
	int AvmCore::integer_d_sse2(double d)
	{
		int id;
		#ifdef WIN32 
		_asm {
			cvttsd2si eax,d
			mov id,eax
		}
		if (id != 0x80000000)
			return id;
		#elif defined(_MAC) && (defined(AVMPLUS_IA32) || defined(AVMPLUS_AMD64))		
		id = _mm_cvttsd_si32(_mm_set_sd(d));
		if (id != (int)0x80000000)
			return id;
        #elif AVMPLUS_LINUX
        asm("movups %1, %%xmm0;"
            "cvttsd2si %%xmm0, %%eax;"
            "movl %%eax, %0" : "=r" (id) : "m" (d) : "%eax");
        if (id != 0x80000000)
            return id;
		#endif

		return doubleToInt32(d);
	}
#endif // AVMPLUS_IA32 or AVMPLUS_AMD64

	int AvmCore::doubleToInt32(double d)
	{
		// From the ES3 spec, 9.5
		//  2.	If Result(1) is NaN, +0, -0, +Inf, or -Inf, return +0.
		//  3.	Compute sign(Result(1)) * floor(abs(Result(1))).
		//  4.	Compute Result(3) modulo 2^32; that is, a finite integer value k of Number 
		//  type with positive sign and less than 2^32 in magnitude such the mathematical 
		//  difference of Result(3) and k is mathematically an integer multiple of 2^32.
		//	5.	If Result(4) is greater than or equal to 2^31, return Result(4)- 2^32, 
		//  otherwise return Result(4).

		// step 2
		if (MathUtils::isNaN(d) || MathUtils::isInfinite(d) || d == 0) {
			return 0;
		}

		// step 3 (round towards 0)
		double ad = d < 0.0 ? MathUtils::floor(-d) : MathUtils::floor(d);

		// step 4
		if (ad > 4294967295.0)
			ad = MathUtils::mod(ad,4294967296.0); // ad %= 0x10000000

		// step 5
		if (ad >= (double)2147483648.0)
		{
			// This case is a large negative number that overflows back to a positive
			// number.  This code has been tweaked to work on both Mac and Windows.  Mac
			// is particularly sensitive to edge numbers such as 0x80000000 when converting
			// doubles to ints).
			if (d < 0.0)
			{
				int intVal = MathUtils::real2int (ad - 2147483648.0);
				return 0x80000000 - intVal;
			}
			// This case is a large positive number overflowing to negative.
			else
			{
				int intVal = MathUtils::real2int (ad - 2147483648.0);
				return 0x80000000 + intVal;
			}
		}
		else
		{
			return MathUtils::real2int(d < 0.0 ? -ad : ad);
		}
	}

	// This routine is a very specific parser to generate a positive integer from a string.
	// The following are supported:
	// "0" - one single digit for zero - NOT "00" or any other form of zero
	// [1-9]+[0-9]* up to 2^32-2 (4294967294)
	// 2^32-1 (4294967295) is not supported (see ECMA quote below).
	// The ECMA that we're supporting with this routine is...
	// cn:  the ES3 test for a valid array index is 
	//  "A property name P (in the form of a string value) is an array index if and 
	//  only if ToString(ToUint32(P)) is equal to P and ToUint32(P) is not equal to 2^32-1."
	bool AvmCore::getIndexFromString (Stringp s, uint32 *result)
	{
		int len = s->length();
		if (!len)
			return false;

		// Don't support 000000 as 0.
		// We don't support 0x1234 as 1234 in hex since string(1234) doesn't equal '0x1234')
		// No leading zeros are supported

		// Single 0 is ok
		const wchar *c = s->c_str();
		if (*c == '0')
		{
			if (len == 1)
			{
				*result = 0;
				return true;
			}
			else
			{
				return false;
			}
		}
		else if ((*c < '0') || (*c > '9'))
		{
			return false;
		}

		// A string that is more than 10 digits (and does not start with 0) 
		// will always be greater than 2^32-1 (4294967295) or not a numeric string
		if (len > 10)
		{
			return false;
		}

		uint32 res = 0;
		int i = 0;
		while (i < len)
		{
			wchar ch = c[i];
			if (ch < '0' || ch > '9')
				return false;

			res = res * 10 + (ch - '0');
			i++;
		}

		*result = res;
		// Our input string length is less then 10 digits so we now
		// our output is in the valid range up to 999,999,999.
		if (len < 10)
		{
			return true;
		}

		// At this point we know our string is 10 characters long and is all numeric 
		// characters.  We need to check it to see if it overflows 2^32 and whether it equals
		// 2^32-1.  This string comparison works as a numeric comparison since we know our input
		// string is also ten numeric digits.

		int comp = String::Compare (c, "4294967295", 10);
		return (comp > 0);
	}

	CodeContext* AvmCore::codeContext() const
	{
		if (codeContextAtom == CONTEXT_NONE) {
			return NULL;
		} else if ((codeContextAtom&7) == CONTEXT_ENV) {
			MethodEnv *env = (MethodEnv*)(codeContextAtom&~7);
			return env->vtable->abcEnv->codeContext;
		} else {
			return (CodeContext*)(codeContextAtom&~7);
		}
	}

	/**
	 * MIR needs a large intermediate buffer for codegen.
	 * These routines allow reuse of this buffer(s)
	 */				  
	// @todo make this static and work with gc
	//List<GrowableBuffer*> AvmCore::mirBuffers(0);  // mir buffer pool
	//GCSpinLock AvmCore::mirBufferLock; // lock for pool

	GrowableBuffer* AvmCore::requestMirBuffer()
	{
		//GCAcquireSpinlock spinlock(mirBufferLock);
		GrowableBuffer* buffer = 0;
		if (mirBuffers.size() > 0)
			buffer = mirBuffers.removeFirst();
		else
			buffer = requestNewMirBuffer();
		return buffer;
	}

	GrowableBuffer* AvmCore::requestNewMirBuffer()
	{
		return new (GetGC()) GrowableBuffer(GetGC()->GetGCHeap());
	}

	void AvmCore::releaseMirBuffer(GrowableBuffer* buffer)
	{
		//GCAcquireSpinlock spinlock(mirBufferLock);
		mirBuffers.add(buffer);
	}

#ifdef MMGC_DRC
	/*static*/ 
	void AvmCore::decrementAtomRegion(Atom *arr, int length)
	{
		for(int i=0; i < length; i++)
		{
			Atom a = arr[i];
			RCObject *obj = (RCObject*)(a&~7);
			if(obj)	{
				switch(a&7) {
				case kStringType:
				case kObjectType:
				case kNamespaceType:
					((RCObject*)(a&~7))->DecrementRef();
					break;
				}
			}
			arr[i] = 0;
		}
	}
#endif

	/*static*/ 
	void AvmCore::atomWriteBarrier(MMgc::GC *gc, const void *container, Atom *address, Atom atomNew)
	{ 
#ifdef MMGC_DRC
		Atom atom = *address;
		if(!isNull(atom)) {
			switch(atom&7)
			{	
				case kStringType:
				case kObjectType:
				case kNamespaceType:
					MMgc::RCObject *obj = (MMgc::RCObject*)(atom&~7);
					obj->DecrementRef();
					break;
			}
		}
#endif

		switch(atomNew&7)
		{
		case kStringType:
		case kObjectType:
		case kNamespaceType:
#ifdef MMGC_DRC
			if(!isNull(atomNew))
				((MMgc::RCObject*)(atomNew&~7))->IncrementRef();
			// fall through
#endif
		case kDoubleType:
			{
				gc->WriteBarrierNoSubstitute(container, (const void*)atomNew);
			}
			break;	

		}
		*address = atomNew;
	}

	Atom AvmCore::gcObjectToAtom(const void* obj)
	{
#ifdef _DEBUG
		// We get a null obj here through ElementE4XNode::_insert
		if (obj)
		{
			GC* gc = GC::GetGC(obj);
			AvmAssert(!gc->IsRCObject(obj));
		}
#endif		
		// return a non-RC atom, makes atomWriteBarrier do the right thing
		return (Atom)obj|kDoubleType;
	}

#ifdef AVMPLUS_MIR

	void AvmCore::initMultinameLate(Multiname& name, Atom index)
	{
		if (isObject(index))
		{
			ScriptObject* i = atomToScriptObject(index);
			if (i->traits() == traits.qName_itraits)
			{
				QNameObject* qname = (QNameObject*) i;
				bool attr = name.isAttr();
				qname->getMultiname(name);
				name.setAttr(attr);
				return;
			}
		}

		name.setName(intern(index));
	}		
#endif
}
