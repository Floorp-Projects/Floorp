/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/***************************************************************
 *	classfile.h
 *	
 *	author: Patrick Dionisio
 *
 *	purpose:  Header file for using Java classfiles.
 *
 *
 ***************************************************************/

#include <stdlib.h>
#include <string.h>

/*Temp strings.*/
char tempString[255];
char access_flags_string[50];

/*Define Constant pool tags.*/
#define CONSTANT_Class				7
#define CONSTANT_Fieldref			9
#define CONSTANT_Methodref			10
#define CONSTANT_InterfaceMethodref		11
#define CONSTANT_String				8
#define CONSTANT_Integer			3
#define CONSTANT_Float				4
#define CONSTANT_Long				5
#define CONSTANT_Double				6
#define CONSTANT_NameAndType			12
#define CONSTANT_Utf8				1

/*Define access_flags.*/
#define ACC_PUBLIC				0x0001
#define ACC_PRIVATE			       	0x0002
#define ACC_PROTECTED				0x0004
#define ACC_STATIC			       	0x0008
#define ACC_FINAL			       	0x0010
#define ACC_SYNCHRONIZED			0x0020
#define ACC_VOLATILE				0x0040
#define ACC_TRANSIENT				0x0080
#define ACC_NATIVE			       	0x0100
#define ACC_INTERFACE				0x0200
#define ACC_ABSTRACT				0x0400

/*Define attribute names.*/
#define SOURCE_FILE			       	1
#define CONSTANT_VALUE				2
#define CODE				       	3
#define EXCEPTIONS			       	4
#define LINENUMBERTABLE				5
#define LOCALVARIABLETABLE			6


