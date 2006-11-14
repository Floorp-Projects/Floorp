/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#include <stdio.h>

#include "avmshell.h"

#ifdef DEBUGGER
namespace avmshell
{
	/**
	 * The array of top level commands that we support.
	 * They are placed into a Nx2 array, whereby the first component
	 * is a String which is the command and the 2nd component is the
	 * integer identifier for the command.
	 *
	 * The StringIntArray object provides a convenient wrapper class
	 * that implements the List interface.
	 *
	 * NOTE: order matters!  For the case of a single character
	 *       match, we let the first hit act like an unambiguous match.
	 */
	DebugCLI::StringIntArray DebugCLI::commandArray[] =
	{
		{ "awatch", CMD_AWATCH },
		{ "break", CMD_BREAK },
		{ "bt", INFO_STACK_CMD },
        { "continue", CMD_CONTINUE },
        { "cf", CMD_CF },
		{ "clear", CMD_CLEAR },
		{ "commands", CMD_COMMANDS },
		{ "condition", CMD_CONDITION },
		{ "delete", CMD_DELETE },
		{ "disable", CMD_DISABLE },
		{ "disassemble", CMD_DISASSEMBLE },
		{ "display", CMD_DISPLAY },
		{ "enable", CMD_ENABLE },
		{ "finish", CMD_FINISH },
		{ "file", CMD_FILE },
		{ "help", CMD_HELP },
		{ "halt", CMD_HALT },
		{ "handle", CMD_HANDLE },
		{ "home", CMD_HOME },
		{ "info", CMD_INFO },
		{ "kill", CMD_KILL },
		{ "list", CMD_LIST },
		{ "next", CMD_NEXT },
		{ "nexti", CMD_NEXT },
		{ "mctree", CMD_MCTREE },
        { "print", CMD_PRINT },
        { "pwd", CMD_PWD },
		{ "quit", CMD_QUIT },
		{ "run", CMD_RUN },
		{ "rwatch", CMD_RWATCH },
		{ "step", CMD_STEP },
		{ "stepi", CMD_STEP },
		{ "set", CMD_SET },
		{ "show", CMD_SHOW },
		{ "source", CMD_SOURCE },
		{ "tutorial", CMD_TUTORIAL },
		{ "undisplay", CMD_UNDISPLAY },
		{ "where", INFO_STACK_CMD },
		{ "watch", CMD_WATCH },
		{ "what", CMD_WHAT },
		{ "viewswf", CMD_VIEW_SWF },
		{ NULL, 0 }
	};

	/**
	 * Info sub-commands
	 */
	DebugCLI::StringIntArray DebugCLI::infoCommandArray[] =
	{
		{ "arguments", INFO_ARGS_CMD },
		{ "breakpoints", INFO_BREAK_CMD },
		{ "display", INFO_DISPLAY_CMD },
		{ "files", INFO_FILES_CMD },
		{ "functions", INFO_FUNCTIONS_CMD },
		{ "handle", INFO_HANDLE_CMD },
		{ "locals", INFO_LOCALS_CMD },
		{ "stack", INFO_STACK_CMD },
        { "sources", INFO_SOURCES_CMD },
        { "swfs", INFO_SWFS_CMD },
        { "targets", INFO_TARGETS_CMD },
		{ "variables", INFO_VARIABLES_CMD },
		{ NULL, 0 }		
	};

	/**
	 * Show sub-commands
	 */
	DebugCLI::StringIntArray DebugCLI::showCommandArray[] =
	{
		{ "break", SHOW_BREAK_CMD },
		{ "files", SHOW_FILES_CMD },
		{ "functions", SHOW_FUNC_CMD },
		{ "memory", SHOW_MEM_CMD },
		{ "net", SHOW_NET_CMD },
		{ "properties", SHOW_PROPERTIES_CMD },
		{ "uri", SHOW_URI_CMD },
		{ "variable", SHOW_VAR_CMD },
		{ NULL, 0 }		
	};

