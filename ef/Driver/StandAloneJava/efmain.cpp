/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include "JavaVM.h"
#include "prprf.h"
#include "plstr.h"
#include "Exceptions.h"
#include "StackWalker.h"
#include "NativeCodeCache.h"	// for printMethodTable

#include "LogModule.h"

#ifdef __MWERKS__
 #include <Console.h>
 #include <OSUtils.h>
#endif

void realMain(void* arg);

struct StageName
{
	char *name;
	CompileStage stage;
};

const StageName stageNames[] =
{
	{"r", csRead},
	{"R", csRead},
	{"read", csRead},
	{"READ", csRead},
	{"p", csPreprocess},
	{"P", csPreprocess},
	{"preprocess", csPreprocess},
	{"PREPROCESS", csPreprocess},
	{"g", csGenPrimitives},
	{"G", csGenPrimitives},
	{"genprimitives", csGenPrimitives},
	{"GENPRIMITIVES", csGenPrimitives},
	{"o", csOptimize},
	{"O", csOptimize},
	{"optimize", csOptimize},
	{"OPTIMIZE", csOptimize},
	{"i", csGenInstructions},
	{"I", csGenInstructions},
	{"instructions", csGenInstructions},
	{"INSTRUCTIONS", csGenInstructions}
};
const nStageNames = sizeof(stageNames) / sizeof(StageName);


static bool getStage(const char *stageName, CompileStage &stage)
{
	const StageName *sn = stageNames;
	for (; sn != stageNames + nStageNames; sn++)
		if (!PL_strcmp(stageName, sn->name)) {
			stage = sn->stage;
			return true;
		}

	return false;
}


/* Options
 * -c, -classpath <canonical class-path>
 * -a, -all Compile all methods.
 * -m, -method <methodName> <sig> : Go to method whose simple name is
 *                         <methodName> and signature is <sig>. The -s option
 *                         specifies what to do with the method.
 * -mn, -methodName <methodName> : Go to method whose simple name is
 *                         <methodName>, which must not be overloaded. The -s option
 *                         specifies what to do with the method.
 * -s, -stage <stage: r, p, g, o, i> default is to generate instructions
 * -v, -verbose : verbose messages
 * -n, -noinvoke : do not invoke compiled method. This is automatically true
 *             if the compile stage is anything other than genInstructions.
 * -l, -lib <libname> : canonical name of native library to load at init time
 * -sys, -system : Initialize system class on start-up
 * -ta, -traceAll: enable method tracing for all methods
 * -t, -trace <className> <methodName> <signature>: Enable tracing for a method with the given fully
 *                           qualified className, simple methodName and java signature.
 * -bc, -breakCompile <className> <methodName> <signature>: Set a debug breakpoint just before compiling
 *                           the method with the given fully qualified className, simple methodName and
 *                           java signature.
 * -be, -breakExec <className> <methodName> <signature>: Set a debug breakpoint just before executing
 *                           the method with the given fully qualified className, simple methodName and
 *                           java signature.
 * -h, -help : Print help message

  * -log <module-name> <level> : turn on logging for <module-name> at level <level>. By default, logs are logged
 *                  to stderr; use -logfile to direct them to a file.
 * -lf, -logFile <filename> : specify the file to put logs into. "stderr" is a valid filename and indicates
 *                 stderr. -lf can be used more than once on the command-line; it over-rides the last value of
 *                 -lf. This can be used to log different modules to different files. For example,
 *                 -lf foo -log FieldOrMethod 4 -log ClassCentral 3 -lf stderr -log Codegen 5
 *                 logs modules FieldOrMethod and ClassCentral to the file foo and the module 
 *                 Codegen to standard error.
 * -ln, -logNames : print out a list of all log modules
 * -debug : Enable debugging
 * -ce, -catchHardwareExceptions : Catch all hardware exceptions (used in the debug builds only) 
 */
