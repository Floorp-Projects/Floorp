/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *	 Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

////////////////////////////////////////////////////////////////////////////////
// Console stubs that send all console output to the Mac OS X Console.app, or
// to the terminal, if Mozilla is launched from the command line.
////////////////////////////////////////////////////////////////////////////////


#include <console.h>
#include <size_t.h>
#include <string.h>

#include <CFURL.h>
#include <CFBundle.h>
#include <CFString.h>
#include <MacErrors.h>
#include <Gestalt.h>
#include <CodeFragments.h>
#include <Fonts.h>
#include <TextEdit.h>
#include <Controls.h>

#include <SIOUX.h>
#include <SIOUXWindows.h>
#include <console_io.h>
#include <stdlib.h>

static CFBundleRef getBundle(CFStringRef frameworkPath)
{
	CFBundleRef bundle = NULL;
    
	//	Make a CFURLRef from the CFString representation of the bundle's path.
	//	See the Core Foundation URL Services chapter for details.
	CFURLRef bundleURL = CFURLCreateWithFileSystemPath(NULL, frameworkPath, kCFURLPOSIXPathStyle, true);
	if (bundleURL != NULL) {
        bundle = CFBundleCreate(NULL, bundleURL);
        if (bundle != NULL)
        	CFBundleLoadExecutable(bundle);
        CFRelease(bundleURL);
	}
	
	return bundle;
}

static void* getSystemFunction(CFStringRef functionName)
{
  static CFBundleRef systemBundle = getBundle(CFSTR("/System/Library/Frameworks/System.framework"));
  if (systemBundle) return CFBundleGetFunctionPointerForName(systemBundle, functionName);
  return NULL;
}

// Useful Carbon-CFM debugging tool, printf that goes to the system console.

typedef int (*io_proc_ptr) (int fd, void* buffer, long count);
static io_proc_ptr system_read = (io_proc_ptr) getSystemFunction(CFSTR("read"));
static io_proc_ptr system_write = (io_proc_ptr) getSystemFunction(CFSTR("write"));

/*
 *	The following four functions provide the UI for the console package.
 *	Users wishing to replace SIOUX with their own console package need
 *	only provide the four functions below in a library.
 */

static CFragConnectionID gConsoleLibrary;

static void* find_symbol(CFragConnectionID connection, StringPtr symName)
{
    Ptr symAddr;
    if (FindSymbol(gConsoleLibrary, symName, &symAddr, NULL) == noErr)
        return symAddr;
    return NULL;
}

/*
 *	extern short InstallConsole(short fd);
 *
 *	Installs the Console package, this function will be called right
 *	before any read or write to one of the standard streams.
 *
 *	short fd:		The stream which we are reading/writing to/from.
 *	returns short:	0 no error occurred, anything else error.
 */

short InstallConsole(short fd)
{
    long version;
    OSErr err = Gestalt(gestaltSystemVersion, &version);
    if (err == noErr && version < 0x00001000) {
        // load the "NSConsole" library.
        err = GetSharedLibrary("\pNSConsole", kCompiledCFragArch, kReferenceCFrag,
                               &gConsoleLibrary, NULL, NULL);
        if (err == noErr) {
            atexit(&RemoveConsole);
            // transfer the SIOUX settings.
            tSIOUXSettings *sioux_settings = (tSIOUXSettings*) find_symbol(gConsoleLibrary, "\pSIOUXSettings");
            if (sioux_settings) {
                *sioux_settings = SIOUXSettings;
                short (*install_console) (short) = (short (*) (short)) find_symbol(gConsoleLibrary, "\pInstallConsole");
                if (install_console)
                    return install_console(fd);
            }
        }
    }
	return 0;
}

/*
 *	extern void RemoveConsole(void);
 *
 *	Removes the console package.  It is called after all other streams
 *	are closed and exit functions (installed by either atexit or _atexit)
 *	have been called.  Since there is no way to recover from an error,
 *	this function doesn't need to return any.
 */

void RemoveConsole()
{
    if (gConsoleLibrary) {
        void (*remove_console) (void) = (void (*) (void)) find_symbol(gConsoleLibrary, "\pInstallConsole");
        if (remove_console)
            remove_console();
        CloseConnection(&gConsoleLibrary);
        gConsoleLibrary = NULL;
    }
}

/*
 *	extern long WriteCharsToConsole(char *buffer, long n);
 *
 *	Writes a stream of output to the Console window.  This function is
 *	called by write.
 *
 *	char *buffer:	Pointer to the buffer to be written.
 *	long n:			The length of the buffer to be written.
 *	returns short:	Actual number of characters written to the stream,
 *					-1 if an error occurred.
 */

