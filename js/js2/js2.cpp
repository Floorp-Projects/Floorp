// -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// The contents of this file are subject to the Netscape Public
// License Version 1.1 (the "License"); you may not use this file
// except in compliance with the License. You may obtain a copy of
// the License at http://www.mozilla.org/NPL/
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
// The Original Code is the JavaScript 2 Prototype.
//
// The Initial Developer of the Original Code is Netscape
// Communications Corporation.  Portions created by Netscape are
// Copyright (C) 1998 Netscape Communications Corporation. All
// Rights Reserved.


//
// JS2 shell.
//

#include <algorithm>
//#include <stdlib.h>
#include "world.h"
namespace JS = JavaScript;
using namespace JavaScript;


#ifdef XP_MAC
#ifdef XP_MAC_MPW
/* Macintosh MPW replacements for the ANSI routines.  These translate LF's to CR's because
   the MPW libraries supplied by Metrowerks don't do that for some reason.  */
static void translateLFtoCR(char *str, int length)
{
    char *limit = str + length;
    while (str != limit) {
        if (*str == '\n')
            *str = '\r';
        str++;
    }
}

int fputc(int c, FILE *file)
{
    char buffer = c;
    if (buffer == '\n')
        buffer = '\r';
    return fwrite(&buffer, 1, 1, file);
}

int fputs(const char *s, FILE *file)
{
    char buffer[4096];
    int n = strlen(s);
    int extra = 0;
    
    while (n > sizeof buffer) {
        memcpy(buffer, s, sizeof buffer);
        translateLFtoCR(buffer, sizeof buffer);
        extra += fwrite(buffer, 1, sizeof buffer, file);
        n -= sizeof buffer;
        s += sizeof buffer;
    }
    memcpy(buffer, s, n);
    translateLFtoCR(buffer, n);
    return extra + fwrite(buffer, 1, n, file);
}

int fprintf(FILE* file, const char *format, ...)
{
    va_list args;
    char smallBuffer[4096];
    int n;
    int bufferSize = sizeof smallBuffer;
    char *buffer = smallBuffer;
    int result;

    va_start(args, format);
    n = vsnprintf(buffer, bufferSize, format, args);
    va_end(args);
    while (n < 0) {
        if (buffer != smallBuffer)
            free(buffer);
        bufferSize <<= 1;
        buffer = malloc(bufferSize);
        if (!buffer) {
            ASSERT(false);
            return 0;
        }
        va_start(args, format);
        n = vsnprintf(buffer, bufferSize, format, args);
        va_end(args);
    }
    translateLFtoCR(buffer, n);
    result = fwrite(buffer, 1, n, file);
    if (buffer != smallBuffer)
        free(buffer);
    return result;
}


#else /* XP_MAC_MPW */
#include <SIOUX.h>
#include <MacTypes.h>

static char *mac_argv[] = {"js", 0};

static void initConsole(StringPtr consoleName, const char* startupMessage, int &argc, char **&argv)
{
    SIOUXSettings.autocloseonquit = true;
    SIOUXSettings.asktosaveonclose = false;
    SIOUXSetTitle(consoleName);
    // Set up a buffer for stderr (otherwise it's a pig).
    setvbuf(stderr, new char[BUFSIZ], _IOLBF, BUFSIZ);

    std::cout << startupMessage;

    argc = 1;
    argv = mac_argv;
}
#endif /* XP_MAC_MPW */
#endif /* XP_MAC */


// Interactively read a line from the input stream in and put it into s.
static bool promptLine(istream &in, string &s, const char *prompt)
{
    std::cout << prompt;
#ifdef XP_MAC_MPW
    /* Print a CR after the prompt because MPW grabs the entire line when entering an interactive command */
    std::cout << std::endl;
#endif
#ifndef _WIN32
	return std::getline(in, s) != 0;
#else
	char buffer[256];
	bool result = fgets(buffer, sizeof(buffer), stdin) != 0;
	s = buffer;
	return result;
#endif
}