/*Opcode table*/
 static char *(mnem[]) = {
/*00              */ "nop",
/*01              */ "aconst_null",
/*02              */ "iconst_ml",
/*03              */ "iconst_0",
/*04              */ "iconst_1",
/*05              */ "iconst_2",
/*06              */ "iconst_3",
/*07              */ "iconst_4",
/*08              */ "iconst_5",
/*09              */ "lconst_0",
/*0a              */ "lconst_1",
/*0b              */ "fconst_0",
/*0c              */ "fconst_1",
/*0d              */ "fconst_2",
/*0e              */ "dconst_0",
/*0f              */ "dconst_1",
/*10 xx           */ "bipush            ",
/*11 xxxx         */ "sipush          ",
/*12 xx           */ "ldc1              ",
/*13 xxxx         */ "ldc2            ",
/*14 xxxx         */ "ldc2w           ",
/*15 xx           */ "iload             ",
/*16 xx           */ "lload             ",
/*17 xx           */ "fload             ",
/*18 xx           */ "dload             ",
/*19 xx           */ "aload             ",
/*1a              */ "iload_0",
/*1b              */ "iload_1",
/*1c              */ "iload_2",
/*1d              */ "iload_3",
/*1e              */ "lload_0",
/*1f              */ "lload_1",
/*20              */ "lload_2",
/*21              */ "lload_3",
/*22              */ "fload_0",
/*23              */ "fload_1",
/*24              */ "fload_2",
/*25              */ "fload_3",
/*26              */ "dload_0",
/*27              */ "dload_1",
/*28              */ "dload_2",
/*29              */ "dload_3",
/*2a              */ "aload_0",
/*2b              */ "aload_1",
/*2c              */ "aload_2",
/*2d              */ "aload_3",
/*2e              */ "iaload",
/*2f              */ "laload",
/*30              */ "faload",
/*31              */ "daload",
/*32              */ "aaload",
/*33              */ "baload",
/*34              */ "caload",
/*35              */ "saload",
/*36 xx           */ "istore            ",
/*37 xx           */ "lstore            ",
/*38 xx           */ "fstore            ",
/*39 xx           */ "dstore            ",
/*3a xx           */ "astore            ",
/*3b              */ "istore_0",
/*3c              */ "istore_1",
/*3d              */ "istore_2",
/*3e              */ "istore_3",
/*3f              */ "lstore_0",
/*40              */ "lstore_1",
/*41              */ "lstore_2",
/*42              */ "lstore_3",
/*43              */ "fstore_0",
/*44              */ "fstore_1",
/*45              */ "fstore_2",
/*46              */ "fstore_3",
/*47              */ "dstore_0",
/*48              */ "dstore_1",
/*49              */ "dstore_2",
/*4a              */ "dstore_3",
/*4b              */ "pstore_0",
/*4c              */ "pstore_1",
/*4d              */ "pstore_2",
/*4e              */ "pstore_3",
/*4f              */ "iastore",
/*50              */ "lastore",
/*51              */ "fastore",
/*52              */ "dastore",
/*53              */ "aastore",
/*54              */ "bastore",
/*55              */ "castore",
/*56              */ "sastore",
/*57              */ "pop",
/*58              */ "pop2",
/*59              */ "dup",
/*5a              */ "dup_x1",
/*5b              */ "dup_x2",
/*5c              */ "dup2",
/*5d              */ "dup2_x1",
/*5e              */ "dup2_x2",
/*5f              */ "swap",
/*60              */ "iadd",
/*61              */ "ladd",
/*62              */ "fadd",
/*63              */ "dadd",
/*64              */ "isub",
/*65              */ "lsub",
/*66              */ "fsub",
/*67              */ "dsub",
/*68              */ "imul",
/*69              */ "lmul",
/*6a              */ "fmul",
/*6b              */ "dmul",
/*6c              */ "idiv",
/*6d              */ "ldiv",
/*6e              */ "fdiv",
/*6f              */ "ddiv",
/*70              */ "irem",
/*71              */ "lrem",
/*72              */ "frem",
/*73              */ "drem",
/*74              */ "ineg",
/*75              */ "lneg",
/*76              */ "fneg",
/*77              */ "dneg",
/*78              */ "ishl",
/*79              */ "lshl",
/*7a              */ "ishr",
/*7b              */ "lshr",
/*7c              */ "iushr",
/*7d              */ "lushr",
/*7e              */ "iand",
/*7f              */ "land",
/*80              */ "ior",
/*81              */ "lor",
/*82              */ "ixor",
/*83              */ "lxor",
/*84 xx yy        */ "iinc              ",
/*85              */ "i2l",
/*86              */ "i2f",
/*87              */ "i2d",
/*88              */ "l2i",
/*89              */ "l2f",
/*8a              */ "l2d",
/*8b              */ "f2i",
/*8c              */ "f2l",
/*8d              */ "f2d",
/*8e              */ "d2i",
/*8f              */ "d2l",
/*90              */ "d2f",
/*91              */ "int2byte",
/*92              */ "int2char",
/*93              */ "int2short",
/*94              */ "lcmp",
/*95              */ "fcmpl",
/*96              */ "fcmpg",
/*97              */ "dcmpl",
/*98              */ "dcmpg",
/*99 xxxx         */ "ifeq            ",
/*9a xxxx         */ "ifne            ",
/*9b xxxx         */ "iflt            ",
/*9c xxxx         */ "ifge            ",
/*9d xxxx         */ "ifgt            ",
/*9e xxxx         */ "ifle            ",
/*9f xxxx         */ "if_icmpeq       ",
/*a0 xxxx         */ "if_icmpne       ",
/*a1 xxxx         */ "if_icmplt       ",
/*a2 xxxx         */ "if_icmpge       ",
/*a3 xxxx         */ "if_icmpgt       ",
/*a4 xxxx         */ "if_icmple       ",
/*a5 xxxx         */ "if_acmpeq       ",
/*a6 xxxx         */ "if_acmpne       ",
/*a7 xxxx         */ "goto            ",
/*a8 xxxx         */ "jsr             ", 
/*a9 xx           */ "ret             ",
/*aa [..]         */ "tableswitch  ",
/*ab [..]         */ "lookupswitch ",
/*ac              */ "ireturn",
/*ad              */ "lreturn",
/*ae              */ "freturn",
/*af              */ "dreturn",
/*b0              */ "areturn",
/*b1              */ "return",
/*b2 xxxx         */ "getstatic       ",
/*b3 xxxx         */ "putstatic       ",
/*b4 xxxx         */ "getfield        ",
/*b5 xxxx         */ "putfield        ",
/*b6 xxxx         */ "invokevirtual   ",
/*b7 xxxx         */ "invokenonvirtual",
/*b8 xxxx         */ "invokestatic    ",
/*b9 xxxx nn (nn) */ "invokeinterface ",
/*ba              */ "ungueltiger opcode", 
/*bb xxxx         */ "new             ",
/*bc              */ "newarray",
/*
bc 04            "newarray  T_BOOLEAN",
bc 05            "newarray  T_CHAR",
bc 06            "newarray  T_FLOAT",
bc 07            "newarray  T_DOUBLE",
bc 08            "newarray  T_BYTE",
bc 09            "newarray  T_SHORT",
bc 0a            "newarray  T_INT",
bc 0b            "newarray  T_LONG",
*/
/*bd xxxx         */ "anewarray       ",
/*be              */ "arraylength",
/*bf              */ "athrow",
/*c0 xxxx         */ "checkcast       ",
/*c1 xxxx         */ "instanceof      ",
/*c2              */ "monitorenter",
/*c3              */ "monitorexit",
/*c4 xx yy xx     */ "",
/*
c4 xx 15 xx      "iload",
c4 xx 16 xx      "lload",
c4 xx 17 xx      "fload",
c4 xx 18 xx      "dload",
c4 xx 19 xx      "aload",
c4 xx 36 xx      "istore",
c4 xx 37 xx      "lstore",
c4 xx 38 xx      "fstore",
c4 xx 39 xx      "dstore",
c4 xx 3a xx      "astore",
c4 xx 84 xx yy   "iinc",
*/
/*c5 xxxx yy      */ "multianewarray  ",
/*c6 xxxx         */ "ifnull          ",
/*c7 xxxx         */ "ifnonnull       ",
/*c8 xxxxxxxx     */ "goto_w      ",
/*c9 xxxxxxxx     */ "jsr_w       ",
/*ca              */ "breakpoint",
/*cb              */ "ungueltiger opcode", 
/*cc              */ "ungueltiger opcode",
/*cd              */ "ungueltiger opcode",
/*ce              */ "ungueltiger opcode",
/*cf              */ "ungueltiger opcode",
/*d0              */ "ungueltiger opcode",
/*d1 xxxx         */ "ret_w           ",
/*d2              */ "ungueltiger opcode"
};



