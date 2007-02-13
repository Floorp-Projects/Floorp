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

#ifndef __avmplus_AvmCore__
#define __avmplus_AvmCore__

namespace avmplus
{

#define DECLARE_NATIVE_SCRIPTS() static NativeScriptInfo scriptEntries[];

#define BEGIN_NATIVE_SCRIPTS(_Class) NativeScriptInfo _Class::scriptEntries[] = {

#define NATIVE_SCRIPT(script_id, _Script) { script_id, (NativeScriptInfo::Handler)_Script::createGlobalObject, _Script::natives, sizeof(_Script) },

#define END_NATIVE_SCRIPTS() { -1, NULL, NULL, 0 } };

#define DECLARE_NATIVE_CLASSES() static NativeClassInfo classEntries[];

#define BEGIN_NATIVE_CLASSES(_Class) NativeClassInfo _Class::classEntries[] = {

#define NATIVE_CLASS(class_id, _Class, _Instance) { avmplus::NativeID::class_id, (NativeClassInfo::Handler)_Class::createClassClosure, _Class::natives, sizeof(_Class), sizeof(_Instance) },

#define END_NATIVE_CLASSES() { -1, NULL, NULL, 0, 0 } };

#define OBJECT_TYPE		(core->traits.object_itraits)
#define CLASS_TYPE		(core->traits.class_itraits)
#define FUNCTION_TYPE	(core->traits.function_itraits)
#define ARRAY_TYPE		(core->traits.array_itraits)
#define STRING_TYPE		(core->traits.string_itraits)
#define NUMBER_TYPE		(core->traits.number_itraits)
#define INT_TYPE		(core->traits.int_itraits)
#define UINT_TYPE		(core->traits.uint_itraits)
#define BOOLEAN_TYPE	(core->traits.boolean_itraits)
#define VOID_TYPE		(core->traits.void_itraits)
#define NULL_TYPE		(core->traits.null_itraits)
#define NAMESPACE_TYPE	(core->traits.namespace_itraits)

const int kBufferPadding = 16;

	/**
	 * The main class of the AVM+ virtual machine.  This is the
	 * main entry point to the VM for running ActionScript code.
	 */
	class AvmCore : public MMgc::GCRoot
	{
	public:
		/**
		 * The console object.  Text to be displayed to the developer
		 * or end-user can be directed to console, much like the cout
		 * object in C++ iostreams.
		 *
		 * The console object is a wrapper around the console output
		 * stream specified by the setConsoleStream method.
		 * Programs embedding AVM+ will typically implement
		 * the OutputStream interface and pass it in via
		 * setConsoleStream.
		 */
		PrintWriter console;

		#ifdef DEBUGGER
		/**
		 * For debugger versions of the VM, this is a pointer to
		 * the Debugger object.
		 */
		Debugger *debugger;
		Profiler *profiler;
		bool allocationTracking;
		bool sampling;
		// call startSampling in AvmCore ctor
		bool autoStartSampling;

		bool samplingNow;
		int takeSample;
		uint32 numSamples;
		GrowableBuffer *samples;
		byte *currentSample;
		void sample();
		void sampleCheck() { if(takeSample) sample(); }
		void startSampling();
		void stopSampling();
		void clearSamples();

		uintptr timerHandle;
		Hashtable *fakeMethodInfos;
		void initSampling();
		#endif

		void branchCheck(MethodEnv *env, bool interruptable, int go)
		{
			if(go < 0)
			{
#ifdef DEBUGGER
				sampleCheck();
#endif
				if (interruptable && interrupted)
						interrupt(env);
			}
		}

		#ifdef AVMPLUS_MIR
		// MIR intermediate buffer pool
		List<GrowableBuffer*, LIST_GCObjects> mirBuffers; // mir buffer pool
		GrowableBuffer* requestNewMirBuffer();	 // create a new buffer
		GrowableBuffer* requestMirBuffer();	     // get next buffer in list or a create a new one
		void releaseMirBuffer(GrowableBuffer* buffer);
		//GCSpinLock mirBufferLock;

		#ifdef AVMPLUS_VERBOSE
		MMgc::GCHashtable* codegenMethodNames;
		#endif /* AVMPLUS_VERBOSE */

		void initMultinameLate(Multiname& name, Atom index);

		#endif /* MIR */

#ifdef AVMPLUS_PROFILE
		StaticProfiler sprof;
		DynamicProfiler dprof;
#endif

		/**
		 * Redirects the standard output of the VM to the specified
		 * output stream.  Output from print() statements and
		 * error messages will be sent here.
		 * @param stream output stream to use for console output
		 */
		void setConsoleStream(OutputStream *stream);

		/**
		 * GCCallback functions 
		 */
		virtual void presweep();
		virtual void postsweep() {}
		
		#ifdef AVMPLUS_VERBOSE
		/**
		 * The verbose flag may be set to display each bytecode
		 * instruction as it is executed, along with a snapshot of
		 * the state of the stack and scope chain.
		 * Caution!  This shoots out a ton of output!
		 */
		bool verbose;
		#endif /* AVMPLUS_VERBOSE */