long WriteCharsToConsole(char *buffer, long n)
{
    if (gConsoleLibrary) {
        static long (*write_chars) (char*, long) = (long (*) (char*, long)) find_symbol(gConsoleLibrary, "\pWriteCharsToConsole");
        if (write_chars)
            return write_chars(buffer, n);
    } else {
        for (char* cr = strchr(buffer, '\r'); cr; cr = strchr(cr + 1, '\r'))
            *cr = '\n';
        if (system_write) return system_write(1, buffer, n);
    }
	return 0;
}

/*
 *	extern long ReadCharsFromConsole(char *buffer, long n);
 *
 *	Reads from the Console into a buffer.  This function is called by
 *	read.
 *
 *	char *buffer:	Pointer to the buffer which will recieve the input.
 *	long n:			The maximum amount of characters to be read (size of
 *					buffer).
 *	returns short:	Actual number of characters read from the stream,
 *					-1 if an error occurred.
 */

long ReadCharsFromConsole(char *buffer, long n)
{
    if (gConsoleLibrary) {
        static long (*read_chars) (char*, long) = (long (*) (char*, long)) find_symbol(gConsoleLibrary, "\pReadCharsFromConsole");
        if (read_chars)
            return read_chars(buffer, n);
    } else {
        if (system_read) return system_read(0, buffer, n);
    }
	return -1;
}

extern char *__ttyname(long fildes)
{
	if (fildes >= 0 && fildes <= 2)
		return "console";
	return (0L);
}

int kbhit(void)
{
      return 0; 
}

int getch(void)
{
      return 0; 
}

void clrscr()
{
	/* Could send the appropriate VT100 sequence here... */
}

tSIOUXSettings	SIOUXSettings = {
    true, true, true, false, true, false, NULL,
    4, 80, 24, 0, 0, kFontIDMonaco,
    9, normal, true, true,	false
};

short SIOUXHandleOneEvent(EventRecord *userevent)
{
    if (gConsoleLibrary) {
        static short (*handle_event) (EventRecord*) = (short (*) (EventRecord*)) find_symbol(gConsoleLibrary, "\pSIOUXHandleOneEvent");
        if (handle_event)
            return handle_event(userevent);
    }
    return false;
}

Boolean SIOUXIsAppWindow(WindowPtr window)
{
    if (gConsoleLibrary) {
        static Boolean (*is_app_window) (WindowPtr) = (Boolean (*) (WindowPtr)) find_symbol(gConsoleLibrary, "\pSIOUXIsAppWindow");
        if (is_app_window)
            return is_app_window(window);
    }
    return false;
}

/**
 * Lower level console implementation. Let's us distinguish stdout from stderr.
 */

static int check_console()
{
    static short status = (InstallConsole(0) == 0);
    return status;
}

using namespace std;

int __read_console(__file_handle handle, unsigned char * buffer, size_t * count, __idle_proc idle_proc)
{
	if (!check_console())
		return(__io_error);

	fflush(stdout);

    long n = *count;
    if (gConsoleLibrary) {
        static long (*read_chars) (char*, long) = (long (*) (char*, long)) find_symbol(gConsoleLibrary, "\pReadCharsFromConsole");
        if (read_chars)
            n = read_chars((char*)buffer, n);
        else
            n = -1;
    } else {
        if (system_read)
            n = system_read(handle, buffer, n);
        else
            n = -1;
    }
    *count = n;
	return (n == -1 ? __io_error : __no_io_error);
}

int __write_console(__file_handle handle, unsigned char * buffer, size_t * count, __idle_proc idle_proc)
{
	if (!check_console())
		return(__io_error);
	
	long n = *count;
    if (gConsoleLibrary) {
        static long (*write_chars) (char*, long) = (long (*) (char*, long)) find_symbol(gConsoleLibrary, "\pWriteCharsToConsole");
        if (write_chars)
            n = write_chars((char*)buffer, n);
        else
            n = -1;
    } else {
        if (system_write) {
            for (char* cr = strchr((char*)buffer, '\r'); cr; cr = strchr(cr + 1, '\r'))
                *cr = '\n';
            n = system_write(handle, buffer, n);
        } else {
            n = -1;
        }
    }
    *count = n;
	return (n == -1 ? __io_error : __no_io_error);
}

int __close_console(__file_handle handle)
{
	return(__no_io_error);
}