struct Options {
	CompileStage stage;     /* Stage upto which method must be compiled */
	bool compileAll;        /* If true, compile all method in class */
	bool verbose;           /* Cause verbose output to be emitted when compiling */
	bool initialize;        /* If true, initialize system class */
	bool invokeMethod;      /* If false, do not invoke method after compiling */
	bool emitHTML;		    /* If true, output a HTML file for each jit'd method */
	bool traceAllMethods;   /* If true, invoking any method generates a debugging trace */
	const char *methodName; /* Name of method to compile */
	const char *methodSig;  /* Signature of method to compile */
	const char *className;  /* Name of class to load */
	const char *classPath; 
	const char *logFileName;/* Name of the logFile -- if NULL, then logFile is stderr */
	JavaObject **args;      /* Command-line arguments to the class */
	const char **argStrings;
	int32 nArgs;            /* Number of command-line arguments to the class */
	const char *libNames[10]; /* Canonical names of libraries to be pre-loaded */
	int numLibs;            /* Number of libraries to be pre-loaded */
	bool debug;				/* Enable debugging */

	bool catchHardwareExceptions; /* Catch hardware exceptions and asserts */

	DebugDesc *compileBreakPoints;  /* Descriptions of compile break points */
	Uint32 nCompileBreakPoints;     /* Number of compile break points */
	Uint32 nCompileBreakSlots;  /* Number of elements currently allocated in compileBreakPoints */

	DebugDesc *execBreakPoints; /* Descriptions of exec break points */
	Uint32 nExecBreakPoints; /* Number of exec break points */
	Uint32 nExecBreakSlots;  /* Number of elements currently allocated in execBreakPoints */
	Options();
	
    ~Options();

	bool parse(int argc, const char **argv);
	void printHelp() {}
	
    private:
	void setBreakPoints(Uint32 &nBreakPoints, Uint32 &nBreakSlots,
						DebugDesc *&breakPoints, const char *argv[]);
};

#ifdef PR_LOGGING
static void printAvailableModules()
{
	LogModuleObject *module;
	int i;
    
	PR_fprintf(PR_STDERR, "Here is the current list of log modules:\n\n");
    
    i = 1;
    for (module = LogModuleObject::getAllLogModules(); module; module = module->getNext())
        PR_fprintf(PR_STDERR, "%2d. %s\n", i++, module->getName());
}
#endif

inline Options::Options():
	stage(csGenInstructions),
	compileAll(false),
	verbose(false),
	initialize(false),
	invokeMethod(true),
	emitHTML(0),
    traceAllMethods(false),
    methodName(0),
    methodSig(0),
	className(0),
	classPath(0),
	logFileName(0),
    args(0),
    argStrings(0),
	nArgs(0),
	numLibs(0),
	debug(false),
	catchHardwareExceptions(false),
	compileBreakPoints(0),
	nCompileBreakPoints(0),
	nCompileBreakSlots(0),
	execBreakPoints(0),
	nExecBreakPoints(0),
	nExecBreakSlots(0)
{}


inline Options::~Options() 
{
    if (args) { 
        delete [] args; 
        delete [] argStrings;
    }
    
    if (compileBreakPoints)
        delete [] compileBreakPoints;
    
    if (execBreakPoints)
        delete [] execBreakPoints;

	if (className)
		VM::freeUtfClassName((char *) className);
}


#ifdef DEBUG
void Options::setBreakPoints(Uint32 &nBreakPoints, Uint32 &nBreakSlots,
							 DebugDesc *&breakPoints, const char *argv[])
{
    if (nBreakPoints+1 > nBreakSlots) {
        DebugDesc *temp = breakPoints;
        
        breakPoints = new DebugDesc[nBreakSlots += 10];
        
        for (Uint32 index = 0; index < nBreakSlots-10; index++)
            breakPoints[index] = temp[index];	
        
    }
    
    breakPoints[nBreakPoints].className = *++argv;
    breakPoints[nBreakPoints].methodName = *++argv;
    breakPoints[nBreakPoints].sig = *++argv;
    nBreakPoints++;
}
#endif