		#ifdef AVMPLUS_INTERP
		/**
		 * The turbo switch determines how bytecode is executed.
		 * When turbo is true, bytecode is translated to native code.
		 * When turbo is false, the gallop() interpreter loop is used.
		 * turbo defaults to true except on platforms where it is
		 * not supported, and except when debugging is in progress.
		 * 
		 * Turbo is a debugger-only option.  release builds always
		 * have it turned on.  This means we can only build release
		 * builds on supported platforms.
		 */
		bool turbo;
		#endif /* AVMPLUS_INTERP */

		#ifdef AVMPLUS_MIR

		#ifdef AVMPLUS_INTERP
		/**
		 * To speed up initialization, we don't use MIR on
		 * $init methods; we use interp instead.  For testing
		 * purposes, one may want to force the MIR to be used
		 * for all code including $init methods.  The
		 * forcemir switch forces all code to run through MIR
		 * instead of interp.
		 */
		bool forcemir;
		#endif

		bool cseopt;
		bool dceopt;

		#if defined (AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
		bool sse2;
		#endif

		/**
		 * If this switch is set, executing code will check the
		 * "interrupted" flag to see whether an interrupt needs
		 * to be handled.
		 */
		bool interrupts;

		/**
		 * If this is set to a nonzero value, executing code
		 * will check the stack pointer to make sure it
		 * doesn't go below this value.
		 */
		uint32 minstack;

		/**
		 * This method will be invoked when the first exception
		 * frame is set up.  This will be a good point to set
		 * minstack.
		 */
		virtual void setStackBase() {}
		
		/**
		 * Genearate a graph for the basic blocks.  Can be used by
		 * 'dot' utility to generate a jpg.
		 */
		#ifdef AVMPLUS_VERBOSE
		bool bbgraph;
		#endif //AVMPLUS_VERBOSE

		#endif // AVMPLUS_MIR

#ifdef AVMPLUS_VERIFYALL
		bool verifyall;
#endif

		/** Internal table of strings for boolean type ("true", "false") */
		DRC(Stringp) booleanStrings[2];

		/** Container object for traits of built-in classes */
		BuiltinTraits traits;

		/** PoolObject for built-in classes */
		PoolObject* builtinPool;

		/** Domain for built-in classes */
		Domain* builtinDomain;
		
		/** The location of the currently active defaultNamespace */
		Namespace *const*dxnsAddr;

		/**
		 * If this flag is set, an interrupt is in progress.
		 * This must be type int, not bool, since it will
		 * be checked by generated code.
		 */
		int interrupted;
		
		/**
		 * The default namespace, "public", that all identifiers
		 * belong to
		 */
		DRC(Namespace*) publicNamespace;
		VTable* namespaceVTable;

		#ifdef FEATURE_JNI
		Java* java;     /* java vm control */
		#endif

		/**
		 * Execute an ABC file that has been parsed into a
		 * PoolObject.
		 * @param pool PoolObject containing the ABC file to
		 *             execute
		 * @param domainEnv The DomainEnv object to execute
		 *                  against, or NULL if a new DomainEnv
		 *                  should be created
		 * @param toplevel the Toplevel object to execute against,
		 *                 or NULL if a Toplevel should be
		 *                 created.
		 * @throws Exception If an error occurs, an Exception object will
		 *         be thrown using the AVM+ exceptions mechanism.
		 *         Calls to handleActionBlock should be bracketed
		 *         in TRY/CATCH.
		 */
		Atom handleActionPool(PoolObject* pool,
								   DomainEnv* domainEnv,
								   Toplevel* &toplevel,
								   CodeContext *codeContext);

		ScriptEnv* prepareActionPool(PoolObject* pool,
									 DomainEnv* domainEnv,
									 Toplevel*& toplevel,
									 CodeContext *codeContext);
		
		void exportDefs(Traits* traits, ScriptEnv* scriptEnv);

		/**
		 * Parse the ABC block starting at offset start in code.
		 * @param code buffer holding the ABC block to parse
		 * @param start zero-indexed offset, in bytes, into the
		 *              buffer where the code begins
		 * @param toplevel the Toplevel object to execute against,
		 *                 or NULL if a Toplevel should be
		 *                 created.
		 * @param nativeMethods the NATIVE_METHOD table
		 * @param nativeClasses the NATIVE_CLASS table
		 * @param nativeScriptss the NATIVE_SCRIPT table
		 * @throws Exception If an error occurs, an Exception object will
		 *         be thrown using the AVM+ exceptions mechanism.
		 *         Calls to handleActionBlock should be bracketed
		 *         in TRY/CATCH.
		 */
		PoolObject* parseActionBlock(ScriptBuffer code,
									 int start,
									 Toplevel* toplevel,
									 Domain* domain,
									 AbstractFunction *nativeMethods[],
									 NativeClassInfo *nativeClasses[],
									 NativeScriptInfo *nativeScripts[]);
		