	/**
	 * enable sub-commands
	 */
	DebugCLI::StringIntArray DebugCLI::enableCommandArray[] =
	{
		{ "breakpoints", CMD_BREAK },
		{ "display", CMD_DISPLAY },
		{ "delete", CMD_DELETE },
		{ "once", ENABLE_ONCE_CMD },
		{ NULL, 0 }		
	};

	/**
	 * disable sub-commands
	 */
	DebugCLI::StringIntArray DebugCLI::disableCommandArray[] =
	{
		{ "display", CMD_DISPLAY },
		{ "breakpoints", CMD_BREAK },
		{ NULL, 0 }		
	};
	
	DebugCLI::DebugCLI(AvmCore *core)
		: Debugger(core)
	{
		currentSourceLen = -1;
	}

	DebugCLI::~DebugCLI()
	{
		if (currentSource) {
			delete [] currentSource;
			currentSource = NULL;
		}
	}
		
	char* DebugCLI::nextToken()
	{
		char *out = currentToken;
		if (currentToken) {
			while (*currentToken) {
				if (*currentToken == ' ' || *currentToken == '\r' || *currentToken == '\n' || *currentToken == '\t') {
					*currentToken++ = 0;
					break;
				}
				currentToken++;
			}
			currentToken = *currentToken ? currentToken : NULL;
		}
		return out;
	}

	/**
	 * Attempt to match given the given string against our set of commands
	 * @return the command code that was hit.
	 */
	int DebugCLI::determineCommand(DebugCLI::StringIntArray cmdList[],
								   const char *input,
								   int defCmd)
	{
		int inputLen = strlen(input);
		
		// first check for a comment
		if (input[0] == '#') {
			return CMD_COMMENT;
		}

		int match = -1;
		bool ambiguous = false;
		
		for (int i=0; cmdList[i].text; i++) {
			if (!strncmp(input, cmdList[i].text, inputLen)) {
				if (match != -1) {
					ambiguous = true;
					break;
				}
				match = i;
			}
		}

		/**
		 * 3 cases:
		 *  - No hits, return unknown and let our caller
		 *    dump the error.
		 *  - We match unambiguously or we have 1 or more matches
		 *    and the input is a single character. We then take the
		 *    first hit as our command.
		 *  - If we have multiple hits then we dump a 'ambiguous' message
		 *    and puke quietly.
		 */
		if (match == -1) {
			// no command match return unknown
			return defCmd;
		}
		// only 1 match or our input is 1 character or first match is exact
		else if (!ambiguous || inputLen == 1 || !strcmp(cmdList[match].text, input)) {
			return cmdList[match].id;
		}
		else {
			// matches more than one command dump message and go
			core->console << "Ambiguous command '" << input << "': ";
			bool first = true;
			for (int i=0; cmdList[i].text; i++) {
				if (!strncmp(cmdList[i].text, input, inputLen)) {
					if (!first) {
						core->console << ", ";
					} else {
						first = false;
					}
					core->console << cmdList[i].text;
				}
			}
			core->console << ".\n";
			return -1;
		}
	}

    const char* DebugCLI::commandNumberToCommandName(DebugCLI::StringIntArray cmdList[],
													 int cmdNumber)
    {
        for (int i = 0; cmdList[i].text; i++)
        {
            if (cmdList[i].id == cmdNumber)
                return cmdList[i].text;
        }

        return "?";
    }

