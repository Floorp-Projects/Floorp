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
#include "NetscapeHeaderGenerator.h"
#include "JNIHeaderGenerator.h"
#include "JavaVM.h"

#include "prprf.h"
#include "plstr.h"


/* argv[1] -- fully qualified className */

void printUsage()
{
  PR_fprintf(PR_STDERR, "Usage: javah [-v] [-options] classes...\n");
  PR_fprintf(PR_STDERR, "\n");
  PR_fprintf(PR_STDERR, "where options include:\n");

  PR_fprintf(PR_STDERR, "    -help      print out this message\n");
  PR_fprintf(PR_STDERR, "    -o         specify the output file name\n");
  PR_fprintf(PR_STDERR, "    -d         specify the output directory\n");
  PR_fprintf(PR_STDERR, "    -jni       create a JNI-style header file\n"); 
  PR_fprintf(PR_STDERR, "    -ns        create Netscape-style header files (default)\n");
  PR_fprintf(PR_STDERR, "    -td        specify the temporary directory\n");
  PR_fprintf(PR_STDERR, "    -trace     adding tracing information to stubs file\n");
  PR_fprintf(PR_STDERR, "    -v         verbose operation\n");
  PR_fprintf(PR_STDERR, "    -classpath <directories separated by colons>\n");
  PR_fprintf(PR_STDERR, "    -version   print out the build version\n");
}

enum HeaderMode {
  headerModeInvalid = 0,
  headerModeJNI,
  headerModeNetscape,
  headerModeJRI
};


/* Parse command-line options and return an initialized HeaderGenerator
 * class. On success, set className to the className to generate headers
 * for.
 */
HeaderGenerator *parseOptions(int argc, char **argv, ClassCentral &central,
			      char **&classNames, int32 &numClasses)
{
  struct Options {
    HeaderGenerator *generator;
    bool verbose;
    const char *classPath;
    const char *tempDir;
    const char *outputDir;
    HeaderMode mode;

    Options() : generator(0), verbose(false), classPath(0), tempDir(0), 
      outputDir(0), mode(headerModeNetscape) { }
  };

  Options options;

  numClasses = 0;

  if (argc < 1) {
    PR_fprintf(PR_STDERR, "javah: Class name expected\n");
    return 0;
  }
  
  for (int i = 1; i < argc; i++) {
    if (!PL_strcmp(argv[i], "-d"))
      options.outputDir = argv[++i];
    else if (!PL_strcmp(argv[i], "-td"))
      options.tempDir = argv[++i];
    else if (!PL_strcmp(argv[i], "-v"))
      options.verbose = true;
    else if (!PL_strcmp(argv[i], "-h") || !PL_strcmp(argv[i], "-help")) {
      printUsage();
      return 0;
    } else if (!PL_strcmp(argv[i], "-classpath"))
      options.classPath = argv[++i];
    else if (!PL_strcmp(argv[i], "-jni")) 
      options.mode = headerModeJNI;
    else if (!PL_strcmp(argv[i], "-jri"))
      options.mode = headerModeJRI;
    else if (!PL_strcmp(argv[i], "-ns"))
      options.mode = headerModeNetscape;
    else if (!PL_strcmp(argv[i], "-stubs")) {
      PR_fprintf(PR_STDERR,
		 "javah does not support the -stubs option since neither\n"
		 "JNI nor Netscape's native method dispatch require stubs\n");
      return 0;
    } else if (!PL_strcmp(argv[i], "-trace")) {
      PR_fprintf(PR_STDERR,
		 "javah does not support the -trace option since neither\n"
		 "JNI nor Netscape's native method dispatch require stubs\n");
      return 0;
    } else if (*argv[i] == '-') {
      PR_fprintf(PR_STDERR, "javah warning: Unknown option %s\n", argv[i]);
      continue;
    } else {      /* This must be the first of a list of class names */
      numClasses = argc-i;
      classNames = new char *[numClasses];

      for (int index = i, j = 0; index < argc; index++, j++)
	  classNames[j] = VM::getUtfClassName(argv[index]);
      
      break;
    }
    
  }
  
  central.setClassPath(options.classPath);

  VM::staticInit();

  HeaderGenerator *generator = 0;
  switch (options.mode) {
  case headerModeNetscape:
    generator = new NetscapeHeaderGenerator(central);
    break;

  case headerModeJNI:
    generator = new JNIHeaderGenerator(central);
    break;

  default:
    break;
  }

  if (!generator)
    return 0;

  generator->setOutputDir(options.outputDir);
  generator->setTempDir(options.tempDir);

  return generator;
}

int main(int argc, char **argv)
{
  try {
    char **classNames;
    int32 numClasses;

    HeaderGenerator *generator = parseOptions(argc, argv, 
					      VM::getCentral(), 
					      classNames, numClasses);
    
    if (!generator)
      return 1;
    
    for (int32 i = 0; i < numClasses; i++) {
	  VM::theVM.setCompileStage(csRead);
	  VM::theVM.setNoInvoke(true);
	  VM::theVM.setVerbose(false);

      VM::theVM.execute(classNames[i], 0, 0, 0, 0); 
      generator->writeFile(classNames[i]);
    }

  } catch (VerifyError err) {
    PR_fprintf(PR_STDERR, "Error loading class file: %d\n", err.cause);
    return 1;
  }

  return 0;
}

#if defined( __GNUC__ ) && ( __GNUC__ < 2 ) || (__GNUC_MINOR__ < 9)
/* for gcc with -fhandle-exceptions */
void terminate() {}
#endif
