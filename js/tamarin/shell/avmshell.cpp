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


#include "avmshell.h"

#if defined(DARWIN) || defined(AVMPLUS_LINUX)
#include <sys/signal.h>
#include <unistd.h>
#endif

#ifdef WIN32
#include <Mmsystem.h>
#include "dbghelp.h"
bool P4Available();
#elif defined AVMPLUS_LINUX
bool P4Available();
#endif

static MMgc::FixedMalloc* fm;

#ifndef OVERRIDE_GLOBAL_NEW
// Custom new and delete operators
// User-defined operator new.

void *operator new(size_t size)
{
	// 10.5 calls new before main
	if (!fm)
	{
		MMgc::GCHeap::Init();
		MMgc::FixedMalloc::Init();

		fm = MMgc::FixedMalloc::GetInstance();
	}

    return fm->Alloc(size);
}

void *operator new[](size_t size)
{
	// 10.5 calls new before main
	if (!fm)
	{
		MMgc::GCHeap::Init();
		MMgc::FixedMalloc::Init();

		fm = MMgc::FixedMalloc::GetInstance();
	}

    return fm->Alloc(size);
}

// User-defined operator delete.
#ifdef _MAC
	// CW9 wants the C++ official prototype, which means we must have an empty exceptions list for throw.
	// (The fact exceptions aren't on doesn't matter.) - mds, 02/05/04
	void operator delete( void *p) throw()
#else
	void operator delete( void *p)
#endif
	{
		if (fm)
			fm->Free(p);
	}

#ifdef _MAC
    // CW9 wants the C++ official prototype, which means we must have an empty exceptions list for throw.
    // (The fact exceptions aren't on doesn't matter.) - mds, 02/05/04
    void operator delete[]( void *p) throw()
#else
    void operator delete[]( void *p )
#endif
    {
		if (fm)
			fm->Free(p);
    }
#endif // OVERRIDE_GLOBAL_NEW


namespace avmshell
{
	#include "toplevel.cpp"

	const int kScriptTimeout = 15;
	const int kScriptGracePeriod = 5;

	BEGIN_NATIVE_CLASSES(Shell)
		NATIVE_CLASS(abcclass_avmplus_System,          SystemClass,        ScriptObject)
		NATIVE_CLASS(abcclass_avmplus_File,            FileClass,          ScriptObject)
		NATIVE_CLASS(abcclass_avmplus_Domain,          DomainClass,        DomainObject)
		NATIVE_CLASS(abcclass_avmplus_StringBuilder,   StringBuilderClass, StringBuilderObject)		
		NATIVE_CLASS(abcclass_avmplus_JObject,          JObjectClass,		JObject)
		NATIVE_CLASS(abcclass_flash_utils_ByteArray,    ByteArrayClass,     ByteArrayObject)		
		NATIVE_CLASS(abcclass_flash_utils_ShortArray,   ShortArrayClass,    ShortArrayObject)		
		NATIVE_CLASS(abcclass_flash_utils_UShortArray,  UShortArrayClass,   UShortArrayObject)		
		NATIVE_CLASS(abcclass_flash_utils_IntArray,     IntArrayClass,      IntArrayObject)		
		NATIVE_CLASS(abcclass_flash_utils_UIntArray,    UIntArrayClass,     UIntArrayObject)		
		NATIVE_CLASS(abcclass_flash_utils_FloatArray,   FloatArrayClass,    FloatArrayObject)		
		NATIVE_CLASS(abcclass_flash_utils_DoubleArray,  DoubleArrayClass,   DoubleArrayObject)	
		NATIVE_CLASS(abcclass_flash_utils_Dictionary,   DictionaryClass,    DictionaryObject)
	END_NATIVE_CLASSES()

	BEGIN_NATIVE_SCRIPTS(Shell)
		NATIVE_SCRIPT(0/*abcscript_avmplus_debugger*/, AvmplusScript)
	END_NATIVE_SCRIPTS()

	BEGIN_NATIVE_MAP(AvmplusScript)
	END_NATIVE_MAP()

	Shell *shell = NULL;
	bool show_error = false;

	#ifdef WIN32
	void Shell::computeStackBase()
	{
		const int kStackMargin = 131072;
		
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);

		int dummy;
		int sp = (int)(&dummy);
		sp &= ~(sysinfo.dwPageSize-1);

