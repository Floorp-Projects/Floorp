/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

#include "CommandLineUtils.h"

void CommandLineUtils::getOptions
    (NamedMap& options, int argc, char** argv, StringList& flags)
{
        String arg;
        String flag;
        for (int i = 0; i < argc; i++) {
            arg.Truncate();
            arg.getNSString().AppendWithConversion(argv[i]);

            if (!arg.IsEmpty() && (arg.CharAt(0) == '-')) {

                // clean up previous flag
                if (!flag.IsEmpty()) {
                    options.put(flag, new String(arg));
                    flag.Truncate();
                }
                // get next flag
                arg.subString(1,flag);

                //-- check full flag, otherwise try to find
                //-- flag within string
                if (!flags.contains(flag)) {
                    PRUint32 idx = 1;
                    String tmpFlag;
                    while(idx <= flag.Length()) {
                        flag.subString(0,idx, tmpFlag);
                        if (flags.contains(tmpFlag)) {
                            if (idx < flag.Length()) {
                                String* value = new String();
                                flag.subString(idx, *value);
                                options.put(tmpFlag,value);
                                break;
                            }
                        }
                        else if (idx == flag.Length()) {
                            cout << "invalid option: -" << flag << endl;
                        }
                        ++idx;
                    }// end while
                }
            }// if flag char '-'
            else {
                // Store both flag key and number key
                if (!flag.IsEmpty())
                    options.put(flag, new String(arg));
                flag.Truncate();
            }

        }// end for
        if (!flag.IsEmpty())
            options.put(flag, new String(NS_LITERAL_STRING("no value")));
} //-- getOptions