		/**
		 * Execute the ABC block starting at offset start in code.
		 * @param code buffer holding the ABC block to execute
		 * @param start zero-indexed offset, in bytes, into the
		 *              buffer where the code begins
		 * @param toplevel the Toplevel object to execute against,
		 *                 or NULL if a Toplevel should be
		 *                 created.
		 * @param nativeMethods the NATIVE_METHOD table
		 * @param nativeClasses the NATIVE_CLASS table
		 * @param nativeScripts the NATIVE_SCRIPT table
		 * @throws Exception If an error occurs, an Exception object will
		 *         be thrown using the AVM+ exceptions mechanism.
		 *         Calls to handleActionBlock should be bracketed
		 *         in TRY/CATCH.
		 */
		Atom handleActionBlock(ScriptBuffer code,
									int start,
									DomainEnv* domainEnv,
									Toplevel* &toplevel,
									AbstractFunction *nativeMethods[],
									NativeClassInfo *nativeClasses[],
									NativeScriptInfo *nativeScripts[],
									CodeContext *codeContext);

				
		/**
		 * Initializes the native table with the specified
		 * class/script entries.
		 */
		void initNativeTables(NativeClassInfo *classEntries,
							 NativeScriptInfo *scriptEntries,
							 AbstractFunction *nativeMethods[],
							 NativeClassInfo* nativeClasses[],
							 NativeScriptInfo* nativeScripts[]);

		/** Implementation of OP_equals */
		Atom eq(Atom lhs, Atom rhs);
		
		/**
		 * this is the abstract relational comparison algorithm according to ECMA 262 11.8.5
		 * @param lhs
		 * @param rhs
		 * @return trueAtom, falseAtom, or undefinedAtom
		 */
		Atom compare(Atom lhs, Atom rhs);

		/** Implementation of OP_strictequals */
		Atom stricteq(Atom lhs, Atom rhs);

		/**
		 * Helper method; returns true if the atom is a tagged ScriptObject
		 * pointer.  The actual type of the object is indicated by
		 * ScriptObject->vtable->traits.
		 */
		static bool isObject(Atom atom)
		{
			return (atom&7) == kObjectType && !isNull(atom);
		}

		static bool isPointer(Atom atom)
		{
			return (atom&7) < kSpecialType || (atom&7) == kDoubleType;
		}

		static bool isTraits(Atom type)
		{
			return type != 0 && (type&7) == 0;
		}

		static bool isNamespace(Atom atom)
		{
			return (atom&7) == kNamespaceType && !isNull(atom);
		}

		static bool isMethodBinding(Binding b)
		{
			return (b&7) == BIND_METHOD;
		}

		static bool isAccessorBinding(Binding b)
		{
			return (b&7) >= BIND_GET;
		}

		static bool hasSetterBinding(Binding b)
		{
			return (b&6) == BIND_SET;
		}

		static bool hasGetterBinding(Binding b)
		{
			return (b&5) == BIND_GET;
		}

		static int bindingToGetterId(Binding b)
		{
			AvmAssert(hasGetterBinding(b));
			return urshift(b,3);
		}

		static int bindingToSetterId(Binding b)
		{
			AvmAssert(hasSetterBinding(b));
			return 1+urshift(b,3);
		}

		static int bindingToMethodId(Binding b)
		{
			AvmAssert(isMethodBinding(b));
			return urshift(b,3);
		}

		static int bindingToSlotId(Binding b)
		{
			AvmAssert(isSlotBinding(b));
			return urshift(b,3);
		}

		/** true if b is a var or a const */
		static int isSlotBinding(Binding b)
		{
			AvmAssert((BIND_CONST&6)==BIND_VAR);
			return (b&6)==BIND_VAR;
		}

		/** true only if b is a var */
		static int isVarBinding(Binding b)
		{
			return (b&7)==BIND_VAR;
		}
		/** true only if b is a const */
		static int isConstBinding(Binding b)
		{
			return (b&7)==BIND_CONST;
		}
		
		/** Helper method; returns true if atom is an Function */
		bool isFunction(Atom atom)
		{
			return istype(atom, traits.function_itraits);
		}

		/** Helper method; returns true if atom's type is double */
		static bool isDouble(Atom atom)
		{
			return (atom&7) == kDoubleType;
		}

		/** Helper method; returns true if atom's type is int */
		static bool isInteger(Atom atom)
		{
			return (atom&7) == kIntegerType;
		}

		/** Helper method; returns true if atom's type is Number */
		static bool isNumber(Atom atom)
		{
			// accept kIntegerType(6) or kDoubleType(7)
			return (atom&6) == kIntegerType;
		}

		/** Helper method; returns true if atom's type is boolean */
		static bool isBoolean(Atom atom)
		{
			return (atom&7) == kBooleanType;
		}

		/** Helper method; returns true if atom's type is null */
		static bool isNull(Atom atom)
		{
			return ISNULL(atom);
		}

		/** Helper method; returns true if atom's type is undefined */
		static bool isUndefined(Atom atom)
		{
			return (atom == undefinedAtom);
		}