		MEMORY_BASIC_INFORMATION buf;
		if (VirtualQuery((void*)sp, &buf, sizeof(buf)) == sizeof(buf)) {
			minstack = (uint32)buf.AllocationBase + kStackMargin;
		}
	}
	
	void CALLBACK TimeoutProc(UINT /*uTimerID*/,
							  UINT /*uMsg*/,
							  DWORD_PTR dwUser,
							  DWORD_PTR /*dw1*/,
							  DWORD_PTR /*dw2*/)
	{
		AvmCore *core = (AvmCore*)dwUser;
		core->interrupted = true;
	}
	#else
	void Shell::computeStackBase()
	{
		const int kMaxAvmPlusStack = 512*1024;
		int sp;
		#ifdef AVMPLUS_PPC
		asm("mr %0,r1" : "=r" (sp));
        #elif defined(AVMPLUS_ARM)
		asm("mov %0,sp" : "=r" (sp));
		#else
		asm("movl %%esp,%0" : "=r" (sp));
		#endif
		minstack = sp-kMaxAvmPlusStack;
	}
	
	void alarmProc(int /*signum*/)
	{
		shell->interrupted = true;
	}
	#endif
	
	void Shell::usage()
	{
		printf("avmplus shell " AVMPLUS_VERSION_USER " build " AVMPLUS_BUILD_CODE "\n\n");
		printf("usage: avmplus\n");
		#ifdef DEBUGGER
			printf("          [-d]          enter debugger on start\n");
		#endif
		#ifdef AVMPLUS_PROFILE
			printf("          [-Ddprofile]  dynamic instruction stats\n");
			printf("          [-Dsprofile]  show static instruction stats\n");
		#endif /* AVMPLUS_PROFILE */
		#ifdef _DEBUG
			printf("          [-Dgreedy]    collect before every allocation\n");
		#endif /* _DEBUG */
		#ifdef DEBUGGER
			printf("          [-Dnogc]      don't collect\n");
			printf("          [-Dgcstats]   generate statistics on gc\n");
			printf("          [-Dnoincgc]   don't use incremental collection\n");
			printf("          [-Dastrace N] display AS execution information, where N is [1..4]\n");
		#endif

		#ifdef AVMPLUS_INTERP
			printf("          [-Dinterp]    do not generate machine code, interpret instead\n");
		#endif /* AVMPLUS_INTERP */

		#ifdef AVMPLUS_VERBOSE
			printf("          [-Dverbose]   trace every instruction (verbose!)\n");
			printf("          [-Dbbgraph]   output MIR basic block graphs for use with Graphviz\n");
		#endif

    #ifdef AVMPLUS_MIR
		#ifdef AVMPLUS_INTERP
		    printf("          [-Dforcemir]  use MIR always, never interp\n");
        #endif /* AVMPLUS_INTERP */

		printf("          [-Dnodce]     disable DCE optimization \n");
		printf("          [-Dnocse]     disable CSE optimization \n");

        #ifdef AVMPLUS_IA32
            printf("          [-Dnosse]     use FPU stack instead of SSE2 instructions\n");
        #endif /* AVMPLUS_IA32 */
    #endif
		
		#ifdef AVMPLUS_VERIFYALL
		    printf("          [-Dverifyall] verify greedily instead of lazily\n");
		#endif

		printf("          [-Dtimeout]   enforce maximum 15 seconds execution\n");
		printf("          [-error]      crash opens debug dialog, instead of dumping\n");
		#ifdef AVMPLUS_INTERACTIVE
		printf("          [-i]          interactive mode\n");
		#endif //AVMPLUS_INTERACTIVE
		printf("          [-log]\n");
		printf("          [-- args]     args passed to AS3 program\n");
		printf("          [-jargs ... ;] args passed to Java runtime\n");
		printf("          filename.abc ...\n");
		printf("          [--] application args\n");
		exit(1);
	}

	void Shell::stackOverflow(MethodEnv *env)
	{
		if (inStackOverflow)
		{
			// Already handling a stack overflow, so do not
			// re-enter handler.
			return;
		}
			
		// Temporarily disable stack overflow checks
		// so that we can construct an exception object.
		// There should be plenty of margin before the
		// actual stack bottom to do this.
		inStackOverflow = true;

		Toplevel *toplevel = env->vtable->toplevel;

		Stringp errorMessage = getErrorMessage(kStackOverflowError);
		Atom args[2] = { nullObjectAtom, errorMessage->atom() };
		Atom errorAtom = toplevel->errorClass()->construct(1, args);
		Exception *exception = new (GetGC()) Exception(errorAtom
                                                  #ifdef DEBUGGER
												  ,this
                                                  #endif
		);

		// Restore stack overflow checks
		inStackOverflow = false;

		// Throw the stack overflow exception
		throwException(exception);
	}
		
	void Shell::interrupt(MethodEnv *env)
	{
		interrupted = false;

		Toplevel *toplevel = env->vtable->toplevel;

		if (gracePeriod) {
			// This script has already had its chance; it violated
			// the grace period.
			// Throw an exception it cannot catch.
			Stringp errorMessage = getErrorMessage(kScriptTerminatedError);
			Atom args[2] = { nullObjectAtom, errorMessage->atom() };
			Atom errorAtom = toplevel->errorClass()->construct(1, args);
			Exception *exception = new (GetGC()) Exception(errorAtom
													  #ifdef DEBUGGER
													  ,this
													  #endif
			                                          );
			exception->flags |= Exception::EXIT_EXCEPTION;
			throwException(exception);
		}

		// Give the script an additional grace period to
		// clean up, and throw an exception.
		gracePeriod = true;

		#ifdef WIN32
		timeSetEvent(kScriptGracePeriod*1000,
					 kScriptGracePeriod*1000,
					 (LPTIMECALLBACK)TimeoutProc,
					 (DWORD_PTR)this,
					 TIME_ONESHOT);
		#else
		#ifndef AVMPLUS_ARM // TODO AVMPLUS_ARM
		alarm(kScriptGracePeriod);
		#endif
		#endif

		toplevel->throwError(kScriptTimeoutError);
	}
	
	void Shell::initShellPool()
	{
		AbstractFunction *nativeMethods[toplevel_abc_method_count];
		NativeClassInfo *nativeClasses[toplevel_abc_class_count];
		NativeScriptInfo *nativeScripts[toplevel_abc_script_count];

		memset(nativeMethods, 0, sizeof(AbstractFunction*)*toplevel_abc_method_count);
		memset(nativeClasses, 0, sizeof(NativeClassInfo*)*toplevel_abc_class_count);
		memset(nativeScripts, 0, sizeof(NativeScriptInfo*)*toplevel_abc_script_count);

		initNativeTables(classEntries, scriptEntries, 
			nativeMethods, nativeClasses, nativeScripts);

		avmplus::ScriptBuffer code = newScriptBuffer(toplevel_abc_length);
		memcpy(code.getBuffer(), toplevel_abc_data, toplevel_abc_length);
		shellPool = parseActionBlock(code, 0, NULL, builtinDomain, nativeMethods, nativeClasses, nativeScripts);
	}

	Toplevel* Shell::initShellBuiltins()
	{
		// Initialize a new Toplevel.  This will also create a new
		// DomainEnv based on the builtinDomain.
		Toplevel* toplevel = initTopLevel();

		// Initialize the shell builtins in the new Toplevel
		handleActionPool(shellPool,
						 toplevel->domainEnv(),
						 toplevel,
						 NULL);

		return toplevel;
	}
	
	Shell::Shell(MMgc::GC *gc) : AvmCore(gc)
	{
		#ifdef DEBUGGER
		debugCLI = NULL;
		#endif

		systemClass = NULL;
		
		gracePeriod = false;
		inStackOverflow = false;

		consoleOutputStream = new (gc) ConsoleOutputStream();
		setConsoleStream(consoleOutputStream);

		computeStackBase();
	}

	bool Shell::executeProjector(int argc, char *argv[], int& exitCode)
	{
		TRY(this, kCatchAction_ReportAsError)
		{
			uint8 header[8];

			char executablePath[256];

			#ifdef WIN32
			GetModuleFileName(NULL, executablePath, sizeof(executablePath));
			#else
			strncpy(executablePath, argv[0], sizeof(executablePath));
			#endif
		   
			FileInputStream file(executablePath);
			if (!file.valid())
			{
				return false;
			}
			
			file.seek(file.length() - 8);
			file.read(header, 8);

			// Check the magic number
			if (header[0] != 0x56 &&
				header[1] != 0x34 &&
				header[2] != 0x12 &&
				header[3] != 0xFA)
			{
				return false;
			}

			int abcLength = (header[4]     |
							 header[5]<<8  |
							 header[6]<<16 |
							 header[7]<<24);

			ScriptBuffer code = newScriptBuffer(abcLength);
			file.seek(file.length() - 8 - abcLength);
			file.read(code.getBuffer(), abcLength);
			
			initBuiltinPool();
			initShellPool();

			#ifdef DEBUGGER
			// Create the debugger
			debugCLI = new (GetGC()) DebugCLI(this);
			debugger = debugCLI;

			// Create the profiler
			profiler = new Profiler(GetGC());
			#endif

			SystemClass::user_argc = argc-1;
			SystemClass::user_argv = &argv[1];
		
			// init toplevel internally
			Toplevel* toplevel = initShellBuiltins();

			// Create a new Domain for the user code
			Domain* domain = new (GetGC()) Domain(this,
											 builtinDomain);

			// Return a new DomainEnv for the user code
			DomainEnv* domainEnv = new (GetGC()) DomainEnv(this,
													  domain,
													  toplevel->domainEnv());

			ShellCodeContext* codeContext = new (GetGC()) ShellCodeContext();
			codeContext->domainEnv = domainEnv;
				
			// parse new bytecode
			handleActionBlock(code, 0, domainEnv, toplevel, NULL, NULL, NULL, codeContext);

			#ifdef DEBUGGER
			delete profiler;
			#endif
		}
		CATCH(Exception *exception)
		{
			#ifdef DEBUGGER
			if (!(exception->flags & Exception::SEEN_BY_DEBUGGER))
			{
				console << string(exception->atom) << "\n";
			}
			if (exception->getStackTrace()) {
				console << exception->getStackTrace()->format(this) << '\n';
			}
			delete profiler;
			#else
			// [ed] always show error, even in release mode,
			// see bug #121382
			console << string(exception->atom) << "\n";
			#endif
			exit(1);
		}
		END_CATCH
		END_TRY
				
		#ifdef AVMPLUS_PROFILE
			dump();
		#endif

		exitCode = 0;
		return true;
	}
		
	int Shell::main(int argc, char *argv[])
	{
		TRY(this, kCatchAction_ReportAsError)
		{
			#if defined (WIN32) || defined(AVMPLUS_LINUX)
			if (!P4Available()) {
				sse2 = false;
			}
			#endif

			int exitCode = 0;
			if (executeProjector(argc, argv, exitCode))
			{
				return exitCode;
			}
						
			if (argc < 2) {
				usage();
			}

			int filenamesPos = -1;
			int endFilenamePos = -1;
			char *filename = NULL;
			bool do_log = false;
#ifdef DEBUGGER
			bool do_debugger = false;
#endif
			bool do_interactive = false;
#ifdef AVMPLUS_VERBOSE
			bool do_verbose = false;
#endif

			for (int i=1; i<argc; i++) {
				char *arg = argv[i];
				// options available to development builds.
				if (arg[0] == '-') 
				{
					if (arg[1] == 'D') {
						if (!strcmp(arg+2, "timeout")) {
							interrupts = true;

						#ifdef AVMPLUS_IA32
						} else if (!strcmp(arg+2, "nosse")) {
							sse2 = false;
						#endif

	                    #ifdef AVMPLUS_VERIFYALL
						} else if (!strcmp(arg+2, "verifyall")) {
							verifyall = true;
		                #endif /* AVMPLUS_VERIFYALL */

	                    #ifdef _DEBUG
						} else if (!strcmp(arg+2, "greedy")) {
							GetGC()->greedy = true;
		                #endif /* _DEBUG */

						#ifdef AVMPLUS_PROFILE
						} else if (!strcmp(arg+2, "dprofile")) {
							dprof.dprofile = true;
						} else if (!strcmp(arg+2, "sprofile")) {
							sprof.sprofile = true;
						#endif /* AVMPLUS_PROFILE */

	                    #ifdef DEBUGGER
						} else if (!strcmp(arg+2, "gcstats")) {
							GetGC()->gcstats = true;
						} else if (!strcmp(arg+2, "nogc")) {
							GetGC()->nogc = true;
						} else if (!strcmp(arg+2, "noincgc")) {
							GetGC()->incremental = false;
						} else if (!strcmp(arg+2, "astrace")) {
							avmplus::Debugger::astrace = (avmplus::Debugger::TraceLevel) strtol(argv[++i], 0, 10);
                    	#endif /* DEBUGGER */
						#ifdef AVMPLUS_INTERP
						} else if (!strcmp(arg+2, "interp")) {
							turbo = false;
		                #endif /* AVMPLUS_INTERP */
						#ifdef AVMPLUS_VERBOSE
						} else if (!strcmp(arg+2, "verbose")) {
							do_verbose = true;
						#endif

	                #ifdef AVMPLUS_MIR
						#ifdef AVMPLUS_INTERP
						} else if (!strcmp(arg+2, "forcemir")) {
							forcemir = true;
                        #endif /* AVMPLUS_INTERP */

						} else if (!strcmp(arg+2, "nodce")) {
							dceopt = false;
							
						} else if (!strcmp(arg+2, "nocse")) {
							cseopt = false;

                        #ifdef AVMPLUS_VERBOSE
						} else if (!strcmp(arg+2, "bbgraph")) {
							bbgraph = true;  // generate basic block graph (only valid with mir switch)
                        #endif
                    #endif /* AVMPLUS_MIR */

						} else {
							usage();
						}
					} else if (!strcmp(arg, "-log")) {
						do_log = true;
					#ifdef AVMPLUS_INTERACTIVE
					} else if (!strcmp(arg, "-i")) {
						do_interactive = true;
					#endif //AVMPLUS_INTERACTIVE
					}
					else if (!strcmp(arg, "-error")) {
						show_error = true;
						#ifdef WIN32
						SetErrorMode(0);  // set to default
						#endif // WIN32
					}
#ifdef AVMPLUS_WITH_JNI
					else if (!strcmp(arg, "-jargs")) {
						// all the following args until the semi colon is for java.
						//@todo fix up this hard limit
						bool first = true;
						Java::startup_options = new char[256];
						memset(Java::startup_options, 0, 256);

						for(i++; i<argc; i++)
						{
							if (*argv[i] == ';')
								break;
							if (!first) strcat(Java::startup_options, " ");
							strcat(Java::startup_options, argv[i]);
							first = false;
						}
						AvmAssert(strlen(Java::startup_options) < 256);
					}
#endif /* AVMPLUS_WITH_JNI */
                                    
	                #ifdef DEBUGGER
					else if (!strcmp(arg, "-d")) {
						do_debugger = true;
					}
		            #endif /* DEBUGGER */

					else if(arg[1] == '-' && arg[2] == 0) {
						endFilenamePos = i;
					}
					else {
						usage();
					}
				} 
				else
				{
					if (filenamesPos == -1)
						filenamesPos = i;
					filename = arg;
				}
			}
		
			if (!filename && !do_interactive) {
				usage();
			}

			if( do_log )
			{
				// open logfile based on last filename
				const char* dot = strrchr(filename, '.');
				if (!dot)
					dot = filename+strlen(filename);

				char* logname = new char[dot-filename+5];  // free upon exit
				strcpy(logname,filename);
				strcpy(logname+(dot-filename),".log");
				printf("%s\n",filename); // but first print name to default stdout
				freopen(logname, "w", stdout);
				delete [] logname;
			}

			initBuiltinPool();
			initShellPool();

#ifdef AVMPLUS_VERBOSE
			if (do_verbose)
				verbose = true;
#endif

			#ifdef DEBUGGER
			// Create the debugger
			debugCLI = new (GetGC()) DebugCLI(this);
			debugger = debugCLI;

			// Create the profiler
			profiler = new Profiler(GetGC());

			if (do_debugger)
			{
				// Activate the debug CLI and stop at
				// start of program
				debugCLI->activate();
				debugCLI->stepInto();
			}
			#endif

			// start the 15 second timeout if applicable
			if (interrupts) {
				#ifdef WIN32
				timeSetEvent(kScriptTimeout*1000,
							 kScriptTimeout*1000,
							 (LPTIMECALLBACK)TimeoutProc,
							 (DWORD_PTR)this,
							 TIME_ONESHOT);
				#else
				#ifndef AVMPLUS_ARM // TODO AVMPLUS_ARM
				signal(SIGALRM, alarmProc);
				alarm(kScriptTimeout);
				#endif
				#endif
			}

			if(endFilenamePos == -1)
				endFilenamePos = argc;
			SystemClass::user_argc = argc-endFilenamePos-1; 
			SystemClass::user_argv = &argv[endFilenamePos+1];
		
			// init toplevel internally
			Toplevel* toplevel = initShellBuiltins();

			// Create a new Domain for the user code
			Domain* domain = new (GetGC()) Domain(this,
											 builtinDomain);

			// Return a new DomainEnv for the user code
			DomainEnv* domainEnv = new (GetGC()) DomainEnv(this,
													  domain,
													  toplevel->domainEnv());

			ShellCodeContext* lastCodeContext = 0;

			// execute each abc file
			for (int i=filenamesPos; filename && i < endFilenamePos; i++)
			{
				filename = argv[i];

				#ifdef AVMPLUS_VERBOSE
				if (verbose) {
					console << "run " << filename << "\n";
				}
				#endif

				FileInputStream f(filename);
				bool isValid = f.valid();
				if (!isValid) {
					fprintf(stderr, "cannot open file: %s\n", filename);
					#ifdef DEBUGGER
					delete profiler;
					#endif
					if (!do_interactive) 
						return(1);
				}

				ShellCodeContext* codeContext = new (GetGC()) ShellCodeContext();
				codeContext->domainEnv = domainEnv;
				
				// parse new bytecode
				if (isValid)
				{
					ScriptBuffer code = newScriptBuffer(f.available());
					f.read(code.getBuffer(), f.available());
					handleActionBlock(code, 0, domainEnv, toplevel, NULL, NULL, NULL, codeContext);
				}

				lastCodeContext = codeContext;
			}

			#ifdef MMGC_COUNTERS
			console << "\nGC stats\n";
			console << "mark item         " << MMgc::GC::MarkItemCount << "\n";
			console << "mark null ptr     " << MMgc::GC::marknullCount << "\n";
			console << "mark alloc        " << MMgc::GC::markallocCount << "\n";
			console << "mark large        " << MMgc::GC::marklargeCount << "\n";
			console << "mark skip         " << MMgc::GC::markskipCount << "\n";
			console << "TrapWriteCount    " << MMgc::GC::TrapWriteCount << "\n";

			console << "\nGCAlloc stats\n";
			console << "SweepBlockCount   " << MMgc::GCAlloc::SweepBlockCount << "\n";
			console << "AllocCount        " << MMgc::GCAlloc::AllocCount << "\n";
			console << "FreeItemCount     " << MMgc::GCAlloc::FreeItemCount << "\n";
			#endif

			#ifdef AVMPLUS_INTERACTIVE
			if (do_interactive)
			{
				enum { kMaxCommandLine = 1024 };
				char commandLine[kMaxCommandLine];
				enum { kMaxFileName = 1024 };
				char fileName[kMaxFileName];
				char imports[kMaxCommandLine];
				strcpy(imports, " ");

				// some defaults
				addToImports(imports, "C:\\src\\farm\\main\\as\\lib\\shell.abc");
				addToImports(imports, "C:\\src\\farm\\main\\as\\lib\\global.abc");

				STARTUPINFO si;
				PROCESS_INFORMATION pi;

				while(do_interactive)
				{
					console << "(avmplus) ";
					fflush(stdout);
					fgets(commandLine, kMaxCommandLine, stdin);

					commandLine[strlen(commandLine)-1] = 0;

					// build up the file that we are going to compile
					bool compile = true;
					bool exec = true;
					fileName[0] = '\0';
					if (strstr(commandLine, ".run ") == commandLine)
					{
						// arg 
						strcpy(fileName, &commandLine[5]);

						// search for .as extension
						const char* dotAt = strrchr(fileName, '.');
						bool fail = true;
						if (dotAt)
						{
							if (strcmp(dotAt, ".abc") == 0)
							{
								compile = false;
								fail = false;
							}
							else if (strcmp(dotAt, ".as") == 0)
							{
								fail = false;
							}
						}

						if (fail)
						{
							console << "only .as and .abc files are supported \n";
							continue;
						}
					}
					else if (strstr(commandLine, ".import ") == commandLine)
					{
						// add to the import list
						strcpy(fileName, &commandLine[8]);
						compile = false;
						exec = false;

						if (!addToImports(imports, fileName))
						{
							console << "file does not exist; not added to import list \n";
						}
						console << imports << "\n";
					}
					else if (strstr(commandLine, ".quit") == commandLine)
					{
						return 0;
					}
					else if (commandLine[0] == '\0' ||  (strstr(commandLine, ".help") == commandLine) )
					{
						console << "ActionScript source can be directly entered on the command line.\nIt will be compiled and executed once the enter key is pressed.\nThe following directives are also recognized\n" ;
						console << ".run [f.as|f.abc]   - runs f, compiles f.as first if required\n" ;
						console << ".import f           - add f to the -import list for compiling \n" ;
						console << ".quit               - exits this shell \n" ;
						console << ".help               - displays help information \n" ;
						continue;
					}
					else
					{
						// put our command line contents in a file
						strcpy(fileName, "___file_for_io.as");
						FILE* f = fopen(fileName , "w");
						if (!f)
						{
							console << "i/o error \n";
							return 1;
						}			

						fputs(commandLine, f);
						fclose(f);
					}

					// set up for the compile if needed
					if (compile)
					{
						// Set the bInheritHandle flag so pipe handles are inherited. 
						SECURITY_ATTRIBUTES saAttr; 
						saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
						saAttr.bInheritHandle = TRUE; 
						saAttr.lpSecurityDescriptor = NULL; 

						HANDLE pRd, pWr;
						CreatePipe(&pRd, &pWr, &saAttr, 64*kMaxCommandLine);
						SetHandleInformation( pRd, HANDLE_FLAG_INHERIT, 0);  // don't inherit read portion; only allow writes from child proc
						SetHandleInformation( GetStdHandle(STD_INPUT_HANDLE), HANDLE_FLAG_INHERIT, 0);  // don't inherit stdin 

						ZeroMemory( &si, sizeof(si) );
						si.cb = sizeof(si);
						ZeroMemory( &pi, sizeof(pi) );
						si.hStdError = pWr;
						si.hStdOutput = pWr;
						si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
						si.dwFlags = STARTF_USESTDHANDLES;

						// now compile and wait
						commandLine[0] = '\0';
						strcpy(commandLine, "asc.exe -debug ");
						strcat(commandLine, imports);
						strcat(commandLine, fileName);
						DWORD err = CreateProcess(0, commandLine, 0,0,TRUE,0,0,0, &si, &pi);
						if (err)
						{
							// Wait until child process exits.
							WaitForSingleObject( pi.hProcess, 20000 );
						}
						else
						{
							console << "failed to compile err=0x";
							console.writeHexAddr(GetLastError());
							console << "\n";
							exec = false;
						}

						// Close process and thread handles. 
						CloseHandle( pi.hProcess );
						CloseHandle( pi.hThread );

						// now check the compile 
						CloseHandle(pWr);  // Close the write end of the pipe before reading from the read end of the pipe.
						commandLine[0] = '\0';
						DWORD dwRead = 0; 
						ReadFile( pRd, commandLine, kMaxCommandLine, &dwRead, NULL);
						if (dwRead > 0) commandLine[dwRead] = '\0';
						if ( !strstr(commandLine, "bytes written") )
						{
							// failed compile						
							console << commandLine;

							// dump the rest of the message
							for(;;)
							{
								if (!ReadFile( pRd, commandLine, kMaxCommandLine, &dwRead, NULL) || dwRead == 0)
									break;

								console << commandLine;
							}
							exec = false;
							console << "\n";
						}

						// now run the abc
						int afterDot = strlen(fileName) - 2;
						strcpy(&fileName[afterDot], "abc");
					}

					if (exec)
					{
						FileInputStream fl(fileName);
						bool isValid = fl.valid();
						if (isValid)
						{
							TRY(this, kCatchAction_ReportAsError)
							{
								ScriptBuffer code = newScriptBuffer(fl.available());
								fl.read(code.getBuffer(), fl.available());
								handleActionBlock(code, 0, domainEnv, toplevel, NULL, NULL, NULL, lastCodeContext);
							}
							CATCH(Exception *exception)
							{
								#ifdef DEBUGGER
								if (!(exception->flags & Exception::SEEN_BY_DEBUGGER))
								{
									console << string(exception->atom) << "\n";
								}
								if (exception->getStackTrace()) {
									console << exception->getStackTrace()->format(this) << '\n';
								}
								#else
								// [ed] always show error, even in release mode,
								// see bug #121382
								console << string(exception->atom) << "\n";
								#endif
							}
							END_CATCH
							END_TRY
						}
						else
						{
							console << "can't find " << fileName << "\n";
						}
					}
				}
			}
			#endif //AVMPLUS_INTERACTIVE

			#ifdef DEBUGGER
			delete profiler;
			#endif
		}
		CATCH(Exception *exception)
		{
			#ifdef DEBUGGER
			if (!(exception->flags & Exception::SEEN_BY_DEBUGGER))
			{
				console << string(exception->atom) << "\n";
			}
			if (exception->getStackTrace()) {
				console << exception->getStackTrace()->format(this) << '\n';
			}
			delete profiler;
			#else
			// [ed] always show error, even in release mode,
			// see bug #121382
			console << string(exception->atom) << "\n";
			#endif
			exit(1);
		}
		END_CATCH
		END_TRY
				
		#ifdef AVMPLUS_PROFILE
			dump();
		#endif
#ifdef AVMPLUS_WITH_JNI
		if (Java::startup_options) delete Java::startup_options;
#endif /* AVMPLUS_WITH_JNI */
		return 0;
	}

	#ifdef AVMPLUS_INTERACTIVE
	int Shell::addToImports(char* imports, char* addition)
	{
		int worked = 0;
		if (addition && addition[0] != '\0')
		{
			FileInputStream fl(addition);
			if (fl.valid())
			{
				strcat(imports, " ");
				strcat(imports, " -import \"");
				strcat(imports, addition);
				strcat(imports, "\" ");
				worked = 1;
			}
		}
		return worked;
	}
	#endif //AVMPLUS_INTERACTIVE
}

