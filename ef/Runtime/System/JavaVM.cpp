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
#include "JavaVM.h"
#include "prprf.h"
#include "plstr.h"
#include "LogModule.h"
#include "Debugger.h"
#ifdef USE_JVMDI
#include "jvmdi.h"
#endif
#include "Thread.h"
#include "JniRuntime.h"

Pool envPool;
ClassWorld world(envPool);
StringPool sp(envPool);
Pool debuggerPool;

UT_DEFINE_LOG_MODULE(HiMom);
UT_DEFINE_LOG_MODULE(VM);

VM VM::theVM;

/* Initialize the VM. If initialize is true, attempts to run 
 * the static initializers to initialize the System class on
 * start-up.
 */
void VM::staticInit(bool initialize)
{
    NativeMethodDispatcher::staticInit();
    InterfaceDispatchTable::staticInit(envPool);
    Standard::initStandardObjects(central);
    JavaString::staticInit();
    JNIInitialize();

    /* Intern a few strings which we'll need */
    /* FIX-ME Why do we need them??? */
    sp.intern("<clinit>");
    sp.intern("()V");
    
    if (initialize) {
	bool loadedSysLibrary;  
	
	Monitor::staticInit();
	
	/* Load package library */
	if (!(loadedSysLibrary = (NativeMethodDispatcher::loadLibrary("Package") != 0)))
	    UT_LOG(VM, PR_LOG_ALWAYS, ("\tCannot load package library!\n"));
    
	if (loadedSysLibrary) {
	    /* execute System::initializeSystemClass() to get us up and going */
	    theVM.execute("java/lang/System", "initializeSystemClass", "()V", 0, 0);
	}
    
	Thread::staticInit();
    }
}

/* Set the class path used to search for class files */
void VM::setClassPath(const char *classPath) 
{
    central.setClassPath(classPath);
}

/* Load a native library. libName is the canonical name of the library.
 * This routine returns true on success, false on failure.
 */
bool VM::loadLibrary(const char *libName) 
{
	return (NativeMethodDispatcher::loadLibrary(libName) != 0);
}

const Class &VM::getStandardClass(StandardClass clz)
{
	return (Standard::get(clz));
}

/* Execute the method whose simple name is given by methodName and signature
 * string is given by signature in the class given by className.
 * If the method is not overloaded, signature can be nil.
 * If invokeMethod is false, then just compile the method but do not 
 * execute it.
 */
void VM::execute(const char *className,
				 const char *methodName,
				 const char *signature,
				 JavaObject *args[],
				 Int32 nArgs)
{
	ClassFileSummary *cfs = &central.addClass(className);

	if (compileStage < csPreprocess)
		return;

	/* Search for specified method */
	Method *method;
  
	if (!methodName) {
		methodName = sp.get("main");
		signature = sp.get("([Ljava/lang/String;)V");

		if (!methodName || !signature)
			return;
	} else {
		methodName = sp.get(methodName);
		if (signature)
			signature = sp.get(signature);
	}
  
	if ((method = cfs->getMethod(methodName, signature)) != 0) {
		// MethodDescriptor descriptor(*method);

		void *code;
		code = method->compile();

#ifdef USE_JVMDI
		if (debugger.getEnabled())
			JVMDI_SetBreakpoint((JNIEnv *) code, 0, 0, 0);
#endif

		if (!noInvoke) {
			/* You can only jump to a static method */
			if ((method->getModifiers() & CR_METHOD_STATIC)) {
				if (!PL_strcmp(methodName, "main")) {
					const Type *type = (const Class *) central.addClass("java/lang/String").getThisClass();
					const Array &typeOfArray = type->getArrayType();
					void *mem = malloc(arrayObjectEltsOffset + getTypeKindSize(tkObject)*nArgs);
					JavaArray *argsArray = new (mem) JavaArray(typeOfArray, nArgs);

					/* Copy args into the array */
					JavaObject **argsInArray = (JavaObject **) ((char *) mem+arrayObjectEltsOffset);
					for (int32 i = 0; i < nArgs; i++)
						argsInArray[i] = args[i];
	  
					args = new JavaObject*[1];
					args[0] = argsArray;
					nArgs = 1;
				} 
				//method->invoke(0, args, nArgs);
				Thread::invoke(method,NULL,args,nArgs);

			} else
				UT_LOG(VM, PR_LOG_ALWAYS, ("specified method is not static; cannot execute\n"));		  
		}

	} else
		UT_LOG(VM, PR_LOG_ALWAYS, ("Could not find method %s.\n", methodName));

}

