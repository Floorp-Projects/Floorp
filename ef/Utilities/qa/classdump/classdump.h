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
*	classdump.h                                                
*                                                                  
*	author:		Patrick Dionisio                           
*                                                                  
*	purpose:	Header file for classdump.c		    
*	                                                           
********************************************************************/


/*Variables.*/

/*linked_list stores the index and the name of a string in 
the constant_pool.*/
struct linked_list {

       	int index;
       	char name[255];
       	struct linked_list *next_ptr;

};

/********************************************************************	
*
*	function:	void printTab(int i)
*
*	purpose:	Used for standout format. Prints out tabs 
*			in by the number of i.
*
*********************************************************************/

void printTab(int i)
{
	int j;

	for (j=0; j<i; j++ ) printf("\t");

}

/********************************************************************	
*
*	function:	void printByte(int i)
*
*	purpose:	Prints out the byte from the integer passed.
*
*
*********************************************************************/

void printByte(int i)
{
	putc(i,stdout);
}

/********************************************************************	
*
*	function:	void printHex1(int i)
*
*	purpose:	Prints out the hexidecimal value of one byte.
*
*
*********************************************************************/

void printHex1(int i)
{
	/*hex[] stores all hex values.*/
	static char hex[] = { '0','1','2','3','4','5','6','7','8','9','a',
		'b','c','d','e','f' };

	/*Print our the byte.*/
	printByte(hex[(i>>4)&15]);
	printByte(hex[i&15]);
	
}

/********************************************************************	
*
*	function:	void printHex2(int i)
*
*	purpose:	Prints out the hexidecimal value of two bytes.
*
*	
*********************************************************************/

void printHex2(int i)
{
	/*Print high-byte.*/
	printHex1(i>>8); 

	/*Print low-byte.*/
	printHex1(i&0xff);
}

/********************************************************************	
*
*	function:	void printHex4(int i)
*
*	purpose:	Prints out the hexidecimal value of four bytes.
*
*	
*********************************************************************/

void printHex4(int i)
{
	/*Print out the first two bytes.*/
	printHex2(i>>16);

	/*Print out the next two bytes.*/
	printHex2(i&0xffff);
}

/********************************************************************	
*
*	function:	void printDec4(int i)
*
*	purpose:	Prints out the decimal value of four bytes. 
*
*
*********************************************************************/

void printDec4(int i)
{
	int k,j=1;

	/*Check if i is negative. If it is, then print out a '-' and
	 make i positive.*/
	if ( i < 0 ) 
	{
		putc('-',stdout);
		i = -i;
	}
	
	/*Print out the bytes.*/
	k = i;
	while ( k /= 10 ) j *= 10;	
	while (j)
	{
		k = i/j;
		printByte(k+'0');
		i -= k*j;
		j /= 10;
	}
}

/********************************************************************	
*
*	function:	void printDec2(int i)
*
*	purpose:	Prints out the decimal value of the two bytes.
*
*
*********************************************************************/

void printDec2(int i)
{
	printDec4(i&0xffff);
}

/********************************************************************	
*
*	function:	int printDec1(int i)
*
*	purpose:	Prints out the decimal value of the one byte.
*
*  
*********************************************************************/

void printDec1(int i)
{
	printDec4(i&0xff);
}

/********************************************************************	
*
*	function:	char * getName(struct linked_list *first_ptr,int index)
*
*	purpose:	Returns the name stored in the given index from the 
*			constant_pool.
*
*********************************************************************/

char* getName(struct linked_list *first_ptr,int index)
{
	/*The pointer to use in going through the linked list.*/
	struct linked_list *current_ptr = NULL;
	current_ptr = first_ptr;

	/*Find the name in the list.*/
	while ( current_ptr != NULL )
	{
		if ( (*current_ptr).index == index ) return (*current_ptr).name;
		else current_ptr = (*current_ptr).next_ptr;
	}

	return "Error in getName";

}

/********************************************************************	
*
*	function:	void freeList(struct linked_list *first_ptr)
*
*	purpose:	Frees up the allocated memory.
*
*
*********************************************************************/

void freeList(struct linked_list *first_ptr)
{
	/*The pointer to use in going through the linked list.*/
	struct linked_list *current_ptr = NULL;
	current_ptr = first_ptr;

	/*Go through and clean the list.*/
	while ( first_ptr != NULL )
	{
		current_ptr = (*current_ptr).next_ptr;	
		free(first_ptr);
		first_ptr = current_ptr;
	}
}

/********************************************************************	
*
*	function:	char* getBytecode(int i, FILE *input)
*
*	purpose:	Prints the bytecode value.
*
*
*********************************************************************/