	void DebugCLI::bt()
	{
		//core->stackTrace->dump(core->console);
		//core->console << '\n';

		// obtain information about each frame 
		int frameCount = core->debugger->frameCount();
		for(int k=0; k<frameCount; k++)
		{
			Atom* ptr;
			int count, line; 
			SourceInfo* src;
			DebugFrame* frame = core->debugger->frameAt(k);

			// source information
			frame->sourceLocation(src, line);

			core->console << "#" << k << "   ";

			// this 
			Atom a = nullObjectAtom;
			frame->dhis(a);
			core->console << core->format(a) << ".";

			// method
			MethodInfo* info = functionFor(src, line);
			if (info)
				core->console << info->name;
			else
				core->console << "<unknown>";

			core->console << "(";

			// dump args
			frame->arguments(ptr, count);
			for(int i=0; i<count; i++)
			{
				// write out the name
				if ( info && (info->getArgName(i) != core->kundefined) )
					core->console << info->getArgName(i) << "=";

				core->console << core->format(*ptr++);
				if (i<count-1)
					core->console << ",";
			}
			core->console << ") at ";
			if (src)
				core->console << src->name();
			else
				core->console << "???";

			core->console << ":" << (line) << "\n";
		}
	}

	MethodInfo* DebugCLI::functionFor(SourceInfo* src, int line)
	{
		MethodInfo* info = NULL;
		if (src)
		{
			// find the function at this location
			int size = src->functionCount();
			for(int i=0; i<size; i++)
			{
				MethodInfo* m = src->functionAt(i);
				if (line >= m->firstSourceLine && line <= m->lastSourceLine)
				{
					info = m;
					break;
				}
			}
		}
		return info;
	}

	// zero based
	char* DebugCLI::lineStart(int linenum)
	{
		if (!currentSource && currentFile)
			setCurrentSource(currentFile);

		if (!currentSource) {
			return NULL;
		}

		// linenumbers map to zero based array entry
		char *ptr = currentSource;
		for (int i=0; i<linenum; i++) {
			// Scan to end of line
			while (*ptr != '\n') {
				if (!*ptr) {
					return NULL;
				}
				ptr++;
			}
			// Advance past newline
			ptr++;
		}
		return ptr;
	}
	
	void DebugCLI::displayLines(int line, int count)
	{
		if (!lineStart(0)) {
			core->console << "No source available for current instruction\n";
		} else {
			// Display status
			int lineAt = line;
			while(count-- > 0)
			{
				char* ptr = lineStart(lineAt-1);
				if (!ptr)
				{
					#if WRAP_AROUND
					lineAt = 1;
					count++; // reset line number to beginning skip this iteration
					#else
					break;
					#endif
				}
				else
				{
					core->console << (lineAt) << ": ";
					while (*ptr && *ptr != '\n')
						core->console << *ptr++;
					core->console << '\n';
					lineAt++;
				}
			}
		}
	}

	void DebugCLI::list(const char* line)
	{
		int currentLine = (core->callStack) ? core->callStack->linenum : 0;
		int linenum = (line) ? atoi(line) : currentLine;
		displayLines(linenum, 10);
	}
	
	void DebugCLI::printIP()
	{
		int line = (core->callStack) ? core->callStack->linenum : 0;
		displayLines(line, 1);
	}

	void DebugCLI::breakpoint(char *location)
	{
		Stringp filename = currentFile;
		char *colon = strchr(location, ':');

		if (colon) {
			*colon = 0;
			filename = core->constantString(location);
			location = colon+1;
		}

		AbcFile *abcFile = (AbcFile*) abcAt(0);
		if (abcFile == NULL) {
			core->console << "No abc file loaded\n";
			return;
		}

		SourceFile *sourceFile = abcFile->sourceNamed(filename);
		if (sourceFile == NULL) {
			core->console << "No source available; can't set breakpoint.\n";
			return;
		}

		int targetLine = atoi(location);

		int breakpointId = ++breakpointCount;
		
		if (breakpointSet(sourceFile, targetLine)) {
			core->console << "Breakpoint " << breakpointId << ": file "
						  << filename
						  << ", " << (targetLine) << ".\n";

			BreakAction *breakAction = new (core->GetGC()) BreakAction(sourceFile,
																  breakpointId,
																  filename,
																  targetLine);
			breakAction->prev = lastBreakAction;
			if (lastBreakAction) {
				lastBreakAction->next = breakAction;
			} else {
				firstBreakAction = breakAction;
			}
			lastBreakAction = breakAction;
		} else {
			core->console << "Could not locate specified line.\n";
		}
	}

