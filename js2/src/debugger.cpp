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

    using namespace Interpreter;

    enum ShellCommand {
        ASSEMBLE,
        AMBIGUOUS,
        AMBIGUOUS2,
        CONTINUE,
        DISSASSEMBLE,
        EXIT,
        HELP,
        LET,
        PRINT,
        REGISTER,
        STEP,
        COMMAND_COUNT
    };    

    static const char *shell_cmds[][3] = {
        {"assemble", "    ", "nyi"},
        {"ambiguous", "   ", "Test command for ambiguous command detection"},
        {"ambiguous2", "  ", "Test command for ambiguous command detection"},
        {"continue", "    ", "Continue execution until complete."},
        {"dissassemble", "", "nyi"},
        {"exit", "        ", "nyi"},
        {"help", "        ", "Display this message."},
        {"let", "         ", "Set a debugger environment variable."},
        {"print", "       ", "nyi"},
        {"register", "    ", "Show the value of a single register or all \
registers, or set the value of a single register."},
        {"step", "        ", "Execute the current opcode."},
        {0, 0} /* sentry */
    };

    enum ShellVariable {
        TRACE,
        VARIABLE_COUNT
    };
    
    static const char *shell_vars[][3] = {
        {"trace", "", "(bool) always show opcode before execution."},
        {0, 0} /* sentry */
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


    /**
     * locate the best match for |partial| in the command list |list|.
     * if no matches are found, return |length|, if multiple matches are found,
     * return |length| plus the number of ambiguous matches
     */
    static uint32
    matchElement (const String &partial, const char *list[][3], size_t length)
    {
        uint32 ambig_matches = 0;
        uint32 match = length;
        
        for (uint32 i = 0; i < length ; ++i)
        {
            String possibleMatch (widenCString(list[i][0]));
            if (startsWith(partial, possibleMatch))
            {
                if (partial.size() == possibleMatch.size())
                {
                    /* exact match */
                    ambig_matches = 0;
                    return i;
                }
                else if (match == COMMAND_COUNT) /* no match yet */
                    match = i;
                else
                    ++ambig_matches; /* something already matched, 
                                      * ambiguous command */
            }
            
        }

        if (ambig_matches == 0)
            return match;
        else
            return length + ambig_matches;
        
    }
    
    

    static void
    showHelp(Formatter &out)
    {
        int i;
        
        for (i = 0; shell_cmds[i][0] != 0; i++)
            out << shell_cmds[i][0]  << shell_cmds[i][1] << "\t" <<
                shell_cmds[i][2] << "\n";
    }
    
    static void
    showCurrentOp(Context* cx, Formatter &aOut)
    {
        ICodeModule *iCode = cx->getICode();
        JSValues &registers = cx->getRegisters();
        InstructionIterator pc = cx->getPC();

        printFormat(aOut, "trace [%02u:%04u]: ",
                    iCode->mID, (pc - iCode->its_iCode->begin()));
        Instruction* i = *pc;
        stdOut << *i;
        if (i->op() != BRANCH && i->count() > 0) {
            aOut << " [";
            i->printOperands(stdOut, registers);
            aOut << "]\n";
        } else {
            aOut << '\n';
        }
    }    

    void
    Shell::listen(Context* cx, InterpretStage stage)
    {

        if (mTraceFlag)
            showCurrentOp (cx, mOut);
            
        if (!(mStopMask & stage))
            return;

        static String lastLine(widenCString("help\n"));
        String line;
        LineReader reader(mIn);

        do {            
            ICodeModule *iCode = cx->getICode();
            InstructionIterator pc = cx->getPC();

            stdOut << "jsd [pc:";
            printFormat (stdOut, "%04X", (pc - iCode->its_iCode->begin()));
            stdOut << "]> ";
        
            reader.readLine(line);

            if (line[0] == uni::lf)
                line = lastLine;
            else
                lastLine = line;

        } while (doCommand(cx, line));
    }


    /**
     * lex and execute the debugger command in |source|, return true if the
     * command does not require the script being debugged to continue (eg, ask
     * for more debugger input.)
     */
    bool
    Shell::doCommand (Interpreter::Context *cx, const String &source)
    {
        Lexer lex (mWorld, source, widenCString("debugger console"), 0);
        const String *cmd;
        uint32 match;
        bool rv = true;
        
        const Token &t = lex.get(true);

        if (t.hasKind(Token::identifier))
            cmd = &(t.getIdentifier());
        else
        {
            mErr << "you idiot.\n";
            return true;
        }        

        match = matchElement (*cmd, shell_cmds, (size_t)COMMAND_COUNT);
        
        if (match <= (uint32)COMMAND_COUNT)
            switch ((ShellCommand)match)
            {
                case COMMAND_COUNT:
                    mErr << "Unknown command '" << *cmd << "'.\n";
                    break;

                case AMBIGUOUS:
                case AMBIGUOUS2:
                    mErr << "I pity the foo.\n";
                    break;

                case CONTINUE:
                    mStopMask &= (IS_ALL ^ IS_STEP);
                    rv = false;
                    break;

                case DISSASSEMBLE:
                    mOut << *cx->getICode();
                    break;    

                case HELP:
                    showHelp (mOut);
                    break;

                case PRINT:
                    doPrint (cx, lex);
                    break;
                    
                case STEP:
                    mStopMask |= IS_STEP;
                    rv = false;
                    break;
                    
                case LET:
                    doSetVariable (lex);
                    break;

                default:
                    mErr << "Input '" << *cmd << "' matched unimplemented " <<
                        "command '" << shell_cmds[match][0] << "'.\n";
                    break;
                    
            }
        else
            mErr << "Ambiguous command '" << *cmd << "', " <<
                (match - (uint32)COMMAND_COUNT + 1) << " similar commands.\n";

        return rv;        
    }

    void
    Shell::doSetVariable (Lexer &lex)
    {
        uint32 match;
        const String *varname;        
        const Token *t = &(lex.get(true));

        if (t->hasKind(Token::identifier))
            varname = &(t->getIdentifier());
        else
        {
            mErr << "invalid variable name.\n";
            return;
        }        

        match = matchElement (*varname, shell_vars, (size_t)VARIABLE_COUNT);

        if (match <= (uint32)VARIABLE_COUNT)
            switch ((ShellVariable)match)
            {
                case VARIABLE_COUNT:
                    mErr << "Unknown variable '" << *varname << "'.\n";
                    break;
                    
                case TRACE:
                    t = &(lex.get(true));
                    if (t->hasKind(Token::assignment))
                        t = &(lex.get(true)); /* optional = */

                    if (t->hasKind(Token::True))
                        mTraceFlag = true;
                    else if (t->hasKind(Token::False))
                        mTraceFlag = false;
                    else
                        goto badval;
                    break;

                default:
                    mErr << "Variable '" << *varname <<
                        "' matched unimplemented variable '" << 
                        shell_vars[match][0] << "'.\n";
            }
        else
            mErr << "Ambiguous variable '" << *varname << "', " <<
                (match - (uint32)COMMAND_COUNT + 1) << " similar variables.\n";
        
        return;
        
    badval:
        mErr << "Invalid value for variable '" <<
            shell_vars[(ShellVariable)match][0] << "'\n";
        

    }
    
    void
    Shell::doPrint (Context *cx, Lexer &lex)
    {
        const Token *t = &(lex.get(true));

        if (!(t->hasKind(Token::identifier)))
        {
            mErr << "Invalid register name.\n";
            return;
        }

/*
        const StringAtom *name = &(t->getIdentifier());
        
        VariableMap::iterator i = ((cx->getICode())->itsVariables)->find(*name);
//        if (i)
            mOut << (*i).first << " = " << (*i).second << "\n";
//        else
//            mOut << "No " << *name << " defined.\n";
  
*/      
    }
    
            
            
    
} /* namespace Debugger */
} /* namespace JavaScript */

                    
            
        