		static bool isNullOrUndefined(Atom atom)
		{
			return ((uintptr)atom) <= (uintptr)kSpecialType;
		}

#ifdef AVMPLUS_VERBOSE
		/** Disassembles an opcode and places the text in str. */
		void formatOpcode(PrintWriter& out, const byte *pc, AbcOpcode opcode, int off, PoolObject* pool);
		static void formatMultiname(PrintWriter& out, uint32 index, PoolObject* pool);
#endif

		/**
		 * The resources table tracks what ABC's have been
		 * decoded, and avoids decoding the same one multiple
		 * times.
		 */
		Hashtable *resources;

		/**
		 * @name interned constants
		 * Constants used frequently in the VM; these are typically
		 * identifiers that are part of the core language semantics
		 * like "prototype" and "constructor".  These are interned
		 * up front and held in AvmCore for easy reference.
		 */
		/*@{*/

		DRC(Stringp) kconstructor;
		DRC(Stringp) kEmptyString;
		DRC(Stringp) ktrue;
		DRC(Stringp) kfalse;
		DRC(Stringp) kundefined;
		DRC(Stringp) knull;
		DRC(Stringp) ktoString;
		DRC(Stringp) ktoLocaleString;
		DRC(Stringp) kvalueOf;
		DRC(Stringp) klength;
		DRC(Stringp) kobject;
		DRC(Stringp) kfunction;
		DRC(Stringp) kxml;
		DRC(Stringp) kboolean;
		DRC(Stringp) knumber;
		DRC(Stringp) kstring;
		DRC(Stringp) kuri;
		DRC(Stringp) kprefix;
		DRC(Stringp) kglobal;
		DRC(Stringp) kcallee;
		DRC(Stringp) kNeedsDxns;
		DRC(Stringp) kAsterisk;
		Atom kNaN;

		DRC(Stringp) cachedChars[128];
		/*@}*/

		/** Constructor */
		AvmCore(MMgc::GC *gc);

		/** Destructor */
		~AvmCore();

		/**
		 * Parses builtin.abc into a PoolObject, to be executed
		 * later for each new Toplevel
		 */
		void initBuiltinPool();
		
		/**
		 * Initializes the specified Toplevel object by running
		 * builtin.abc
		 */
		Toplevel* initTopLevel();		

		virtual size_t getToplevelSize() const;
		virtual Toplevel* createToplevel(VTable *vtable);
		
	public:
		/**
		 * toUInt32 is the ToUInt32 algorithm from
		 * ECMA-262 section 9.6, used in many of the
		 * native core objects
		 */
		uint32 toUInt32(Atom atom) const
		{
			return (uint32)integer(atom);
		}

		/**
		 * toInteger is the ToInteger algorithm from
		 * ECMA-262 section 9.4, used in many of the
		 * native core objects
		 */
		double toInteger(Atom atom) const
		{
			if ((atom & 7) == kIntegerType) {
				return (double) (atom>>3);
			} else {
				return MathUtils::toInt(number(atom));
			}
		}

		/**
		 * Converts the passed atom to a 32-bit signed integer.
		 * If the atom is already an integer, it is simply
		 * decoded.  Otherwise, it is coerced to the int type
		 * and returned.  This is ToInt32() from E3 section 9.5
		 */
		int integer(Atom atom) const;

		// convert atom to integer when we know it is already a legal signed-32 bit int value
		static int integer_i(Atom a)
		{
			if ((a&7) == kIntegerType)
				return int(a>>3);
			else
				// TODO since we know value is legal int, use faster d->i
				return MathUtils::real2int(atomToDouble(a));
		}

		// convert atom to integer when we know it is already a legal unsigned-32 bit int value
		static uint32 integer_u(Atom a)
		{
			if ((a&7) == kIntegerType)
			{
				return uint32(a>>3);
			}
			else
			{
				// TODO figure out real2int for unsigned
				return (uint32) atomToDouble(a);
			}
		}

		static int integer_d(double d);

#if defined (AVMPLUS_IA32) || defined(AVMPLUS_AMD64)
		static int integer_d_sse2(double d);
		Atom doubleToAtom_sse2(double n);
#endif

	private:
		static int doubleToInt32(double d);

	public:
		static double number_d(Atom a)
		{
			AvmAssert(isNumber(a));

			if ((a&7) == kIntegerType)
				return a>>3;
			else
				return atomToDouble(a);
		}

		/**
		 * intAtom is similar to the integer method, but returns
		 * an atom instead of a C++ int.
		 */
		Atom intAtom(Atom atom)
		{
			return intToAtom(integer(atom));
		}

		Atom uintAtom(Atom atom)
		{
			return uintToAtom(toUInt32(atom));
		}

		/**
		 * Converts the passed atom to a C++ bool.
		 * If the atom is already an E4 boolean, it is simply
		 * decoded.  Otherwise, it is coerced to the boolean type
		 * and returned.
		 * [ed] 12/28/04 use int because bool is sometimes byte-wide.
		 */
		int boolean(Atom atom);

		/**
		 * Returns the passed atom's string representation.
		 * If the passed atom is not a string, it is coerced
		 * to type string using the ECMAScript coercion rules.
		 */
		Stringp string(Atom atom);

		Stringp coerce_s(Atom atom);