int _main(int argc, char *argv[])
{
	if (!fm)
	{
		MMgc::GCHeap::Init();
		MMgc::FixedMalloc::Init();

		fm = MMgc::FixedMalloc::GetInstance();
	}
	
	MMgc::GCHeap* heap = MMgc::GCHeap::GetGCHeap();

	// memory zero'ing check
/*	int *foo = new int[2];
	AvmAssert(memcmp(foo, "\0\0\0\0\0\0\0\0\0\0\0\0", 2*sizeof(int)) == 0);
	delete foo;*/

	int exitCode = 0;
	{
		MMgc::GC gc(heap);
		avmshell::shell = new avmshell::Shell(&gc);
		exitCode = avmshell::shell->main(argc, argv);
		delete avmshell::shell;
	}

	MMgc::FixedMalloc::Destroy();
	MMgc::GCHeap::Destroy();
	fm = 0;
 	return exitCode;
}

#ifdef AVMPLUS_WIN32
unsigned long CrashFilter(LPEXCEPTION_POINTERS pException, int exceptionCode)
{
	unsigned long result;
	if ((result = UnhandledExceptionFilter(pException)) != EXCEPTION_EXECUTE_HANDLER)
	{
		return result;
	}
	else if (avmshell::show_error)
	{
		// if -error option dont do a dump 
		return EXCEPTION_CONTINUE_SEARCH;
	}

	printf("avmplus crash: exception 0x%08lX occurred\n", exceptionCode);

	typedef BOOL (WINAPI *MINIDUMP_WRITE_DUMP)(
		HANDLE hProcess,
		DWORD ProcessId,
		HANDLE hFile,
		MINIDUMP_TYPE DumpType,
		PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
		PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
		PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

	HMODULE hDbgHelp = LoadLibrary("dbghelp.dll");
	MINIDUMP_WRITE_DUMP MiniDumpWriteDump_ = (MINIDUMP_WRITE_DUMP)GetProcAddress(hDbgHelp, 
						"MiniDumpWriteDump");

	if (MiniDumpWriteDump_)
	{
		MINIDUMP_EXCEPTION_INFORMATION  M;
		const char DumpPath[] = "avmplusCrash.dmp";

		M.ThreadId = GetCurrentThreadId();
		M.ExceptionPointers = pException;
		M.ClientPointers = 0;

		printf("Writing minidump crash log to %s\n", DumpPath);

		HANDLE hDumpFile = CreateFile(DumpPath, GENERIC_WRITE, 0, 
							          NULL, CREATE_ALWAYS,
								      FILE_ATTRIBUTE_NORMAL, NULL);

		MiniDumpWriteDump_(GetCurrentProcess(),
						   GetCurrentProcessId(),
						   hDumpFile,
						   MiniDumpNormal,
						   (pException) ? &M : NULL, NULL, NULL);

		CloseHandle(hDumpFile);
	}
	else
	{
		printf("minidump not available, no crash log written.\n");
	}

	return result;
}

int main(int argc, char *argv[])
{
	SetErrorMode(SEM_NOGPFAULTERRORBOX);
	int code = 0;
	__try
	{
		code = _main(argc, argv);
	}
	__except(CrashFilter(GetExceptionInformation(), GetExceptionCode()))
	{
		code = -1;
	}
	if (avmshell::show_error) printf("error %d", code);
	return code;
}
#else

#ifdef AVMPLUS_ARM
// TODO this is a hack until we learn how to determine stack top
// in ARM
int StackTop;
#endif

int main(int argc, char *argv[])
{
	#ifdef AVMPLUS_ARM
	int sp;
	asm("mov %0,sp" : "=r" (sp));
	StackTop = sp;
	#endif

#ifdef AVMPLUS_MACH_EXCEPTIONS
	GenericGuard::staticInit();
#endif
	int code = _main(argc, argv);
	if (avmshell::show_error) printf("error %d", code);
	
#ifdef AVMPLUS_MACH_EXCEPTIONS
	GenericGuard::staticDestroy();
#endif	

	return code;
}
#endif