bool Options::parse(int argc, const char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!PL_strcmp(argv[i], "-a") || !PL_strcmp(argv[i], "-all")) {
            compileAll = true;
            invokeMethod = false;
            methodName = 0;
            methodSig = 0;

#ifdef DEBUG
        } else if (!PL_strcmp(argv[i], "-bc") || !PL_strcmp(argv[i], "-breakCompile")) {	  
            setBreakPoints(nCompileBreakPoints, nCompileBreakSlots, 
                compileBreakPoints, &argv[i]);
            i += 3;
        } else if (!PL_strcmp(argv[i], "-be") || !PL_strcmp(argv[i], "-breakExec")) {	  
            setBreakPoints(nExecBreakPoints, nExecBreakSlots, 
                execBreakPoints, &argv[i]);
            i += 3;
#endif

        } else if (!PL_strcmp(argv[i], "-c") || !PL_strcmp(argv[i], "-classpath")) {
            classPath = argv[++i];
        } else if (!PL_strcmp(argv[i], "-h") || !PL_strcmp(argv[i], "-help")) {
            printHelp();
        } else if (!PL_strcmp(argv[i], "-l") || !PL_strcmp(argv[i], "-lib")) {
            libNames[numLibs++] = argv[++i];
        } else if (!PL_strcmp(argv[i], "-m") || !PL_strcmp(argv[i], "-method")) {
            compileAll = false;
            methodName = argv[++i];
            methodSig = argv[++i];
        } else if (!PL_strcmp(argv[i], "-mn") || !PL_strcmp(argv[i], "-methodName")) {
            compileAll = false;
            methodName = argv[++i];
            methodSig = 0;
        } else if (!PL_strcmp(argv[i], "-n") || !PL_strcmp(argv[i], "-noinvoke")) {
            invokeMethod = false;
        } else if (!PL_strcmp(argv[i], "-html")) {
            emitHTML = true;
        } else if (!PL_strcmp(argv[i], "-v") || !PL_strcmp(argv[i], "-verbose")) {
            verbose = true;
        } else if (!PL_strcmp(argv[i], "-s") || !PL_strcmp(argv[i], "-stage")) {
            const char *stageName = argv[++i];
            if (!getStage(stageName, stage)) {
                PR_fprintf(PR_STDERR, "%s: Bad stage argument %s\n", argv[0],
                    stageName);
                return false;
            }
            
            if (stage != csGenInstructions)
                invokeMethod = false;
        } else if (!PL_strcmp(argv[i], "-sys") || !PL_strcmp(argv[i], "-system")) {
            initialize = true;
        } else if (!PL_strcmp(argv[i], "-ta") || !PL_strcmp(argv[i], "-traceAll")) {
            traceAllMethods = true;
        } else if (!PL_strcmp(argv[i], "-log")) {
#ifdef PR_LOGGING
			/* Get the names of the log module, level and filename */
			const char *logModuleName = argv[++i];
			PRLogModuleLevel logLevel = (PRLogModuleLevel) atoi(argv[++i]);
			
			/* Identify this module */
            LogModuleObject *module = NULL;
            for (module = LogModuleObject::getAllLogModules(); module; module = module->getNext()) {
				if (!PL_strcmp(logModuleName, module->getName())) {
					module->setLogLevel(logLevel);

					if (logFileName)
						module->setLogFile(logFileName);

					break;
				}
            }
            if (!module) {
                fprintf(stderr, "Incorrect module name \"%s\" in -log option.\n", logModuleName);
                printAvailableModules();
                return false;
            }
#else
			i += 2;
#endif
		} else if (!PL_strcmp(argv[i], "-lf") || !PL_strcmp(argv[i], "-logFile")) {
			if (!PL_strcmp(argv[++i], "stderr"))
				logFileName = 0;
			else
				logFileName = argv[i];
		} else if (!PL_strcmp(argv[i], "-ln") || !PL_strcmp(argv[i], "-logNames")) {
#ifdef PR_LOGGING
			printAvailableModules(); 
			return false;
#else
			PR_fprintf(PR_STDERR, "This copy of sajava does not support logging\n");
			return false;
#endif
		} else if (!PL_strcmp(argv[i], "-debug"))
			debug = true;
		else if (!PL_strcmp(argv[i], "-ce") || !PL_strcmp(argv[i], "-catchHardwareExceptions"))
			catchHardwareExceptions = true;
		else if (argv[i][0] == '-') {
			PR_fprintf(PR_STDERR, "bad option %s\n", argv[i]);
			return false;
		} else { /* Must be a class name */
            className = VM::getUtfClassName(argv[i++]);
#ifdef DEBUG_laurentm
			for (Uint32 l = 0; l < strlen(className); l++)
				if (className[l] == '.')
					className[l] = '/';
#endif            
            if ((nArgs = argc-i) > 0) {
                args = new JavaObject *[nArgs];
                argStrings = new const char *[nArgs];
                
                for (int j = 0; i < argc; i++, j++) 
                    argStrings[j] = argv[i];
                
            } else
                args = 0;
            
            break;
        } 
    }
    
    return true;
}