		/**
		 * Returns true if the passed atom is of string type.
		 */
		static bool isString(Atom atom)
		{
			return (atom&0x7) == kStringType && !isNull(atom);
		}

		static bool isName(Atom atom)
		{
			return isString(atom) && atomToString(atom)->isInterned();
		}

		/**
		 * an interned atom is canonicalized in this way:
		 * boolean -> "true" or "false"
		 * number -> intern'ed string value
		 * string -> intern'ed string value
		 * object -> intern'ed result of toString()
		 *
		 * this way, interned atoms are suitable to be used as map keys and can
		 * be compared using ==.
		 * @param atom
		 * @return
		 */
		Stringp intern(Atom atom);

		Namespace* internNamespace(Namespace* ns);

		/** Helper function; reads a signed 24-bit integer from pc */
		static int readS24(const byte *pc)
		{
			#ifdef WIN32
				// unaligned short access still faster than 2 byte accesses
				return ((unsigned short*)pc)[0] | ((signed char*)pc)[2]<<16;
			#else
				return pc[0] | pc[1]<<8 | ((signed char*)pc)[2]<<16;
			#endif
		}

        /**
         * Returns the size of the instruction + all it's operands.  For OP_lookupswitch the size will not include
         * the size for the case targets.
         */
		static int calculateInstructionWidth(const byte* p)
		{
            int a, b;
            unsigned int c, d;
			const byte* p2 = p;
            readOperands(p2, c, a, d, b);
			return int(p2-p);
		}

        /**
         * Read in some operands for the instruction located at *pc.  
         * Returns the size of the instruction, but will not read in all the case targets for 
         * an OP_lookupswitch, since there will be a variable number of them. 
         */
        static void readOperands(const byte* &pc, unsigned int& imm32, int& imm24, unsigned int& imm32b, int& imm8 )
        {
            AbcOpcode opcode = (AbcOpcode)*pc++;
            int op_count = opOperandCount[opcode];

            imm8 = pc[0];
			if( opcode == OP_pushbyte || opcode == OP_debug )
			{
				// these two operands have a byte as their first operand, which is not encoded
				// with the variable length encoding scheme for bigger numbers, because it will
				// never be larger than a byte.
				--op_count;
				pc++;
			}

			if( op_count > 0 )
			{
                if( opcode >= OP_ifnlt && opcode <= OP_lookupswitch )
                {
                    imm24 = AvmCore::readS24(pc);
                    pc += 3;
                }
                else
                {
    				imm32 = AvmCore::readU30(pc);
                }

				if( opcode == OP_debug )
                {
                    --op_count; //OP_debug has a third operand of a byte
                    pc++;
                }
                if( op_count > 1 )
				{
					imm32b = AvmCore::readU30(pc);
				}
			}
        }
		/** 
         * Helper function; reads an unsigned 32-bit integer from pc 
         * See AbcParser::readS32 for more explanation of the variable length
         * encoding scheme.  
         * 
         * 2nd argument is set to the actual size, in bytes, of the number read in,
         * and third argument is the version of the ABC 
         */
		static uint32 readU30(const byte *&p)
		{
			unsigned int result = p[0];
			if (!(result & 0x00000080))
			{
				p++;
				return result;
			}
			result = (result & 0x0000007f) | p[1]<<7;
			if (!(result & 0x00004000))
			{
				p += 2;
				return result;
			}
			result = (result & 0x00003fff) | p[2]<<14;
			if (!(result & 0x00200000))
			{
				p += 3;
				return result;
			}
			result = (result & 0x001fffff) | p[3]<<21;
			if (!(result & 0x10000000))
			{
				p += 4;
				return result;
			}
			result = (result & 0x0fffffff) | p[4]<<28;
			p += 5;
			return result;
		}

		/** Helper function; reads an unsigned 16-bit integer from pc */
		static int readU16(const byte *pc)
		{
			#ifdef WIN32
				// unaligned short access still faster than 2 byte accesses
				return *((unsigned short*)pc);
			#else
				return pc[0] | pc[1]<<8;
			#endif
		}

		/**
		 * this is the implementation of the actionscript "is" operator.  similar to java's
		 * instanceof.  returns true/false according to AS rules.  in particular, it will return
		 * false if value==null.
		 */
		bool istype(Atom atom, Traits* itraits);

		/**
		 * this is the implementation of the actionscript "is" operator.  similar to java's
		 * instanceof.  returns true/false according to AS rules.  in particular, it will return
		 * false if value==null.
		 */
		Atom istypeAtom(Atom atom, Traits* itraits) { 
			return istype(atom, itraits) ? trueAtom : falseAtom; 
		}

		/**
		 * implementation of OP_increg, decreg, increment, decrement which correspond to
		 * ++ and -- operators in AS.
		 */
		void increment_d(Atom *atom, int delta);

		/**
		 * implementation of OP_increg, decreg, increment, decrement which correspond to
		 * ++ and -- operators in AS.
		 */
		void increment_i(Atom *atom, int delta);
		
		/**
		 * ES3's internal ToPrimitive() function
		 */
		Atom primitive(Atom atom);

