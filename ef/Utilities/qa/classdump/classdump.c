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

/********************************************************************
*                                                                   
*	file:		classdump.c                                 
*                                                                   
*	author:		Patrick Dionisio                            
*                                                                   
*	purpose:	To read and print out a Java classfile.      
*	                                                            
*	usage:		classfile -<params> <classfile>               
*                                                                   
********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "classfile.h"
#include "classdump.h"

void main(int argc, char* argv[])
{

	/*Variables.*/
	int i,j,k = 0;
	int count = 0;
	int subcount = 0;
	int tag = 0;
	int length = 0;
	int access_flags;
	int index = 0;
	
	/*Pointer to the linked list which store the strings of the constant_pool.*/
	struct linked_list *first_ptr = NULL;
	struct linked_list *current_ptr = NULL;
	struct linked_list *new_ptr = NULL;

	/*Open class for reading.*/
	FILE *classfile;
	
	/*Get arguments.*/
	if (argc > 1) classfile = fopen(argv[1],"rb");
	else {
		printf("\nNo file to read.\n");
		exit(0);
	}
	
	
	/* Print classname */
	printf("classfile:");
	printTab(3);
	printf("%s",argv[1]);

	/* Start reading classfile. Refer to the JVM Spec for more info.*/

	/* u4 magic */
	printf("\nmagic: ");
	printTab(4);
	printHex4(getu4(classfile));

	/* u2 minor */
	printf("\nminor: ");
	printTab(4);
	printDec2(getu2(classfile));
	
	/* u2 major */
	printf("\nmajor: "); 
	printTab(4);
	printDec2(getu2(classfile));

	printf("\n");

	/* u2 constant_pool_count */
	printf("\nconstant_pool:"); 
	count = getu2(classfile);
	printf("\n");


	/* cp_info constant_pool[constant_pool_count-1] */
	for ( i = 1; i <= count - 1; i++ )
	{
		tag = getu1(classfile);
		
		switch (tag)
		{

			case CONSTANT_Utf8:
				
				printDec1(i);

				printTab(1);
				printf("utf8");

				if ( !first_ptr ) {
					
					first_ptr = malloc(sizeof(struct linked_list));
					(*first_ptr).index = i;
					strcpy((*first_ptr).name,getUtf8(classfile));
					current_ptr = first_ptr;
					
				}
				
				else {
					
					new_ptr = malloc(sizeof(struct linked_list));
					if ( new_ptr == NULL ) 
					{
						printf("Out of Memory for new_ptr");
						exit(0);
					}
					(*new_ptr).index = i;
					strcpy((*new_ptr).name,getUtf8(classfile));
					(*new_ptr).next_ptr = NULL;
					(*current_ptr).next_ptr = new_ptr;
					current_ptr = new_ptr;

				}

				printTab(3);
				printf("%s",(*current_ptr).name);
				printf("\n");
				break;
			
			case 2: 
				
				printf("unicode");
				printDec1(i);
				break;

			case CONSTANT_Integer:

				printDec1(i);

				printTab(1);
				printf("integer");

				printTab(3);
				printDec4(getu4(classfile));
				printf("\n");
				break;
			
			case CONSTANT_Float:

				printDec1(i);

				printTab(1);
				printf("float");

				printTab(3);
				printDec4(getu4(classfile));
				printf("\n");
				break;

			case CONSTANT_Long:

				printDec1(i);

				printTab(1);
				printf("long");

				printTab(3);
				printDec4((getu4(classfile)<<32) + getu4(classfile));
				printf("\n");
				break;

			case CONSTANT_Double:

				printDec1(i);

				printTab(1);
				printf("double");

				printTab(3);
				printDec4((getu4(classfile)<<32) + getu4(classfile));
				printf("\n");
				break;

			case CONSTANT_Class:

				printDec1(i);

				printTab(1);
				printf("class");

				printTab(3);
				printDec2(getu2(classfile));
				printf("\n");
				break;

			case CONSTANT_String:

				printDec1(i);

				printTab(1);
				printf("string");

				printTab(3);
				printDec2(getu2(classfile));
				printf("\n");
				break;

			case CONSTANT_Fieldref:
				
				printDec1(i);

				printTab(1);
				printf("fieldref");

				printTab(2);
				printf("class: ");
				printDec2(getu2(classfile));
				
				printTab(1);
				printf("name+type: ");
				printDec2(getu2(classfile));
				printf("\n");
				break;

			case CONSTANT_Methodref:

				printDec1(i);

				printTab(1);
				printf("methodref");

				printTab(2);
				printf("class: ");
				printDec2(getu2(classfile));

				printTab(1);
				printf("name+type: ");
				printDec2(getu2(classfile));
				printf("\n");
				break;

			case CONSTANT_InterfaceMethodref:

				printDec1(i);

				printTab(1);
				printf("interfacemethodref");

				printTab(1);
				printf("class: ");
				printDec2(getu2(classfile));
				
				printTab(1);
				printf("name+type: ");
				printDec2(getu2(classfile));
				printf("\n");
				break;

			case CONSTANT_NameAndType:
				
				printDec1(i);

				printTab(1);
				printf("name+type");

				printTab(2);
				printf("class: ");
				printDec2(getu2(classfile));
				
				printTab(1);
				printf("descriptor: ");
				printDec2(getu2(classfile));
				printf("\n");
				break;

			default: 
				
				printf("\nError in Constant Pool.");
				exit(0);

		}

	}

	/* u2 access_flags */
	printf("\naccess_flags"); 
	printTab(3);
	access_flags = getu2(classfile);
	printf("%s",getAccessFlags(access_flags));
	
	/* u2 this_class */
	printf("\nthis_class");
	printTab(3);
	printDec2(getu2(classfile));
	
	/* u2 super_class */
	printf("\nsuper_class");
	printTab(3);
	printDec2(getu2(classfile));
	
	printf("\n");

	/* u2 interfaces_count */
	printf("\ninterfaces");
	printTab(3);
	count = getu2(classfile);
	printDec2(count);
	
	/* u2 interfaces[interfaces_count] */
	if ( count != 0 ) for ( i=1; i <= count; i++)
	{
		printf("\n");
		printTab(1);
		printf("interface");
		printTab(2);
		printf("%i",i);
		printTab(1);
		printf("index ");
		printDec2(getu2(classfile));
		printf("\n");
	}

	printf("\n");
	
	/* u2 fields_count */
	printf("\nfields");
	printTab(4);
	count = getu2(classfile);
	printDec2(count);
	
	/* field_info fields[fields_count] */
	if ( count != 0 ) for ( i=1; i <= count; i++)
	{
		printf("\n");
		printTab(1);
		printf("field");
		printTab(3);
		printf("%i",i);
		
		/* u2 access_flags */
		printf("\n");
		printTab(1);
		printf("access_flags");
		printTab(2);
		access_flags = getu2(classfile);
		printf("%s",getAccessFlags(access_flags));

		/* u2 name_index */
		printf("\n");
		printTab(1);
		printf("name");
		printTab(3);
		printDec2(getu2(classfile));

		/* u2 descriptor_index */
		printf("\n");
		printTab(1);
		printf("descriptor");
		printTab(2);
		printDec2(getu2(classfile));

		/* u2 atributes_count */
		printf("\n");
		printTab(1);
		printf("attributes");
		printTab(2);
		subcount = getu2(classfile);
		printDec2(subcount);
		
		/* attribute_info attributes[attributes_count] */
		if ( subcount != 0 ) for ( j=1; j<=subcount; j++)
		{
			printf("\n");
			printTab(2);
			printf("attr_name");
			printTab(1);
			index = getu2(classfile);
						
			if ( whichAttribute(getName(first_ptr,index)) == CONSTANT_VALUE )
			{
				getAttributes(CONSTANT_VALUE,classfile,first_ptr);
			}

			else /*Silently ignore other attributes.*/
			{
				length = getu4(classfile);
				for ( k=0; k < length; k++ ) getu1(classfile);
			}
		}
	
		printf("\n");
	}

	printf("\n");

	/* u2 methods_count */
	printf("\nmethods");
	printTab(4);
	count = getu2(classfile);
	printDec2(count);
	
	/* methods_info methods[methods_count] */
	if ( count != 0 ) for ( i=1; i<=count; i++)
	{
		printf("\n");
		printTab(1);
		printf("method");
		printTab(3);
		printf("%i",i);

		/* u2 access_flags */
		printf("\n");
		printTab(1);
		printf("access_flags");
		printTab(2);
		access_flags = getu2(classfile);
		printf("%s",getAccessFlags(access_flags));

		/* u2 name_index */
		printf("\n");
		printTab(1);
		printf("name");
		printTab(3);
		printDec2(getu2(classfile));

		/* u2 descriptor_index */
		printf("\n");
		printTab(1);
		printf("descriptor");
		printTab(2);
		printDec2(getu2(classfile));

		/* u2 atributes_count */
		printf("\n");
		printTab(1);
		printf("attributes");
		printTab(2);
		subcount = getu2(classfile);
		printDec2(subcount);
		
		/* attribute_info attributes[attributes_count] */
		if ( subcount != 0 ) for ( j=1; j<=subcount; j++)
		{
			printf("\n");
			printTab(2);
			printf("attr_name");
			printTab(1);
			index = getu2(classfile);
			
			switch ( whichAttribute(getName(first_ptr,index)) )
			{
				case CODE:
					getAttributes(CODE,classfile,first_ptr);
					break;

				case EXCEPTIONS:
					getAttributes(EXCEPTIONS,classfile,first_ptr);
					break;

				default:/*Silently ignore other attributes.*/
					length = getu4(classfile);
					for ( k=0; k < length; k++ ) getu1(classfile);
			}
		
		}
	
		printf("\n");
	}

	/* u2 attributes_count */
	printf("\nattributes");
	count = getu2(classfile);
	printTab(3);
	printDec2(count);
	
	/* attributes_info attributes[attributes_count] */
	for ( i=1; i<=count; i++ )
	{
		printf("\n");
		printTab(1);
		printf("attribute");
		printTab(2);
		printf("%i",i);
		index = getu2(classfile);

		printf("\n");
		printTab(2);
		printf("attr_name");
		printTab(1);
		if (whichAttribute(getName(first_ptr,index)) == SOURCE_FILE)	
			getAttributes(SOURCE_FILE,classfile,first_ptr);
		else
		{
			length = getu4(classfile);
			for ( k=0; k < length; k++ ) getu2(classfile);
		}

		break;
	}

	
	printf("\n");

	/*Free memory.*/
	freeList(first_ptr);
	
	fclose(classfile);
	
		
}




