	void DebugCLI::showBreakpoints()
	{
		BreakAction *breakAction = firstBreakAction;
		while (breakAction) {
			breakAction->print(core->console);
			breakAction = breakAction->next;
		}
	}
	
	void DebugCLI::deleteBreakpoint(char *idstr)
	{
		int id = atoi(idstr);

		BreakAction *breakAction = firstBreakAction;
		while (breakAction) {
			if (breakAction->id == id) {
				break;
			}
			breakAction = breakAction->next;
		}

		if (breakAction) {
			if (breakAction->prev) {
				breakAction->prev->next = breakAction->next;
			} else {
				firstBreakAction = breakAction->next;
			}
			if (breakAction->next) {
				breakAction->next->prev = breakAction->prev;
			} else {
				lastBreakAction = breakAction->prev;
			}
			if (breakpointClear(breakAction->sourceFile, breakAction->linenum)) {
				core->console << "Breakpoint " << id << " deleted.\n";
			} else {
				core->console << "Internal error; could not delete breakpoint.\n";
			}
		} else {
			core->console << "Could not find breakpoint.\n";
		}
	}
	
	void DebugCLI::locals()
	{
		Atom* ptr;
		int count, line; 
		SourceInfo* src;
		DebugFrame* frame = core->debugger->frameAt(0);

		// source information
		frame->sourceLocation(src, line);

		// method
		MethodInfo* info = functionFor(src, line);
		if (info)
		{
			frame->arguments(ptr, count);
			for(int i=0; i<count; i++)
			{
				// write out the name
				if (info && (info->getLocalName(i) != core->kundefined) )
					core->console << info->getLocalName(i) << " = ";

				core->console << core->format(*ptr++);
				//if (i<count-1)
					core->console << "\n";
			}
		}
	}

	Atom DebugCLI::ease2Atom(const char* to, Atom baseline)
	{
		// first make a string out of the value
		Atom a = core->newString(to)->atom();

		// using the type of baseline try to convert to into an appropriate Atom
		if (core->isNumber(baseline))
			return core->numberAtom(a);
		else if (core->isBoolean(baseline))
			return core->booleanAtom(a);
		
		return nullStringAtom;
	}

	void DebugCLI::set()
	{
		const char* what = nextToken();
		const char* equ = nextToken();
		const char* to = nextToken();
		if (!to || !equ || !what || *equ != '=')
		{
			core->console << " Bad format, should be:  'set {variable} = {value}' ";
		}
		else
		{
			// look for the varable in our locals or args.
			Atom* ptr;
			int count, line; 
			SourceInfo* src;
			DebugFrame* frame = core->debugger->frameAt(0);

			// source information
			frame->sourceLocation(src, line);
			if (!src)
			{
				core->console << "Unable to locate debug information for current source file, so no local or argument names known";
				return;
			}

			// method
			MethodInfo* info = functionFor(src, line);
			if (!info)
			{
				core->console << "Unable to find method debug information, so no local or argument names known";
				return;
			}

			frame->arguments(ptr, count);
			for(int i=0; i<count; i++)
			{
				Stringp arg = info->getArgName(i);
				if (arg->Equals(what)) 
				{
					// match!
					Atom a = ease2Atom(to, ptr[i]);
					if (a == undefinedAtom)
						core->console << " Type mismatch : current value is " << core->format(ptr[i]);
					else
						frame->setArgument(i, a);
					return;
				}
			}

			frame->locals(ptr, count);
			for(int i=0; i<count; i++)
			{
				Stringp local = info->getLocalName(i);
				if ( local->Equals(what)) 
				{
					// match!
					Atom a = ease2Atom(to, ptr[i]);
					if (a == undefinedAtom)
						core->console << " Type mismatch : current value is " << core->format(ptr[i]);
					else
						frame->setLocal(i, a);
					return;
				}
			}
		}
	}