ClassCentral 	VM::central(envPool, world, sp, 0);
DebuggerState	VM::debugger(debuggerPool);

/*
 * Compile and dump the methods in the file read into cfs.  
 * Run the compiler's stages
 * up to and including the given stage.  Use the envPool for temporary 
 * storage.
 */
void VM::compileMethods(const char *className)
{
	ClassFileSummary &cfs = central.addClass(className);
  
	uint nMethods = cfs.getMethodCount();
	const Method **methods = cfs.getMethods();
  
	for (uint16 i =0; i < nMethods; i++) {
		Method *method = const_cast<Method *>(methods[i]);

		method->compile();
	}
}


/* Intern a string, expected to be in UTF8 format, and return a reference 
 * to the interned string object.
 */
JavaString &VM::intern(const char *str)
{
	const char *internedp = sp.intern(str);
	return *sp.getStringObj(internedp);  
}

#ifdef DEBUG

/* Copy breakPointsIn into breakpointsOut, allocating memory as neccessary */
inline void VM::setBreakPoints(DebugDesc *breakPointsIn, Uint32 numBreakPointsIn,
							   DebugDesc *&breakPointsOut, Uint32 &numBreakPointsOut)
{
	if (breakPointsOut)
		delete [] breakPointsOut;

	breakPointsOut = new DebugDesc[(numBreakPointsOut = numBreakPointsIn)];

	for (Uint32 i = 0; i < numBreakPointsIn; i++) {
		/* Replace slashes with dots, since the fully qualified class name is stored with dots
		 * as the separators
		 */
		for (char *cnameTemp = const_cast<char *>(breakPointsIn[i].className); *cnameTemp; cnameTemp++)
			if (*cnameTemp == '/') *cnameTemp = '.';

		breakPointsOut[i].className = sp.intern(breakPointsIn[i].className);
		breakPointsOut[i].methodName = sp.intern(breakPointsIn[i].methodName);

		/* Replace all dots in the signature with slashes */
		for (char *bTemp = const_cast<char *>(breakPointsIn[i].sig); *bTemp; 
		     bTemp++)
		    if (*bTemp == '.') *bTemp = '/';

		breakPointsOut[i].sig = sp.intern(breakPointsIn[i].sig);    
	}

}


/* Specify a list of methods to break into when compiling the method */  
void VM::setCompileBreakPoints(DebugDesc *breakPoints, Uint32 numBreakPoints) 
{
	setBreakPoints(breakPoints, numBreakPoints, compileBreakPoints, nCompileBreakPoints);
}

/* Specify a list of methods to break into when running the method */
void VM::setExecBreakPoints(DebugDesc *breakPoints, Uint32 numBreakPoints) 
{
	setBreakPoints(breakPoints, numBreakPoints, execBreakPoints, nExecBreakPoints);
}

#endif

/* Construct and return a fully qualified class-name
 * from the input name, which is a fully qualified
 * java class name.
 * getUtfClassName("java.lang.String") = "java/lang/String"
 */
char *VM::getUtfClassName(const char *name)
{
  char *utfClassName = PL_strdup(name);

  for (char *t = utfClassName; *t; t++)
    if (*t == '.') *t = '/';

  return utfClassName;
}

/* Free the name returned by getUtfClassName() */
void VM::freeUtfClassName(char *name)
{
  free(name);
}


VM::~VM()
{
	if (execBreakPoints)
		delete [] execBreakPoints;
  
	if (compileBreakPoints)
		delete [] compileBreakPoints;
}