//------------------------------------------------------------------------------

#ifdef DEBUG_WALDEMAR
 #define PREPEND_DEFAULT_OPTIONS
 static const char *const defaultOptions[] = {
	"-stage", "o",
	"-all",
	"-verbose",
	"-noinvoke",
	"-classpath", "/Full-Stutter/Java/java.jar:/Full-Stutter/Java/Tiny/Java Classes",
	"-log", "FieldOrMethod", "3"};
#endif


#ifdef PREPEND_DEFAULT_OPTIONS
//
// Prepend user-specific default options from the defaultOptions array.
//
static void prependDefaultOptions(int &argc, const char **&argv)
{
	int nDefaultOptions = sizeof(defaultOptions) / sizeof(const char *);
	if (nDefaultOptions) {
		int newArgC = argc + nDefaultOptions;
		const char **newArgV = new const char *[newArgC];
		const char **src = argv;
		const char **dst = newArgV;
		*dst++ = *src++;
		copy(defaultOptions, defaultOptions+nDefaultOptions, dst);
		dst += nDefaultOptions;
		copy(src, src+argc-1, dst);
		argc = newArgC;
		argv = newArgV;
	}
}
#endif

// We need to actually create a globally
// scoped thread before we use main, because on some platforms
// (NT) NSPR implicit initialization causes us to be on a fiber
// which means that if we mistakenly call any Win32 functions
// that call WaitForSingleObject, etc we end up hanging.
//
// The NSPR people say they will make this happen automagically
// (I won't hold my breath!) =)

#define MAIN_WRAPPER_RETURN(inWrapper, inRv)	\
	PR_BEGIN_MACRO								\
	inWrapper->rv = inRv;						\
	return;										\
	PR_END_MACRO

struct MainWrapper
{
	int				argc;	// count of args
	const char**	argv;	// vector of actual strings
	int				rv;		// return value from main
};

int main(int argc, const char **argv)
{
	MainWrapper		mainWrapper = {argc, argv, -1};
	PRThread*		thread;

	thread = PR_CreateThread(PR_USER_THREAD,
							 &realMain,
							 &mainWrapper,
							 PR_PRIORITY_NORMAL,
							 PR_GLOBAL_THREAD,
							 PR_JOINABLE_THREAD,
							 0);

	if (thread)
		PR_JoinThread(thread);
	else
		trespass("could not create main thread");
	PR_Cleanup(); 
	return mainWrapper.rv;
}

