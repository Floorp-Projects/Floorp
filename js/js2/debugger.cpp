/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#include "world.h"
#include "debugger.h"

#include <string>
#include <ctype.h>
#include <assert.h>

namespace JavaScript {
namespace Debugger {

    enum ShellCommand {
        ASSEMBLE,
        DISSASSEMBLE,
        STEP,
        PRINT,
        PRINT2,
        EXIT,
        COMMAND_COUNT
    };    

    static const char *shell_cmd_names[] = {
        "assemble",
        "dissassemble",
        "step",
        "print",
        "print2",
        "exit",
        0
    };    

    /* return true if str2 starts with/is str1
     * XXX ignore case */
    static bool
    startsWith (const String &str1, const String &str2)
    {
        uint n;
        size_t m = str1.size();

        if (m > str2.size())
            return false;

        for (n = 0; n < m; ++n)
            if (str1[n] != str2[n])
                return false;
        
        return true;
    }

    bool
    Shell::doCommand (Interpreter::Context * /*context*/, const String &source)
    {
        Lexer lex (mWorld, source, widenCString("debugger console"), 0);
        const String *cmd;
        ShellCommand match = COMMAND_COUNT;
        uint32 ambig_matches = 0;
        
        const Token &t = lex.get(true);

        if (t.hasKind(Token::identifier))
            cmd = &(t.getIdentifier());
        else
        {
            mErr << "you idiot.\n";
            return false;
        }
        
        for (int i = ASSEMBLE; i < COMMAND_COUNT; ++i)
        {
            String possibleMatch (widenCString(shell_cmd_names[i]));
            if (startsWith(*cmd, possibleMatch))
            {
                if (cmd->size() == possibleMatch.size())
                {
                    /* exact match */
                    ambig_matches = 0;
                    match = (ShellCommand)i;
                    break;
                }
                else if (match == COMMAND_COUNT) /* no match yet */
                    match = (ShellCommand)i;
                else
                    ++ambig_matches; /* something already matched, 
                                      * ambiguous command */
            }
            
        }

        if (ambig_matches == 0)
            switch (match)
            {
                case COMMAND_COUNT:
                    mErr << "Unknown command '" << *cmd << "'.\n";
                    break;
                    
                case ASSEMBLE:
                case DISSASSEMBLE:
                case STEP:
                case PRINT:
                default:
                    mErr << "Input '" << *cmd << "' matched command '" << 
                        shell_cmd_names[match] << "'.\n";
                    break;
                    
            }
        else
            mErr << "Ambiguous command '" << *cmd << "', " <<
                (ambig_matches + 1) << " similar commands.\n";

        return  (ambig_matches == 0);
        
    }

} /* namespace Debugger */
} /* namespace JavaScript */

                    
            
        
