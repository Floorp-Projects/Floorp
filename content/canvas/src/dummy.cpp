/*
 * This file only exists because some platforms can't handle building an empty .a
 * file, or .lib, or whatever (e.g. OS X, possibly Win32).
 *
 * Need at least one external symbol for some linkers to create a proper
 * archive file: https://bugzilla.mozilla.org/show_bug.cgi?id=311143
 */
void concvsdummy(void) {}
