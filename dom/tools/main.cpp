/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

// initialize cout
#include "ostream.h"
#include "string.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include "IdlParser.h"
#include "Exceptions.h"
#include "IdlSpecification.h"

int main(int argc, char *argv[])
{
  // extract filenames from argv
  if (argc >= 2) {
    int args = argc - 1;

    // test if an output directory is provided
    if (args > 1) {
      int createDir = 1;

      size_t length = strlen(argv[args]);
      if (length > 4) {
        if (0 == strcmp(argv[args] + length - 4, ".idl")) {
          createDir = 0;
        }
      }

      if (createDir) {
        struct stat sb;
        if (stat(argv[args], &sb) == 0) {
          if (!(sb.st_mode & _S_IFDIR)) {
            cout << "Creating directory " << argv[args] << " ...\n";
            if (mkdir(argv[args]) < 0) {
              cout << "WARNING: cannot create output directory [" << argv[args] << "]\n";
              cout << "++++++++ using current directory\n";
            }
          }
        }

        args--;
      }
    }

    for (int i = 1; i <= args; i++) {

      cout << "************** PARSING " << argv[i] << " **************" << "\n";

      // create a specification object. On parser termination it will
      // contain all parsed interfaces
      IdlSpecification *specification = new IdlSpecification();

      // initialize and run the parser
      IdlParser *parser = new IdlParser();
      try {
        parser->Parse(argv[i], *specification);
      } catch(AbortParser &exc) {
        cout << exc;
        delete parser;
        return -1;
      } catch(...) {
        cout << "Unknown Exception. Parser Aborted.";
      }

      cout << "************** PARSED **************" << "\n" 
           << "************** DUMPING **************" << "\n" 
           << *specification;

      delete parser;
    }

    return 0;
  }

  cout << "ERROR: no file specified\n";
  return -1;
}