static void readEvalPrint(istream &in, World &world)
{
	String buffer;
	string line;
	String sourceLocation = widenCString("console");

	while (promptLine(in, line, buffer.empty() ? "js> " : "")) {
		if (!buffer.empty())
			buffer += uni::lf;
		appendChars(buffer, line.data(), line.size());
		if (!buffer.empty()) {
			try {
				Lexer l(world, buffer, sourceLocation);
				while (true) {
					const Token &t = l.get(true);
					if (t.hasKind(Token::end))
						break;
					String out;
					out += ' ';
					t.print(out, true);
					showString(std::cout, out);
				}
			} catch (Exception &e) {
				std::cout << std::endl;
				showString(std::cout, e.fullMessage());
			}
			std::cout << std::endl;
			clear(buffer);
		}
	}
    std::cout << std::endl;
#if 0	
    do {
        bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineNum;
        do {
            if (!GetLine(cx, bufp, file, startline == lineNum ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineNum++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));
    } while (!hitEOF);
    fprintf(stdout, "\n");
    return;
#endif
}


#if 0	
static int ProcessInputFile(JSContext *cx, JSObject *obj, char *filename)
{
    JSBool ok, hitEOF;
    JSScript *script;
    jsval result;
    JSString *str;
    char buffer[4096];
    char *bufp;
    int startline;
    FILE *file;

    if (filename && strcmp(filename, "-")) {
        file = fopen(filename, "r");
        if (!file) {
            fprintf(stderr, "Can't open \"%s\": %s", filename, strerror(errno));
            return 1;
        }
    } else {
        file = stdin;
    }

    if (!isatty(fileno(file))) {
        /*
         * It's not interactive - just execute it.
         *
         * Support the UNIX #! shell hack; gobble the first line if it starts with '#'.
         */
        int ch = fgetc(file);
        if (ch == '#') {
            while((ch = fgetc(file)) != EOF)
                if (ch == '\n' || ch == '\r')
                    break;
        } else
            ungetc(ch, file);
        script = JS_CompileFileHandle(cx, obj, filename, file);
        if (script)
            (void)JS_ExecuteScript(cx, obj, script, &result);
        return;
    }

    /* It's an interactive filehandle; drop into read-eval-print loop. */
    int32 lineNum = 1;
    hitEOF = JS_FALSE;
    do {
        bufp = buffer;
        *bufp = '\0';

        /*
         * Accumulate lines until we get a 'compilable unit' - one that either
         * generates an error (before running out of source) or that compiles
         * cleanly.  This should be whenever we get a complete statement that
         * coincides with the end of a line.
         */
        startline = lineNum;
        do {
            if (!GetLine(cx, bufp, file, startline == lineNum ? "js> " : "")) {
                hitEOF = JS_TRUE;
                break;
            }
            bufp += strlen(bufp);
            lineNum++;
        } while (!JS_BufferIsCompilableUnit(cx, obj, buffer, strlen(buffer)));
    } while (!hitEOF);
    fprintf(stdout, "\n");
    return;
}


static int
usage(void)
{
    std::cerr << "usage: js [-s] [-w] [-v version] [-f scriptfile] [scriptfile] [scriptarg...]\n";
    return 2;
}


static int
ProcessArgs(char **argv, int argc)
{
    int i;
    char *filename = NULL;
    jsint length;
    jsval *vector;
    jsval *p;
    JSObject *argsObj;
    JSBool isInteractive = JS_TRUE;

    for (i=0; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
              case 'f':
                if (i+1 == argc) {
                    return usage();
                }
                filename = argv[i+1];
                /* "-f -" means read from stdin */
                if (filename[0] == '-' && filename[1] == '\0')
                    filename = NULL;
                ProcessInputFile(filename);
                filename = NULL;
                /* XXX: js -f foo.js should interpret foo.js and then
                 * drop into interactive mode, but that breaks test
                 * harness. Just execute foo.js for now.
                 */
                isInteractive = JS_FALSE;
                i++;
                break;

              default:
                return usage();
            }
        } else {
            filename = argv[i++];
            isInteractive = JS_FALSE;
            break;
        }
    }

    if (filename || isInteractive)
        ProcessInputFile(filename);
    return gExitCode;
}
#endif

