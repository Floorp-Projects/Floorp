/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
// Commands.h
//
// Scott M. Silver


void stepIn(char*);
void stepOver(char*);
void go(char*);
void disassemble(char*);
void quit(char*);
void unhandledCommandLine(char*);
void breakExecution(char*);
void run(char*);
void printThreads(char*);
void printThisThread(char*);
void printThreadStack(char*);
void runClass(char*);
void switchThread(char*);
void connectToVM(char*);
void printIntegerRegs(char*);
void printFPRegs(char*);
void printIntegerRegs(char*);
void printAllRegs(char*);
void dumpMemory(char*);
void kill(char*);