		/** OP_toboolean; ES3 ToBoolean() */
		Atom booleanAtom(Atom atom);

		/** OP_tonumber; ES3 ToNumber */
		Atom numberAtom(Atom atom);
		
		/**
		 * ES3's internal ToNumber() function for internal use
		 */
		double number(Atom atom) const;

#ifdef AVMPLUS_PROFILE
		/**
		 * dump profiler stats 
		 */
		void dump();
#endif
		
		/**
		 * produce an atom from a string.  used only for string constants.
		 * @param s
		 * @return
		 */
		Atom constant(const char *s)
		{
			return constantString(s)->atom();
		}

		Stringp constantString(const char *s)
		{
			return internString(newString(s));
		}

		/**
		 * The interrupt method is called from executing code
		 * when the interrupted flag is set.
		 */
		virtual void interrupt(MethodEnv *env) = 0;

		/**
		 * This is called when the stack overflows
		 * (when the machine stack pointer is about to go below
		 *  minstack)
		 */
		virtual void stackOverflow(MethodEnv *env) = 0;

		void _stackOverflow(MethodEnv *env) { stackOverflow(env); }
		
		/**
		 * Throws an exception.  Constructs an Exception object
		 * and calls throwException.
		 */
		void throwAtom(Atom atom);

		/**
		 * The AVM+ equivalent of the C++ "throw" statement.
		 * Throws an exception, transferring control to the
		 * nearest CATCH block.
		 */
		void throwException(Exception *exception);

		/**
		 * throwErrorV is a convenience function for throwing
		 * an exception with a formatted error message,
		 */
		void throwErrorV(ClassClosure *type, int errorID, Stringp arg1=0, Stringp arg2=0, Stringp arg3=0);

		/**
		 * formatErrorMessageV is a convenience function for
		 * assembling an error message with varags.
		 */
		String* formatErrorMessageV( int errorID, Stringp arg1=0, Stringp arg2=0, Stringp arg3=0);

		/**
		 * Convenience methods for converting various objects into value 
		 * strings used for error message output.
		 */
		String* toErrorString(int d);
		String* toErrorString(AbstractFunction* m);
		String* toErrorString(Multiname* n);
		String* toErrorString(Namespace* ns);
		String* toErrorString(Traits* t);
		String* toErrorString(const char* s);
		String* toErrorString(const wchar* s);
		String* atomToErrorString(Atom a);

		/**
		 * getErrorMessage returns the format string for an
		 * errorID.  Override to provide format strings for
		 * additional implementation-dependent error strings.
		 */
		virtual String* getErrorMessage(int errorID);

		#ifdef DEBUGGER
		/**
		 * willExceptionBeCaught walks all the way up the
		 * ActionScript stack to see if there is any "catch"
		 * statement which is going to catch the specified
		 * exception.
		 */
		bool willExceptionBeCaught(Exception* exception);

		/**
		 * findErrorMessage searches an error messages table.
		 * Only available in debugger builds.
		 */
		String* findErrorMessage(int errorID,
								 int* mapTable,
								 const char** errorTable,
								 int numErrors);

		/**
		 * Determines the language id of the given platform
		 */
		virtual int determineLanguage();
		int langID;
		
		/** The call stack of currently executing code. */
		CallStackNode *callStack;

		/**
		 * Creates a StackTrace from the current executing call stack
		 */
		StackTrace* newStackTrace();

		StackTrace *getStackTrace();
		StackTrace *getStackTrace(void/*StackTrace::Element*/ *e, int depth);
		void rehashTraces(int newSize);
		int findTrace(void/*StackTrace::Element*/ *e, int depth);
		StackTrace **stackTraces;
		uint32 numTraces;

		/* used to charge primitive allocations (String and Namespace) */
		static void chargeAllocation(Atom a);

		#ifdef _DEBUG
		void dumpStackTrace();
		#endif
#endif /* DEBUGGER */

		CodeContextAtom codeContextAtom;

		CodeContext* codeContext() const;

		/** env of the highest catch handler on the call stack, or NULL */
		ExceptionFrame *exceptionFrame;
		
		Exception *exceptionAddr;

		/**
		 * Searches the exception handler table of info for
		 * a try/catch block that contains the instruction at pc
		 * and matches the type of exception.
		 * @param info      the method to search the exception handler
		 *                  table of
		 * @param pc        the program counter at the point where
		 *                  the exception occurred; either a pointer into
		 *                  bytecode or into native compiled code
		 * @param exception the exception object that was thrown;
		 *                  used to match the type.
		 * @return ExceptionHandler object for the exception
		 *         handler that matches, or re-throws the passed-in
		 *         exception if no handler is found.
		 */
		ExceptionHandler* findExceptionHandler(MethodInfo *info,
											   sintptr pc,
											   Exception *exception);
		
		ExceptionHandler* beginCatch(ExceptionFrame *ef,
				MethodInfo *info, sintptr pc, Exception *exception);

		/**
		 * Just like findExceptionHandler(), except that this function
		 * returns NULL if it can't find an exception handler, whereas
		 * findExceptionHandler() re-throws the passed-in exception if
		 * it can't find a handler.
		 */
		ExceptionHandler* findExceptionHandlerNoRethrow(MethodInfo *info,
														sintptr pc,
														Exception *exception);

