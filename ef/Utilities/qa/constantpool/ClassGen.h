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

#ifndef _CR_CLASSGEN_H_
#define _CR_CLASSGEN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "ConstantPool.h"
#include "DoublyLinkedList.h"
#include "Attributes.h"
#include "InfoItem.h"
#include "Pool.h"
#include "StringPool.h"
#include "ClassReader.h"
#include "FileReader.h"
#include "FileWriter.h"

/* Class items */
#define CG_MAGIC 1
#define CG_MINORVERSION 2
#define CG_MAJORVERSION 3
#define CG_CONSTANTPOOL 4
#define CG_ACCESSFLAGS 5
#define CG_THISCLASS 6
#define CG_SUPERCLASS 7
#define CG_INTERFACES 8
#define CG_FIELDS 9
#define CG_METHODS 10
#define CG_ATTRIBUTES 11

/* byte */
typedef unsigned char byte;

/* getType(char *constantType) 
 *
 * function: Compares the passed string to the list of Constant Types.
 * Returns the constant type of the given string.
 *
 */
int getType(char *constantType);

/* getStringLength(char *line)
 *
 * function: Finds the length of the passed string array.
 * Returns the length of the string from index 0 to '\n'.
 *
 */
int getStringLength(char *line);

/* isBlankLine(const char *line)
 *
 *function: Checks if the passed string is a blank line.
 *Returns true/false if the string is a blank line or not.
 *
 */
bool isBlankLine(const char *line);

/* getItem(const char *line)
 *
 * function: Returns the appropriate classfile item.
 *
 */
int getItem(const char *line);

/* getAttribute(const char *line)
 *
 * function: Returns the appropriate attribute.
 *
 */
int getAttribute(const char *line);

/* stripLine(char* line,int size,byte c)
 *
 * function: Removes the specified char from the line.
 *
 */
void stripLine(char* line,int size,byte c);

/* parseBytecodes(FILE *fp, FileWriter &writer)
 *
 *function: Parses and generates the bytecodes.
 *
 */
void parseBytecodes(FILE *fp,FileWriter &writer);

/* createConstantPool(FILE *fp, FileWriter &writer)
 *
 *function: Creates the constant pool.
 *
 */
void createConstantPool(FILE *fp,FileWriter &writer);

/* createInterfaces(FILE *fp, FileWriter &writer)
 *
 *function: Creates the interfaces structure.
 *
 */
void createInterfaces(FILE *fp,FileWriter &writer,ClassFileReader &reader);

/* createFields(FILE *fp, FileWriter &writer)
 *
 *function: Creates the fields structure.
 *
 */
void createFields(FILE *fp,FileWriter &writer,ClassFileReader &reader);

/* createMethods(FILE *fp, FileWriter &writer,ClassFileReader &reader)
 *
 *function: Creates the methods structure.
 *
 */
void createMethods(FILE *fp,FileWriter &writer,ClassFileReader &reader);

/* createAttributes(FILE *fp, FileWriter &writer,ClassFileReader &reader,int attribute,int tabs)
 *
 *function: Creates the attribute structure.
 *
 */

void createAttributes(FILE *fp,FileWriter &writer,ClassFileReader &reader,int attribute);

/* createClass(FILE *fp,FileWriter &writer,ClassFileReader &reader)
 *
 *function: Create the class file.
 *
 */
void createClass(FILE *fp,FileWriter &writer,ClassFileReader &reader);

#endif /* _CR_CLASSGEN_H_ */