void realMain(void* arg)
{
	MainWrapper*	mainWrapper = (MainWrapper*) arg;
	int				argc = mainWrapper->argc;
	const char**	argv = mainWrapper->argv;

#ifdef XP_MAC
    argc = ccommand((char ***)&argv);
#endif
#ifdef PREPEND_DEFAULT_OPTIONS
	prependDefaultOptions(argc, argv);
#endif
	Options options;
    
    if (!options.parse(argc, argv))
        MAIN_WRAPPER_RETURN(mainWrapper, 1);
    
	if (options.debug)
		VM::debugger.enable();

    if (options.classPath)
        VM::setClassPath(options.classPath);
    
    if (!options.className) {
        PR_fprintf(PR_STDERR, "%s: Error: Class name expected.\n", argv[0]);
        MAIN_WRAPPER_RETURN(mainWrapper, 1);
    }
    
    if (options.compileAll && options.methodName) {
        PR_fprintf(PR_STDERR, "%s: Error: Cannot combine -all with -method.\n", 
            argv[0]);
        MAIN_WRAPPER_RETURN(mainWrapper, 1);
    }
    
    try {
#ifdef DEBUG
        VM::theVM.setCompileBreakPoints(options.compileBreakPoints, options.nCompileBreakPoints);
        VM::theVM.setExecBreakPoints(options.execBreakPoints, options.nExecBreakPoints);
        VM::theVM.setInhibitBackpatching(options.traceAllMethods);
        VM::theVM.setEmitHTML(options.emitHTML);
#endif
        VM::theVM.setNoInvoke(!options.invokeMethod);
        VM::theVM.setCompileStage(options.stage);
        VM::theVM.setVerbose(options.verbose);
		VM::theVM.setCatchHardwareExceptions(options.catchHardwareExceptions);

        VM::staticInit(options.initialize);
        
#ifdef DEBUG
        // Don't bother tracing system staticInit
        VM::theVM.setTraceAllMethods(options.traceAllMethods);
#endif
    } catch (VerifyError err) {
        printf("Error initializing VM: %d\n", err.cause);
        MAIN_WRAPPER_RETURN(mainWrapper, 1);
    }
    
    int32 i;
    /* Load all native libraries specified on the command-line */
    for (i = 0; i < options.numLibs; i++) {
        if (!VM::loadLibrary(options.libNames[i]))
            printf("Warning: Cannot load library %s\n", options.libNames[i]);
    }
    
    /* Intern all command-line arguments and convert into strings */
    for (i = 0; i < options.nArgs; i++)
        options.args[i] = &VM::intern(options.argStrings[i]);
    
    if (options.compileAll) {
        try {
            VM::theVM.compileMethods(options.className);
        } catch (VerifyError err) {
            PR_fprintf(PR_STDERR, "Error loading class/compiling methods: %d\n", 
                err.cause);
            MAIN_WRAPPER_RETURN(mainWrapper, 1);
        }

        MAIN_WRAPPER_RETURN(mainWrapper, 0);
    }
    
    /* If we're here, we wish to execute a certain method */

	// install the platform specific hardware exception handler
    if (true || VM::debugger.getEnabled())	{   // true->false delegates to debugger
#ifdef _WIN32
        // Check if we want to catch the hardware exception.
        if (VM::theVM.getCatchHardwareExceptions()) {
            installHardwareExceptionHandler(win32HardwareThrowExit);
        } else {
            installHardwareExceptionHandler(win32HardwareThrow);
        }
#endif
    }

	// if using the debugger ignore user's command to execute a class
	if (VM::debugger.getEnabled())
	{
		printf("Command sequence is remote\n");
		VM::debugger.waitForDebugger();
	}
	else
	{
		try 
		{
			VM::theVM.execute(options.className, options.methodName, 
				options.methodSig, 
				options.args, options.nArgs);
		} catch (VerifyError err) {
			printf("Error loading class/executing method: %d\n", err.cause);
			MAIN_WRAPPER_RETURN(mainWrapper, 1);
		} 
	}
    
	// remove the platform specific hardware exception handler
	if (true || VM::debugger.getEnabled())
		removeHardwareExceptionHandler();

    MAIN_WRAPPER_RETURN(mainWrapper, 0);
}

#ifdef __GNUC__
// for gcc with -fhandle-exceptions
void terminate() {}
#endif

#if defined(_WIN32) && defined(DEBUG_LOG)
void stack(void *inPc)
{
	stackWalker(inPc);
}
#endif