		/**
		 * Returns true if the passed atom is an XML object,
		 * as defined in the E4X Specification.
		 */				
		bool isXML (Atom atm);

		/* Returns tru if the atom is a Date object */
		bool isDate(Atom atm);

		// From http://www.w3.org/TR/2004/REC-xml-20040204/#NT-Name
		bool isLetter (wchar c);
		bool isDigit (wchar c);
		bool isCombiningChar (wchar c);
		bool isExtender (wchar c);

		Stringp ToXMLString (Atom a);
		Stringp EscapeElementValue (const Stringp s, bool removeLeadingTrailingWhitespace);
		Stringp EscapeAttributeValue (Atom v);

		/**
		 * Converts an Atom to a E4XNode if its traits match.
		 * Otherwise, null is returned. (An exception is NOT thrown)
		 */
		E4XNode *atomToXML (Atom atm);

		/**
		 * Converts an Atom to a XMLObject if its traits match.
		 * Otherwise, null is returned. (An exception is NOT thrown)
		 */
		XMLObject *atomToXMLObject (Atom atm);

		/**
		 * Returns true if the passed atom is a XMLList object,
		 * as defined in the E4X Specification.
		 */		
		bool isXMLList (Atom atm);

		/**
		 * Converts an Atom to a XMLListObject if its traits match.
		 * Otherwise, null is returned. (An exception is NOT thrown)
		 */
		XMLListObject *atomToXMLList (Atom atm);

		/**
		 * Returns true if the passed atom is a QName object,
		 * as defined in the E4X Specification.
		 */		
		bool isQName (Atom atm);

		/**
		 * Returns true if the passed atom is a Dictionary object,
		 * as defined in the E4X Specification.
		 */		
		bool isDictionary (Atom atm);

		bool isDictionaryLookup(Atom key, Atom obj)
		{
			return isObject(key) && isDictionary(obj);
		}

		/**
		 * Returns true if the passed atom is a valid XML name,
		 * as defined in the E4X Specification.
		 */		
		bool isXMLName(Atom arg);

		/**
		 * Converts an Atom to a QNameObject if its traits match.
		 * Otherwise, null is returned. (An exception is NOT thrown)
		 */
		QNameObject *atomToQName (Atom atm);

		/** Implementation of OP_typeof */		
		Stringp _typeof (Atom arg);

		/** The XML entities table, used by E4X */
		Hashtable *xmlEntities;
		
	private:
		DECLARE_NATIVE_CLASSES()
		DECLARE_NATIVE_SCRIPTS()

		void registerNatives(NativeTableEntry *nativeMap, AbstractFunction *nativeMethods[]);

		//
		// this used to be Heap
		//

		/** size of interned String table */
		int stringCount;

		/** number of deleted entries in our String table */
		int deletedCount;
		#define AVMPLUS_STRING_DELETED ((Stringp)(1))

		/** size of interned Namespace table */
		int nsCount;

		int numStrings;
		int numNamespaces;
				
	public:

		static Namespace *atomToNamespace(Atom atom)
		{
			AvmAssert((atom&7)==kNamespaceType);
			return (Namespace*)(atom&~7);
		}
		
		static double atomToDouble(Atom atom)
		{
			AvmAssert((atom&7)==kDoubleType);

			double* obj = (double*)(atom&~7);
			return *obj;
		}

		/**
		 * Convert an Atom of kStringType to a Stringp
		 * @param atom atom to convert.  Note that the Atom
		 *             must be of kStringType
		 */
		static Stringp atomToString(Atom atom)
		{
			AvmAssert((atom&7)==kStringType);
			return (Stringp)(atom&~7);
		}

		// Avoid adding validation checks here and returning NULL.  If this
		// is returning a bad value, the higher level function should be fixed
		// or AbcParser/Verifier should be enhanced to catch this case.
		static ScriptObject* atomToScriptObject(const Atom atom)
		{
			AvmAssert((atom&7)==kObjectType);
			return (ScriptObject*)(atom&~7);
		}
	
		// helper function, allows generic objects to work with Hashtable
		// and AtomArray, uses double type which is the only non-RC
		// GCObject type
		static Atom gcObjectToAtom(const void* obj);
		static const void* atomToGCObject(Atom a) { return (const void*)(a&~7); }
		static bool isGCObject(Atom a) { return (a&7)==kDoubleType; }

	private:
		/** search the string intern table */
		int findString(const wchar *s, int len);

		/** search the namespace intern table */
		int findNamespace(const Namespace *ns);

#ifdef DEBUGGER
#if defined(MMGC_IA32) || defined(MMGC_IA64)
		static inline uint32 FindOneBit(uint32 value)
		{
#ifndef __GNUC__
			_asm
			{
				bsr eax,[value];
			}
#else
			// DBC - This gets rid of a compiler warning and matchs PPC results where value = 0
			register int	result = ~0;
			
			if (value)
			{
				asm (
					"bsr %1, %0"
					: "=r" (result)
					: "m"(value)
					);
			}
			return result;
#endif
		}