/********************************************************************	
*
*	function:	int getu1(FILE *classfile)
*
*	purpose:	Reads one byte from the file and returns an integer 
*			value of the byte.
*
*********************************************************************/
 
int getu1(FILE *classfile)
{
	int i;
	if ( (i=getc(classfile)) == EOF ) printf("\nEnd of file reached.");
	return i;
}

/********************************************************************	
*
*	function:	int getu2(FILE *classfile)
*
*	purpose:	Reads two bytes from the file and returns the integer 
*			value of the two bytes.
*
*********************************************************************/

int getu2(FILE *classfile)
{
	int i;
	i = getu1(classfile);
	return ( (i<<8) + getu1(classfile));
}

/********************************************************************	
*
*	function:	int getu4(FILE *classfile)
*
*	purpose:	Reads four bytes from the file and returns the integer 
*			value of the four bytes.
*
*********************************************************************/

int getu4(FILE *classfile)
{
	int i;
	i = getu2(classfile);
	return( (i<<16) + getu2(classfile));
}

/********************************************************************	
*
*	function:	char utf8Tochar(int j)
*
*	purpose:	Returns the char value of the given int.
*
*
*********************************************************************/

char utf8Tochar(int j)
{
	char c;
	
	c = j;
	
	return c;
}

/********************************************************************	
*
*	function:	char* getUtf8(FILE *input)
*
*	purpose:	Gets the Utf8 data from the file and returns the
*			string value.
*
*********************************************************************/

char* getUtf8(FILE *input)
{
	/*Local variables.*/
	int length=0;
	int i = 0;
	
	/*Get the length of the string.*/
	length = getu2(input);

	/*Read the string from the file.*/
	for ( i=0; i<length; i++ )
	{

		tempString[i] = utf8Tochar(getu1(input));

	}
	
	/*Set the last value to null.*/
	tempString[length] = '\0';

	return tempString;
}

/********************************************************************	
*
*	function:	char* getAccessFlags(int access_flags)
*
*	purpose:	Returns the access flags.
*
*  
*********************************************************************/

char* getAccessFlags(int access_flags)
{
	/*Clear tempString.*/
	access_flags_string[0] = '\0';

	/* Find all flags and add them to the string. */
	if (access_flags&ACC_PUBLIC)		strcat(access_flags_string,"PUBLIC ");
	if (access_flags&ACC_PRIVATE)		strcat(access_flags_string,"PRIVATE ");
	if (access_flags&ACC_PROTECTED)		strcat(access_flags_string,"PROTECTED ");
	if (access_flags&ACC_STATIC)		strcat(access_flags_string,"STATIC ");
	if (access_flags&ACC_FINAL)			strcat(access_flags_string,"FINAL ");
	if (access_flags&ACC_SYNCHRONIZED)  strcat(access_flags_string,"SYNCHRONIZED ");
	if (access_flags&ACC_VOLATILE)		strcat(access_flags_string,"VOLATILE ");
	if (access_flags&ACC_TRANSIENT)		strcat(access_flags_string,"TRANSIENT ");
	if (access_flags&ACC_NATIVE)		strcat(access_flags_string,"NATIVE ");
	if (access_flags&ACC_INTERFACE)		strcat(access_flags_string,"INTERFACE ");
	if (access_flags&ACC_ABSTRACT)		strcat(access_flags_string,"ABSTRACT ");

	return access_flags_string;
}