void getBytecode(int i, FILE *input)
{
	/*Local variables.*/
	int nested[10];
	int j,k,l,m,n,o;
	int posn = 0; /*Index for nested.*/
	int posf = 0; /*Current length of code.*/

	/*Reference type table.*/
	static char typ[256] = {
	/*     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f */ 
	/*0*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*1*/  1, 2, 1, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
	/*2*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*3*/  0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	/*4*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*5*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*6*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*7*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*8*/  0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*9*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6,
	/*a*/  6, 6, 6, 6, 6, 6, 6, 6, 6, 1, 7, 8, 0, 0, 0, 0,
	/*b*/  0, 0, 2, 2, 2, 2, 2, 2, 2, 9,10, 2, 0, 2, 0, 0,
	/*c*/  2, 2, 0, 0,11, 5, 2, 2, 3, 3, 0, 0, 0, 2, 0, 0,
	/*d*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*e*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/*f*/  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


	l=posf; 
	j=posf+i;
	posn++;

	/*Print out bytecodes till the length has been reached.*/
	while (posf<j)
	{
		printf("\n");
		printTab(4);
		nested[posn]=posf-l; 
		
		/*Get opcode.*/
		k=getu1(input); 
		posf++; 
		
		/*Check if opcode is valid.*/ 
		if (k>0xd2) k=0xd2;

		
		/*Refer to the type table and print out the correct bytecode along
		  with any values.*/
		switch (typ[k])
		{
		       
			case 0:  
				printf(mnem[k]); 
				break;

			case 1: 
				m=getu1(input); 
				printf(mnem[k]);
				printf("  "); 
				printHex1(m); 
				posf++; 
				break;

			case 2: 
				m=getu2(input);  
				printf(mnem[k]);
				printf("  "); 
				printHex2(m); 
				posf = posf + 2; 
				break;

			case 3: 
				m=getu4(input);  
				printf(mnem[k]); 
				printf("  "); 
				printHex4(m); 
				posf = posf + 4; 
				break;

			case 4: 
				m=getu1(input); 
				n=getu1(input);  
				printf(mnem[k]); 
				printf("  "); 
				printHex1(m);
				printf(","); 
				printHex1(n); 
				posf = posf + 2; 
				break;

			case 5: 
				m=getu2(input); 
				n=getu1(input);  
				printf(mnem[k]); 
				printf("  "); 
				printHex2(m);
				printf(","); 
				printHex1(n); 
				posf = posf + 3; 
				break;

			case 6: 
				m=getu2(input);  
				printf(mnem[k]); 
				printf("  "); 
				printHex2(m+nested[posn]); 
				posf = posf + 2; 
				break;

			case 7: /*Special case for tableswitch*/
				
				/*Read and ignore padded bytes.*/
				for (m=(nested[posn]&3)^3; m>0; m--) 
				{ getu1(input); posf++; }
 
				m=getu4(input); 
				n=getu4(input); 
				o=getu4(input); 
				posf = posf + 12;

				printf(mnem[k]); 
				printTab(1); 
				printHex4(m+nested[posn]);
				printf(","); 
				printHex4(n);
				printf(","); 
				printHex2(o); 
				posn++; 
				nested[posn]=n;  
				
				/*Print jump-offsets.*/
				while (nested[posn] <= o)
				{
					m=getu4(input); 
					printf("\n"); 
					printTab(6); 
					posf = posf + 4; 
					printHex4(m+nested[posn-1]);
					nested[posn]++;
				}
					
				posn--;  
				printf("\n");
				break;

			case 8:	/*Special case for lookupswitch. */ 
				
				/*Read and ignore padded bytes.*/
				for (m=(nested[posn]&3)^3; m>0; m--) 
				{ getu1(input); posf++; }
 
				m=getu4(input); 
				n=getu4(input); 
				posf = posf + 8;
				printf(mnem[k]); 
				printTab(1); 
				printHex4(m+nested[posn]);
				printf(","); 
				printHex4(n); 
				printf("\n"); 
				printTab(6);
				posn++; 
				nested[posn]=0;
 
				/*Print out offset pairs.*/
				while (nested[posn] < n)
				{
					m=getu4(input); 
					o=getu4(input);  
					posf = posf + 8;
					printHex4(m); 
					printf(" : "); 
					printHex4(o+nested[posn-1]);
					nested[posn]++; 
					printf("\n"); 
					printTab(6);
				}
 
				posn--;  
				printf("\n");
				break;

			case 9: 
				m=getu2(input); 
				n=getu1(input); 
				o=getu1(input);  
				posf = posf + 4; 
				printf(mnem[k]); 
				printf("  "); 
				printHex2(m);
				printf(","); 
				printHex1(n);
				printf(","); 
				printHex1(o); 
				break;

			case 10:
				m=getu1(input); 
				posf++; 
				printf(mnem[k]); 
				printf("  "); 
				
				switch (m) 
				{
					case  4: printf("T_BOOLEAN"); break;
					case  5: printf("T_CHAR"); break;
					case  6: printf("T_FLOAT"); break;
					case  7: printf("T_DOUBLE"); break;
					case  8: printf("T_BYTE"); break;
					case  9: printf("T_SHORT"); break;
					case 10: printf("T_INT"); break;
					case 11: printf("T_LONG"); break;
					default: printf("\nError");
				}
				break;

			case 11:
				m=getu1(input); 
				n=getu1(input); 
				posf = posf + 2;
				if ( n<0x15 || (n>0x19 && n<0x36) || (n>0x3a && n!=0x84)) printf("\nError");

				m=(m<<8)+getu1(input);  
				posf++; 
				printf(mnem[n]); 
				printf("  "); 
				printHex2(m); 
				break;
              
			default: printf("\nError");
		}

	}
  
	if (posf!=j) printf("\nError");
	posn--;  
	printf("\n");

}


/********************************************************************	
*
*	function:	int whichAttribute(char *constant_pool_name)
*
*	purpose:	Returns the attribute of the passed string.
*
*
*********************************************************************/

int whichAttribute(char *constant_pool_name)
{
	if ( strcmp("SourceFile",constant_pool_name) == 0 ) return SOURCE_FILE;
	if ( strcmp("ConstantValue",constant_pool_name) == 0 ) return CONSTANT_VALUE;
	if ( strcmp("Code",constant_pool_name) == 0 ) return CODE;
	if ( strcmp("Exceptions",constant_pool_name) == 0 ) return EXCEPTIONS;
	if ( strcmp("LineNumberTable",constant_pool_name) == 0 ) return LINENUMBERTABLE;
	if ( strcmp("LocalVariableTable",constant_pool_name) == 0 ) return LOCALVARIABLETABLE;
	
}

/********************************************************************	
*
*	function:	void getAttributes(int attr, FILE *input, struct linked_list *first_ptr)
*
*	purpose:	Prints out the attribute info.
*
*	
*********************************************************************/

void getAttributes(int attr, FILE *input, struct linked_list *first_ptr)
{
	/*Local variables.*/
	int length = 0;
	int k = 0;
	int count = 0;
	int index = 0;

	/*Print out the appropriate attribute.  Refer to the JVM Spec for more info.*/
	switch (attr)
	{
		case SOURCE_FILE:
			printf("SourceFile");
			printf(" length ");
			printDec4(getu4(input));
			printf("  index ");
			printDec2(getu2(input));
			break;

		case CONSTANT_VALUE:
			printf("ConstantValue");
			printf(" length ");
			printDec4(getu4(input));
			printf("  index ");
			printDec2(getu2(input));
			break;

		case CODE:
			printf("Code");
			
			printf("\n");
			printTab(2);
			printf("length");
			printTab(2);
			printDec4(getu4(input));
			
			printf("\n");
			printTab(2);
			printf("max_stack");
			printTab(1);
			printDec2(getu2(input));
			
			printf("\n");
			printTab(2);
			printf("max_local");
			printTab(1);
			printDec2(getu2(input));
			
			printf("\n");
			printTab(2);
			printf("length");
			printTab(2);
			length = getu4(input);
			printDec4(length);

			/*Print out the bytecodes.*/
			printf("\n\t\tbytecode");		    
			getBytecode(length,input);
			
			printf("\n");
			printTab(2);
			printf("exceptions");
			length = getu2(input);
			printTab(1);
			printDec2(length);
			for ( k=0; k < length; k++ )
			{
				printf("\n");
				printTab(4);
				printf("exception %i",k);
				printf("\n");
				printTab(4);
				printf("start_pc: ");
				printDec2(getu2(input));
				printf("end_pc: ");
				printDec2(getu2(input));
				printf("handler_pc: ");
				printDec2(getu2(input));
				printf("catch_type: ");
				printDec2(getu2(input));
			}
			
			printf("\n");
			printTab(2);
			printf("attributes");
			printTab(1);
			count = getu2(input);
			printDec2(count);
			for ( k=0; k<count; k++ )
			{
				index = getu2(input);
				switch(whichAttribute(getName(first_ptr,index)))
				{
					case LINENUMBERTABLE:
						getAttributes(LINENUMBERTABLE,input,first_ptr);
						printf("\n");
						break;

					case LOCALVARIABLETABLE:
						getAttributes(LOCALVARIABLETABLE,input,first_ptr);
						printf("\n");
						break;
					
					default: /*Ignore.*/
						for ( length=0; length < getu4(input); length++ ) getu1(input);
				}
			}

			break;

		case EXCEPTIONS:
			printf("Exceptions");
			length = getu4(input);
			printDec4(length);
			for ( k=0; k < length; k++ ) getu1(input);
			break;

		case LINENUMBERTABLE:
			
			printf("\n");
			printTab(4);
			printf("LineNumberTable ");

			printDec4(getu4(input));

			length = getu2(input);
			for ( k=0; k < length; k++ ) 
			{	
				printf("\n");
				printTab(4);
				printf("%i",k);
				printf(" start ");
				printDec2(getu2(input));
				printf("  line ");
				printDec2(getu2(input));
			}
			break;

		case LOCALVARIABLETABLE:

			printf("\n");
			printTab(4);
			printf("LocalVariableTable ");
			
			printDec4(getu4(input));

			length = getu2(input);
			for ( k=0; k < length; k++ ) 
			{
				printf("\n");
				printTab(4);
				printf("%i",k);
				printf(" start ");
				printDec2(getu2(input));
				printf("  length ");
				printDec2(getu2(input));
				printf("  name ");
				printDec2(getu2(input));
				printf("  descriptor ");
				printDec2(getu2(input));
				printf("  index ");
				printDec2(getu2(input));
			}
			break;

		default:
			printf("\nError in Attribute");
			exit(0);

	}
}