	void DebugCLI::print(const char *name)
	{
		if (!name) {
			core->console << "Must specify a name.\n";
			return;
		}

		// todo deal with exceptions
		Multiname mname(
			core->publicNamespace,
			core->constantString(name)
		);

		#if 0
		// rick fixme
		Atom objAtom = env->findproperty(outerScope, scopes, extraScopes, &mname, false);
		Atom valueAtom = env->getproperty(objAtom, &mname);
		core->console << core->string(valueAtom) << '\n';
		#endif
	}

	bool DebugCLI::filterException(Exception *exception)
	{
		// Filter exceptions when -d switch specified
		if (activeFlag) {
			core->console << "Exception has been thrown:\n"
						  << core->string(exception->atom)
						  << '\n';
			enterDebugger();
			return true;
		}
		return false;
	}

	void DebugCLI::info()
	{
		char *command = nextToken();
		int cmd = infoCommandFor(command);

		switch (cmd) {
		case -1:
			// ambiguous, we already printed error message
			break;
		case INFO_LOCALS_CMD:
			locals();
			break;
		case INFO_BREAK_CMD:
			showBreakpoints();
			break;
		case INFO_UNKNOWN_CMD:
			core->console << "Unknown command.\n";
			break;
		default:
			core->console << "Command not implemented.\n";
			break;
		}
	}
	
	void DebugCLI::enterDebugger()
	{	
		setCurrentSource( (core->callStack) ? (core->callStack->filename) : 0 );
		if (currentSource == NULL)
		{
			stepInto();
			return;
		}

		for (;;) {
			printIP();
			
			core->console << "(asdb) ";
			fflush(stdout);
			fgets(commandLine, kMaxCommandLine, stdin);

			commandLine[strlen(commandLine)-1] = 0;
			
			if (!commandLine[0]) {
				strcpy(commandLine, lastCommand);
			} else {
				strcpy(lastCommand, commandLine);
			}
				
			currentToken = commandLine;
		
			char *command = nextToken();
			int cmd = commandFor(command);

			switch (cmd) {
			case -1:
				// ambiguous, we already printed error message
				break;
			case CMD_INFO:
				info();
				break;
			case CMD_BREAK:
				breakpoint(nextToken());
				break;
			case CMD_DELETE:
				deleteBreakpoint(nextToken());
				break;
			case CMD_LIST:
				list(nextToken());
				break;
			case CMD_UNKNOWN:
				core->console << "Unknown command.\n";
				break;
			case CMD_QUIT:
				exit(0);
				break;
			case CMD_CONTINUE:
				return;
			case CMD_PRINT:
				print(nextToken());
				break;
			case CMD_NEXT:
				stepOver();
				return;
			case INFO_STACK_CMD:
				bt();
				break;
			case CMD_FINISH:
				stepOut();
				return;
			case CMD_STEP:
				stepInto();
				return;
			case CMD_SET:
				set();
				break;
			default:
				core->console << "Command not implemented.\n";
				break;
			}
		}
	}

	void DebugCLI::setCurrentSource(Stringp file)
	{
		if (!file)
			return;

		currentFile = file;

		if (currentSource) {
			delete [] currentSource;
			currentSource = NULL;
			currentSourceLen = -1;
		}
		
		// Open this file and suck it into memory
		FileInputStream f(currentFile->toUTF8String()->c_str());
		if (f.valid()) {
			currentSourceLen = f.available();
			currentSource = new char[currentSourceLen+1];
			f.read(currentSource, currentSourceLen);
			currentSource[currentSourceLen] = 0;

			// whip through converting \r\n to space \n
			for(int i=0; i<currentSourceLen-1;i++) {
				if (currentSource[i] == '\r' && currentSource[i+1] == '\n')
					currentSource[i] = ' ';
			}
		} else {
			core->console << "Error opening source file " << currentFile->c_str() << "\n";
		}
	}

	//
	// BreakAction
	//
		
	void BreakAction::print(PrintWriter& out)
	{
		out << id << " at "
			<< filename
			<< ":" << (linenum) << '\n';
	}
}
#endif
