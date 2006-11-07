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

#ifndef __avmshell_AvmDebugCLI__
#define __avmshell_AvmDebugCLI__

#ifdef DEBUGGER
namespace avmshell
{
	/**
	 * Represents a single breakpoint in the Debugger.
	 */
	class BreakAction : public MMgc::GCObject
	{
	public:
		BreakAction *prev;
		BreakAction *next;
		SourceFile *sourceFile;
		int id;
		Stringp filename;
		int linenum;

		BreakAction(SourceFile *sourceFile,
					int id,
					Stringp filename,
					int linenum)
		{
			this->sourceFile = sourceFile;
			this->id = id;
			this->filename = filename;
			this->linenum = linenum;
		}
		
		void print(PrintWriter& out);
	};

	/**
	 * A simple command line interface for the Debugger.
	 * Supports a gdb-like command line.
	 */
	class DebugCLI : public avmplus::Debugger
	{
	public:
		/** @name command codes */
		/*@{*/
		const static int CMD_UNKNOWN		= 0;
		const static int CMD_QUIT			= 1;
		const static int CMD_CONTINUE		= 2;
		const static int CMD_STEP			= 3;
		const static int CMD_NEXT			= 4;
		const static int CMD_FINISH			= 5;
		const static int CMD_BREAK			= 6;
		const static int CMD_SET			= 7;
		const static int CMD_LIST			= 8;
		const static int CMD_PRINT			= 9;
		const static int CMD_TUTORIAL		= 10;
		const static int CMD_INFO			= 11;
		const static int CMD_HOME			= 12;
		const static int CMD_RUN			= 13;
		const static int CMD_FILE			= 14;
		const static int CMD_DELETE			= 15;
		const static int CMD_SOURCE			= 16;
		const static int CMD_COMMENT		= 17;
		const static int CMD_CLEAR			= 18;
		const static int CMD_HELP			= 19;
		const static int CMD_SHOW			= 20;
		const static int CMD_KILL			= 21;
		const static int CMD_HANDLE			= 22;
		const static int CMD_ENABLE			= 23;
		const static int CMD_DISABLE		= 24;
		const static int CMD_DISPLAY		= 25;
		const static int CMD_UNDISPLAY		= 26;
		const static int CMD_COMMANDS		= 27;
		const static int CMD_PWD            = 28;
		const static int CMD_CF             = 29;
		const static int CMD_CONDITION		= 30;
		const static int CMD_AWATCH			= 31;
		const static int CMD_WATCH			= 32;
		const static int CMD_RWATCH			= 33;
		const static int CMD_WHAT			= 34;
		const static int CMD_DISASSEMBLE	= 35;
		const static int CMD_HALT			= 36;
		const static int CMD_MCTREE			= 37;
		const static int CMD_VIEW_SWF		= 38;
		/*@}*/

		/** @name info sub commands */
		/*@{*/
		const static int INFO_UNKNOWN_CMD	= 100;
		const static int INFO_ARGS_CMD		= 101;
		const static int INFO_BREAK_CMD		= 102;
		const static int INFO_FILES_CMD		= 103;
		const static int INFO_HANDLE_CMD	= 104;
		const static int INFO_FUNCTIONS_CMD	= 105;
		const static int INFO_LOCALS_CMD	= 106;
		const static int INFO_SOURCES_CMD	= 107;
		const static int INFO_STACK_CMD		= 108;
		const static int INFO_VARIABLES_CMD	= 109;
		const static int INFO_DISPLAY_CMD	= 110;
		const static int INFO_TARGETS_CMD   = 111;
		const static int INFO_SWFS_CMD		= 112;
		/*@}*/

		/** @name show subcommands */
		/*@{*/
		const static int SHOW_UNKNOWN_CMD	= 200;
		const static int SHOW_NET_CMD		= 201;
		const static int SHOW_FUNC_CMD		= 202;
		const static int SHOW_URI_CMD		= 203;
		const static int SHOW_PROPERTIES_CMD= 204;
		const static int SHOW_FILES_CMD		= 205;
		const static int SHOW_BREAK_CMD		= 206;
		const static int SHOW_VAR_CMD		= 207;
		const static int SHOW_MEM_CMD		= 208;
		/*@}*/

		/** @name misc subcommands */
		/*@{*/
		const static int ENABLE_ONCE_CMD	= 301;
		/*@}*/

		/**
		 * StringIntArray is used to store the command arrays
		 * used to translate command strings into command codes
		 */
		struct StringIntArray
		{
			const char *text;
			int id;
		};
		
		DebugCLI(AvmCore *core);
		~DebugCLI();
		
		void enterDebugger();
		void setCurrentSource(Stringp file);
		bool filterException(Exception *exception);
		bool hitWatchpoint() { return false; }

		/**
		 * @name command implementations
		 */
		/*@{*/
		void breakpoint(char *location);
		void deleteBreakpoint(char *idstr);
		void showBreakpoints();
		void bt();
		void locals();
		void info();
		void set();
		void list(const char* line);
		void print(const char *name);
		void quit();
		/*@}*/

		void activate() { activeFlag = true; }
		
	private:
		bool activeFlag;
		char *currentSource;
		int currentSourceLen;
		Stringp currentFile;
		int breakpointCount;

		BreakAction *firstBreakAction, *lastBreakAction;
		
		enum { kMaxCommandLine = 1024 };
		char commandLine[kMaxCommandLine];
		char lastCommand[kMaxCommandLine];
		char *currentToken;
		char *nextToken();

		void printIP();
		void displayLines(int linenum, int count);
				
		char* lineStart(int linenum);
		Atom ease2Atom(const char* to, Atom baseline);
		MethodInfo* functionFor(SourceInfo* src, int line);

		/**
		 * @name command name arrays and support code
		 */
		/*@{*/
		int determineCommand(StringIntArray cmdList[],
							 const char *input,
							 int defCmd);
		const char* commandNumberToCommandName(StringIntArray cmdList[],
											   int cmdNumber);
		int commandFor(const char *input) {
			return determineCommand(commandArray, input, CMD_UNKNOWN);
		}
		int infoCommandFor(const char *input) {
			return determineCommand(infoCommandArray, input, INFO_UNKNOWN_CMD);
		}
		
		static StringIntArray commandArray[];
		static StringIntArray infoCommandArray[];
		static StringIntArray showCommandArray[];
		static StringIntArray enableCommandArray[];
		static StringIntArray disableCommandArray[];
		/*@}*/
	};
}
#endif

#endif /* __avmshell_AvmDebugCLI__ */
