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
#include "BytecodeGraph.h"
#include "ClassCentral.h"
#include "PrimitiveOptimizer.h"
#include "ErrorHandling.h"
#include "FieldOrMethod.h"
#include "NativeCodeCache.h"
#include "DebugUtils.h"
#include "NativeMethodDispatcher.h"
#include "JavaString.h"
#include "Debugger.h"

#include "prprf.h"

/* A structure to specify methods in debug breakpoints */
struct NS_EXTERN DebugDesc {
    const char *className;     /* Fully qualified class name of the class */
    const char *methodName;    /* Simple name of the method */
    const char *sig;           /* Java signature string of the method */
};

class NS_EXTERN VM {
public:
    static VM theVM;
    static DebuggerState debugger;

    VM() : compileBreakPoints(0),
	nCompileBreakPoints(0),
	execBreakPoints(0), 
	nExecBreakPoints(0), 
	compileStage(csGenInstructions), 
	noInvoke(false) {}

    ~VM();

    static void staticInit(bool initialize = false);

    void execute(const char *className, const char *methodName,
		 const char *signature,
		 JavaObject *args[],
		 Int32 nArgs);

    void compileMethods(const char *className);

    static void setClassPath(const char *classPath = 0);

    static ClassCentral &getCentral() { return central; }

    static bool loadLibrary(const char *libName);

    static JavaString &intern(const char *str);

    static const Class &getStandardClass(StandardClass clz);

    static char *getUtfClassName(const char *name);

    static void freeUtfClassName(char *name);

    /* 
     * The following methods have been put in to support temporary debugging
     * and will work only with the debug build of the VM
     */
    void setCompileBreakPoints(DebugDesc *breakPoints, Uint32 numBreakPoints);
  
    /*
     * Get the current list of compile breakpoints. Returns the number of
     * compile breakpoints 
     */ 
    Uint32 getCompileBreakPoints(DebugDesc *&breakPoints)
    { breakPoints = compileBreakPoints; return nCompileBreakPoints; }

    void setExecBreakPoints(DebugDesc *breakPoints, Uint32 numBreakPoints);

    /*
     * Get the current list of execution breakpoints. Returns the number of
     * execution breakpoints 
     */ 
    Uint32 getExecBreakPoints(DebugDesc *&breakPoints) 
    { breakPoints = execBreakPoints; return nExecBreakPoints; }
  
    /* Get and set the compile stage for all methods compiled */
    void setCompileStage(CompileStage stage) { compileStage = stage; }
    CompileStage getCompileStage() { return compileStage; }

    /* get and set the verbosity of compilation */
    void setVerbose(bool ver) { verbose = ver; }
    bool getVerbose() { return verbose; }

    void setNoInvoke(bool noinv) { noInvoke = noinv; }
    bool getNoInvoke() { return noInvoke; }

    void setTraceAllMethods(bool b) { 
	assert(!b || inhibitBackpatching); 
	traceAllMethods = b; 
    }
    bool getTraceAllMethods() { return traceAllMethods; }

    void setInhibitBackpatching(bool b) { inhibitBackpatching = b; }
    bool getInhibitBackpatching() { return inhibitBackpatching; }

    void setEmitHTML(bool emit) { emitHTML = emit; }
    bool getEmitHTML() { return emitHTML; }

    void setCatchHardwareExceptions(bool catchExc) { catchHardwareExceptions = catchExc; }
    bool getCatchHardwareExceptions() { return catchHardwareExceptions; }
    
private:
#ifdef DEBUG
    void setBreakPoints(DebugDesc *breakPointsIn, Uint32 numBreakPointsIn,
			DebugDesc *&breakPointsOut, Uint32 &numBreakPointsOut);
#endif

    static ClassCentral central;

    DebugDesc *compileBreakPoints;
    Uint32 nCompileBreakPoints;
  
    DebugDesc *execBreakPoints;
    Uint32 nExecBreakPoints;

    CompileStage compileStage;
    bool verbose;
    bool noInvoke;				/* If true, do not invoke method */
    bool inhibitBackpatching;	// If true, don't update vtables or
    // call-sites when methods are compiled 
    bool traceAllMethods;
    bool emitHTML;

    bool catchHardwareExceptions;  // If true, catch all hardware exceptions and asserts
};

  

/* UtfClassName is a wrapper around VM::getUtfClassName(); */
class UtfClassName {
public:
    UtfClassName(const char *name) { 
			className = VM::getUtfClassName(name);
    }
    
    ~UtfClassName() { VM::freeUtfClassName(className); }
		operator char * () { return className; }

private:
    char *className; 
};