		#elif defined(MMGC_PPC)

		static inline int FindOneBit(uint32 value)
		{
			register int index;
			#ifdef DARWIN
			asm ("cntlzw %0,%1" : "=r" (index) : "r" (value));
			#else
			register uint32 in = value;
			asm { cntlzw index, in; }
			#endif
			return 31-index;
		}

		#else // generic platform

		static int FindOneBit(uint32 value)
		{
			for (int i=0; i < 32; i++)
				if (value & (1<<i))
					return i;
			// asm versions of this function are undefined if no bits are set
			AvmAssert(false);
			return 0;
		}

		#endif  // MMGC_platform
#endif

	public:
		/**
		 * intern the given string atom which has already been allocated
		 * @param atom
		 * @return
		 */
		Stringp internString(Stringp s);
		Stringp internString(Atom atom);
		Stringp internInt(int n);
		Stringp internDouble(double d);
		Stringp internUint32(uint32 ui);

		/**
		 * intern the given string and allocate it on the heap if necessary
		 * @param s
		 * @return
		 */
		Stringp internAlloc(const wchar *s, int len);
		Stringp internAllocUtf8(const byte *s, int len);

		// doesn't do any heap allocations if the string is interned
		Stringp internAllocAscii(const char *str);

		bool getIndexFromAtom (Atom a, uint32 *result) const
		{
			if (AvmCore::isInteger(a))
			{
				*result = uint32(a >> 3);
				return true;
			}
			else
			{
				AvmAssert (AvmCore::isString(a));
				return AvmCore::getIndexFromString (atomToString (a), result); 
			}
		}

		static bool getIndexFromString(Stringp s, uint32 *result);
			
		ScriptBufferImpl* newScriptBuffer(size_t size);
		VTable* newVTable(Traits* traits, VTable* base, ScopeChain* scope, AbcEnv* abcEnv, Toplevel* toplevel);

		RegExpObject* newRegExp(RegExpClass* regExpClass,
								Stringp pattern,
								Stringp options);

		ScriptObject* newObject(VTable* ivtable, ScriptObject *delegate);

		// like newObject but runs init if there is one
		ScriptObject* newActivation(VTable *vtable, ScriptObject *delegate);

		/**
		 * traits with base traits (inheritance)
		 */
		Traits* newTraits(Traits *base,
						  int nameCount,
						  int interfaceCount,
						  size_t sizeofInstance);
		
		FrameState* newFrameState(int frameSize, int scopeBase, int stackBase);
        Namespace* newNamespace(Atom prefix, Atom uri, Namespace::NamespaceType type = Namespace::NS_Public);
		Namespace* newNamespace(Atom uri, Namespace::NamespaceType type = Namespace::NS_Public);
		Namespace* newNamespace(Stringp uri, Namespace::NamespaceType type = Namespace::NS_Public);
		Namespace* newPublicNamespace(Stringp uri) { return newNamespace(uri); }
		NamespaceSet* newNamespaceSet(int nsCount);

		// String creation
		Stringp newString(const char *str) const;
		Stringp newString(const wchar *str) const;
		Stringp newString(const char *str, int len) const;		

		Stringp uintToString(uint32 i);
		Stringp intToString(int i);
		Stringp doubleToString(double d);
		Stringp concatStrings(Stringp s1, Stringp s2) const;
		
		Atom doubleToAtom(double n);
		Atom uintToAtom(uint32 n);
		Atom intToAtom(int n);

		Atom allocDouble(double n)
		{
			double *ptr = (double*)GetGC()->Alloc(sizeof(double), 0);
			*ptr = n;
			return kDoubleType | (uintptr)ptr;
		}
		
		void rehashStrings(int newlen);
		void rehashNamespaces(int newlen);

		// static version for smart pointers
		static void atomWriteBarrier(MMgc::GC *gc, const void *container, Atom *address, Atom atomNew);
#ifdef MMGC_DRC
		static void decrementAtomRegion(Atom *ar, int length);
#endif

#ifdef AVMPLUS_VERBOSE
	public:
		Stringp format(Atom atom);
		Stringp formatAtomPtr(Atom atom);
#endif

#ifdef DEBUGGER
	public:
		int sizeInternedTables();
#endif

	// NOT MARKED.  These fields are at the end of core and are explicitly
	// excluded from being traced in AvmCore::trace
	private:
		// hash set containing intern'ed strings
		DRC(Stringp) * strings;
		// hash set containing namespaces
		DRC(Namespacep) * namespaces;

		// avoid multiple inheritance issues
		class GCInterface : MMgc::GCCallback
		{
		public:
			GCInterface(MMgc::GC * _gc) : MMgc::GCCallback(_gc) {}
			void SetCore(AvmCore* _core) { this->core = _core; }
			void presweep() { core->presweep(); }
			void postsweep() { core->postsweep(); }
			void log(const char *str) { core->console << str; }
		private:
			AvmCore *core;
		};
		GCInterface gcInterface;
	};
}

#endif /* __avmplus_AvmCore__ */
