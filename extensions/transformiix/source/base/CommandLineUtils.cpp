/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

#include "CommandLineUtils.h"

/**
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
void CommandLineUtils::getOptions
    (NamedMap& options, int argc, char** argv, StringList& flags)
{
        String arg;
        String flag;
        for (int i = 0; i < argc; i++) {
            arg.clear();
            arg.append(argv[i]);

            if ((arg.length()>0) && (arg.charAt(0) == '-')) {

	            // clean up previous flag
                if (flag.length()>0) {
                    options.put(flag, new String(arg));
                    flag.clear();
	            }
	            // get next flag
                arg.subString(1,flag);

	            //-- check full flag, otherwise try to find
	            //-- flag within string
	            if (!flags.contains(flag)) {
                    Int32 idx = 1;
                    String tmpFlag;
	                while(idx <= flag.length()) {
                        flag.subString(0,idx, tmpFlag);
                        if (flags.contains(tmpFlag)) {
	                        if (idx < flag.length()) {
                                String* value = new String();
                                flag.subString(idx, *value);
                                options.put(tmpFlag,value);
	                            break;
	                        }
	                    }
	                    else if (idx == flag.length()) {
                            cout << "invalid option: -" << flag << endl;
	                    }
	                    ++idx;
	                }// end while
	            }
            }// if flag char '-'
	        else {
	            // Store both flag key and number key
                if (flag.length() > 0) options.put(flag, new String(arg));
                flag.clear();
	        }

	    }// end for
        if (flag.length()>0) options.put(flag, new String("no value"));
} //-- getOptions