#include "icodegenerator.h"
static void testICG(World &world)
{
    //
    // testing ICG 
    //
    uint32 pos = 0;
    ICodeGenerator icg;

    // var i,j; i = j + 2;
    // i is bound to var #0, j to var #1

    icg.beginStatement(pos);
    Register r1 = icg.loadVariable(1);
    Register r2 = icg.loadImmediate(2.0);
    icg.saveVariable(0, icg.op(ADD, r1, r2));

    // j = a.b
    icg.beginStatement(pos);
    Register r4 = icg.loadName(world.identifiers[widenCString("a")]);
    Register r5 = icg.getProperty(world.identifiers[widenCString("b")], r4);
    icg.saveVariable(1, r5);
    
    // while (i) i = i + j;
    icg.beginWhileStatement(pos);
    r1 = icg.loadVariable(0);
    icg.endWhileExpression(r1);        
    icg.saveVariable(0, icg.op(ADD, icg.loadVariable(0), icg.loadVariable(1)));
    icg.endWhileStatement();

    // if (i) if (j) i = 3; else j = 4;
    r1 = icg.loadVariable(0);   
    icg.beginIfStatement(pos, r1);
        r2 = icg.loadVariable(1);
        icg.beginIfStatement(pos, r2);
        icg.saveVariable(0, icg.loadImmediate(3));
        icg.beginElseStatement(true);
        icg.saveVariable(1, icg.loadImmediate(4));
        icg.endIfStatement();
    icg.beginElseStatement(false);
    icg.endIfStatement();


    // switch (i) { case 3: case 4: j = 4; break; case 5: j = 5; break; default : j = 6; }
    r1 = icg.loadVariable(0);   
    icg.beginSwitchStatement(pos, r1);
        // case 3, note empty case statement (?necessary???)
        icg.endCaseCondition(icg.loadImmediate(3));
            icg.beginCaseStatement();
            icg.endCaseStatement();
        // case 4
        icg.endCaseCondition(icg.loadImmediate(4));
            icg.beginCaseStatement();
            icg.beginStatement(pos);
            icg.saveVariable(1, icg.loadImmediate(4));
            icg.breakStatement();
            icg.endCaseStatement();
        // case 5
        icg.endCaseCondition(icg.loadImmediate(5));
            icg.beginCaseStatement();
            icg.beginStatement(pos);
            icg.saveVariable(1, icg.loadImmediate(5));
            icg.breakStatement();
            icg.endCaseStatement();
        // default
            icg.beginDefaultStatement();
            icg.beginStatement(pos);
            icg.saveVariable(1, icg.loadImmediate(6));
            icg.endDefaultStatement();
    icg.endSwitchStatement();

    // for ( ; i; i + 1 ) j = 99;
    icg.beginForStatement(pos);
        r1 = icg.loadVariable(0);
        icg.forCondition(r1);
        icg.saveVariable(0, icg.op(ADD, icg.loadVariable(0), icg.loadImmediate(1)));
        icg.forIncrement();
        icg.saveVariable(0, icg.loadImmediate(99));
    icg.endForStatement();



    InstructionStream *iCode = icg.complete();

    std::cout << icg;
}

int main(int argc, char **argv)
{
  #if defined(XP_MAC) && !defined(XP_MAC_MPW)
    initConsole("\pJavaScript Shell", "Welcome to the js2 shell.\n", argc, argv);
  #endif
	World world;
  #if 0
    testICG(world);
  #else
	readEvalPrint(std::cin, world);
  #endif
    return 0;

    //return ProcessArgs(argv + 1, argc - 1);
}
