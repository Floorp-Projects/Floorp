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

    /* keep in sync with list in debugger.h */
    static const char *shell_cmds[][3] = {
        {"assemble", "", 0},
        {"ambiguous", "", "Test command for ambiguous command detection"},
        {"ambiguous2", "", "Test command for ambiguous command detection"},
        {"continue", "", "Continue execution until complete."},
        {"dissassemble", "[start_pc] [end_pc]", "Dissassemble entire module, or subset of module."},
        {"exit", "", 0},
        {"help", "", "Display this message."},
        {"istep", "", "Execute the current opcode and stop."},
        {"let", "", "Set a debugger environment variable."},
        {"print", "", 0},
        {"register", "", "(nyi) Show the value of a single register or all registers, or set the value of a single register."},
        {"step", "", "Execute the current JS statement and stop."},
        {0, 0} /* sentry */
    };

    enum ShellVariable {
        TRACE_SOURCE,
        TRACE_ICODE,
        VARIABLE_COUNT
    };
    
    static const char *shell_vars[][3] = {
        {"tracesource", "", "(bool) Show JS source while executing."},
        {"traceicode", " ", "(bool) Show opcodes while executing."},
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
        
        out << "JavaScript 2.0 Debugger Help...\n\n";
        
        for (i = 0; shell_cmds[i][0] != 0; i++)
        {
            out << "Command : " << shell_cmds[i][0] << " " << 
                shell_cmds[i][1] << "\n";

            if (shell_cmds[i][2])
                out << "Help    : " << shell_cmds[i][2] << "\n";
            else
                out << "Help    : (probably) Not Implemented.\n";
        }
    }

    static uint32
    getClosestSourcePosForPC (Context *cx, InstructionIterator pc)
    {
        ICodeModule *iCode = cx->getICode();
        
        if (iCode->mInstructionMap->begin() == iCode->mInstructionMap->end())
            return NotABanana;
            /*NOT_REACHED ("Instruction map is empty, waah.");*/

        InstructionMap::iterator pos_iter =
            iCode->mInstructionMap->upper_bound (static_cast<uint32>(pc - iCode->its_iCode->begin()));
        if (pos_iter != iCode->mInstructionMap->begin())
            --pos_iter;
    
        return pos_iter->second;
    }
        
    void
    Shell::showSourceAtPC (Context *cx, InstructionIterator pc)
    {
        if (!mResolveFileCallback)
        {
            mErr << "Source not available (Debugger was improperly initialized.)\n";
            return;
        }

        ICodeModule *iCode = cx->getICode();

        String fn = iCode->getFileName();
        const Reader *reader = mResolveFileCallback(fn);
        if (!reader)
        {
            mErr << "Source not available.\n";
            return;
        }
        
        uint32 pos = getClosestSourcePosForPC(cx, pc);
        if (pos == NotABanana)
        {
            mErr << "Map is empty, cannot display source.\n";
            return;
        }
            
        uint32 lineNum = reader->posToLineNum (pos);
        const char16 *lineBegin, *lineEnd;

        uint32 lineStartPos = reader->getLine (lineNum, lineBegin, lineEnd);
        String sourceLine (lineBegin, lineEnd);
        
        mOut << fn << ":" << lineNum << " " << sourceLine << "\n";

        uint padding = fn.length() + (uint32)(lineNum / 10) + 3;
        uint i;        

        for (i = 0; i < padding; i++)
            mOut << " ";
        
        padding = (pos - lineStartPos);
        for (i = 0; i < padding; i++)
            mOut << ".";
        
        mOut << "^\n";
        
    }

    void
    Shell::showOpAtPC(Context* cx, InstructionIterator pc)
    {
        ICodeModule *iCode = cx->getICode();

        if ((pc < iCode->its_iCode->begin()) ||
            (pc >= iCode->its_iCode->end()))
        {
            mErr << "PC Out Of Range.";
            return;
        }

        JSValues &registers = cx->getRegisters();

        printFormat(mOut, "trace [%02u:%04u]: ",
                    iCode->mID, (pc - iCode->its_iCode->begin()));
        Instruction* i = *pc;
        stdOut << *i;
        if (i->op() != BRANCH && i->count() > 0) {
            mOut << " [";
            i->printOperands(stdOut, registers);
            mOut << "]\n";
        } else {
            mOut << '\n';
        }
    }    

    void
    Shell::listen(Context* cx, Context::Event event)
    {
        InstructionIterator pc = cx->getPC();
        
        if (mTraceSource)
            showSourceAtPC (cx, pc);
        if (mTraceICode)
            showOpAtPC (cx, pc);
            
        if (!(mStopMask & event))
            return;

        if ((mLastCommand == STEP) && (mLastICodeID == cx->getICode()->mID) &&
            (mLastSourcePos == getClosestSourcePosForPC (cx, cx->getPC())))
            /* we're in source-step mode, and the source position hasn't
             * changed yet */
            return;

        if (!mTraceSource && !mTraceICode)
            showSourceAtPC (cx, pc);
                
        static String lastLine(widenCString("help\n"));
        String line;
        LineReader reader(mIn);

        do {
            stdOut << "jsd";
            if (mLastCommand != COMMAND_COUNT)
                stdOut << " (" << shell_cmds[mLastCommand][0] << ") ";
            stdOut << "> ";
        
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
        {
            switch ((ShellCommand)match)
            {
                case COMMAND_COUNT:
                    mErr << "Unknown command '" << *cmd << "'.\n";
                    break;

                case AMBIGUOUS:
                case AMBIGUOUS2:
                    mErr << "I pity the foogoo.\n";
                    break;

                case CONTINUE:
                    mStopMask &= (Context::EV_ALL ^ Context::EV_STEP);
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
                    mStopMask |= Context::EV_STEP;
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

            mLastSourcePos = getClosestSourcePosForPC (cx, cx->getPC());
            mLastICodeID = cx->getICode()->mID;
            mLastCommand = (ShellCommand)match;

        } else
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
                    
                case TRACE_SOURCE:
                    t = &(lex.get(true));
                    if (t->hasKind(Token::assignment))
                        t = &(lex.get(true)); /* optional = */

                    if (t->hasKind(Token::True))
                        mTraceSource = true;
                    else if (t->hasKind(Token::False))
                        mTraceSource = false;
                    else
                        goto badval;
                    break;

                case TRACE_ICODE:
                    t = &(lex.get(true));
                    if (t->hasKind(Token::assignment))
                        t = &(lex.get(true)); /* optional = */

                    if (t->hasKind(Token::True))
                        mTraceICode = true;
                    else if (t->hasKind(Token::False))
                        mTraceICode = false;
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
    Shell::doPrint (Context *, Lexer &lex)
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

                    
            
        
