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

#include "ClassGen.h"

/* getType(char *constantType) 
 *
 * function: Compares the passed string to the list of Constant Types.
 * Returns the constant type of the given string.
 *
 */
int getType(char *constantType) {

  if (strcmp(constantType,"Class") == 0) return 7;
  if (strcmp(constantType,"FieldRef") == 0) return 9;
  if (strcmp(constantType,"MethodRef") == 0) return 10;
  if (strcmp(constantType,"InterfaceMethodRef") == 0) return 11;
  if (strcmp(constantType,"String") == 0) return 8;
  if (strcmp(constantType,"Integer") == 0) return 3;
  if (strcmp(constantType,"Float") == 0) return 4;
  if (strcmp(constantType,"Long") == 0) return 5;
  if (strcmp(constantType,"Double") == 0) return 6;
  if (strcmp(constantType,"NameAndType") == 0) return 12;
  if (strcmp(constantType,"Utf8") == 0) return 1;
  
  //The type is unknown.  Return 0 to cause an error.
  return 0;

}

/* getStringLength(char *line)
 *
 * function: Finds the length of the passed string array.
 * Returns the length of the string from index 0 to '\n'.
 *
 */
int getStringLength(char *line) {

  int i = 0;
  while (line[i] != '\0') {
    i++;
  };

  return i;

}

/* isBlankLine(const char *line)
 *
 *function: Checks if the passed string is a blank line.
 *Returns true/false if the string is a blank line or not.
 *
 */
bool isBlankLine(const char *line) {

  for (; *line; line++) {
    
    if (!isspace(*line)) return false;

  }

  return true;

}

/* getItem(const char *line)
 *
 * function: Returns the appropriate classfile item.
 *
 */
int getItem(const char *line) {

  if (strcmp(line,"#magic\n") == 0) return CG_MAGIC;
  if (strcmp(line,"#minor_version\n") == 0) return CG_MINORVERSION;
  if (strcmp(line,"#major_version\n") == 0) return CG_MAJORVERSION;
  if (strcmp(line,"#constant_pool_count\n") == 0) return CG_CONSTANTPOOL;
  if (strcmp(line,"#access_flags\n") == 0) return CG_ACCESSFLAGS;
  if (strcmp(line,"#this_class\n") == 0) return CG_THISCLASS;
  if (strcmp(line,"#super_class\n") == 0) return CG_SUPERCLASS;
  if (strcmp(line,"#interfaces_count\n") == 0) return CG_INTERFACES;
  if (strcmp(line,"#fields_count\n") == 0) return CG_FIELDS;
  if (strcmp(line,"#methods_count\n") == 0) return CG_METHODS;
  if (strcmp(line,"#attributes_count\n") == 0) return CG_ATTRIBUTES;

  return 0;

}

/* getAttribute(const char *line)
 *
 * function: Returns the appropriate attribute.
 *
 */
int getAttribute(const char *line) {
  
  if (strcmp(line,"SourceFile") == 0) return CR_ATTRIBUTE_SOURCEFILE;
  if (strcmp(line,"ConstantValue") == 0) return CR_ATTRIBUTE_CONSTANTVALUE;
  if (strcmp(line,"LineNumberTable") == 0) return CR_ATTRIBUTE_LINENUMBERTABLE;
  if (strcmp(line,"LocalVariableTable") == 0) return CR_ATTRIBUTE_LOCALVARIABLETABLE;
  if (strcmp(line,"Code") == 0) return CR_ATTRIBUTE_CODE;
  if (strcmp(line,"Exceptions") == 0) return CR_ATTRIBUTE_EXCEPTIONS;

  return 0;

}

/* stripLine(char* line,int size,byte c)
 *
 * function: Removes the specified char from the line.
 *
 */
void stripLine(char* line,int size,byte c) {

  int i = 0;
  int j = 0;

  while ( line[i] == c ) i++;

  for (j = 0; j < (size - i); j++) {

    line[j] = line[j+i];
    if (line[j] == '\n') break;

  }
  
  /* Empty the rest of the string. */
  j++;
  while ( j < size ) {

    line[j] = '\0';
    j++;

  }

}

/* parseBytecodes(FILE *fp, FileWriter &writer)
 *
 *function: Parses and generates the bytecodes.
 *
 */
void parseBytecodes(FILE *fp,FileWriter &writer) {

  char line[256];

  fgets(line,sizeof(line),fp);
  stripLine(line,sizeof(line),' ');

  while (strcmp(line,"#end_bytecodes\n") != 0) {

    /* Need to add code here. */
    //printf("%s\n",line);

    fgets(line,sizeof(line),fp);
    stripLine(line,sizeof(line),' ');
    stripLine(line,sizeof(line),'\t');

  }

  return;

}

/* createConstantPool(FILE *fp, FileWriter &writer)
 *
 *function: Creates the constant pool.
 *
 */
void createConstantPool(FILE *fp,FileWriter &writer) {

  char line[256];
  char stringTag[256];

  /* Init string. */
  for (int m = 0; m < 256; m++) {

    line[m] = '\0';
    stringTag[m] = '\0';

  }
  
  fgets(line,sizeof(line),fp); /* #begin_constant_pool */
  fgets(line,sizeof(line),fp); /* #0 Reserved */

  fgets(line,sizeof(line),fp);
  while (strcmp(line,"#end_constant_pool\n") != 0) { 

    printf("string: %s", line);

    int index;
    char tag; 

    /* Get the index and constant tag from the string read. */
    sscanf(line,"\t#%i %s\n",&index,stringTag);
    
    /* Get the constant type and generate the correct info. */
    
    switch(getType(stringTag)) {
      
    case CR_CONSTANT_UTF8: { /* Utf8 */
      
      uint16 utf8Length;
      char utf8[256];

      /* Init utf8. */
      for (int i = 0; i < 256; i++) utf8[i] = '\0';

      fgets(line,sizeof(line),fp);
      stripLine(line,sizeof(line),'\t');
      sscanf(line,"%s",utf8);

      tag = 1;
      utf8Length = getStringLength(utf8);
      
      writer.writeU1(&tag,1);
      writer.writeU2(&utf8Length,1);

      printf("length: %i utf8: ",getStringLength(utf8));

      for (int j = 0; j < getStringLength(utf8); j++) {
	writer.writeU1(&utf8[j],1);
	printf("%c",utf8[j]);
      }
      
      printf("\n");

      break;
    }

    case CR_CONSTANT_INTEGER: { /* Integer */

      int tempValue;
      uint32 value;
      
      fgets(line,sizeof(line),fp);
      sscanf(line,"\tvalue %i",&tempValue);
      value = (uint32)tempValue;
      
      tag = 3;

      writer.writeU1(&tag,1);
      writer.writeU4(&value,1);
      
      break;

    }

    case CR_CONSTANT_FLOAT: {	/* Float */
      int s,e,m;
      uint32 s32,e32,m32;
      uint32 value;
      
      fgets(line,sizeof(line),fp);
      
      /* This will need to be refined later on. */
      sscanf(line,"\tvalue s %i e %i m %i",&s,&e,&m);
      e32 = (uint32)e;
      m32 = (uint32)m;
      
      tag = 4;
      
      /* Compute value. */
      if (s == 1) s32 = 0;
      else s32 = 1;      
      value = (s32 << 31) | (e32 << 23) | (m32 & 0x00ffffff);
      
      writer.writeU1(&tag,1);
      writer.writeU4(&value,1);
      
      break;
    
    }

    case CR_CONSTANT_LONG: {	/* Long */
     
      int tempHi,tempLo;
      uint32 hi,lo;

      fgets(line,sizeof(line),fp);
      
      /* This will need to be refined later on. */
      sscanf(line,"\tvalue hi %i lo %i",&tempHi,&tempLo);
      hi = (uint32)tempHi;
      lo = (uint32)tempLo;
      
      tag = 5;
      
      writer.writeU1(&tag,1);
      writer.writeU4(&hi,1);
      writer.writeU4(&lo,1);
      
      break;
    }
    
    case CR_CONSTANT_DOUBLE: { /* Double */
      
      char data[8];
      int byte;
      
      /* This will need to be refined later on. */
      fscanf(fp,"\tvalue ");
      for (int i = 0; i < 7; i++) {
	fscanf(fp,"%x ",&byte);
	data[i] = byte;
      }
      fscanf(fp,"%x",&byte);
      data[7] = byte;
      fgets(line,sizeof(line),fp);
      
      tag = 6;
      
      writer.writeU1(&tag,1);
      writer.writeU1(data,8);
      break;

    }

    case CR_CONSTANT_CLASS: { /* Class */

      int tempTagInfo;
      uint16 tagInfo;

      fgets(line,sizeof(line),fp);

      sscanf(line,"\tindex %i",&tempTagInfo);
      tagInfo = (uint16)tempTagInfo;

      tag = 7;
      
      writer.writeU1(&tag,1);
      writer.writeU2(&tagInfo,1);
      
      break;
    }
    
    case CR_CONSTANT_STRING: { /* String */

      int tempTagInfo;
      uint16 tagInfo;

      fgets(line,sizeof(line),fp);
     
      sscanf(line,"\tindex %i",&tempTagInfo);
      tagInfo = (uint16)tempTagInfo;

      tag = 8;
      
      writer.writeU1(&tag,1);
      writer.writeU2(&tagInfo,1);
      
      break;
    }

    case CR_CONSTANT_FIELDREF: { /* FieldRef */

      int tempClassIndex, tempTypeIndex;
      uint16 classIndex;
      uint16 typeIndex;
      
      fgets(line,sizeof(line),fp);
      sscanf(line,"\tclassIndex %i typeIndex %i",&tempClassIndex,&tempTypeIndex);
      classIndex = (uint16)tempClassIndex;
      typeIndex = (uint16)tempTypeIndex;

      tag = 9;
      
      writer.writeU1(&tag,1);
      writer.writeU2(&classIndex,1);
      writer.writeU2(&typeIndex,1);

      break;

    }
    
    case CR_CONSTANT_METHODREF: { /* MethodRef */
	
      int tempClassIndex, tempTypeIndex;
      uint16 classIndex;
      uint16 typeIndex;
	
      fgets(line,sizeof(line),fp);
      sscanf(line,"\tclassIndex %i typeIndex %i",&tempClassIndex,&tempTypeIndex);
      classIndex = (uint16)tempClassIndex;
      typeIndex = (uint16)tempTypeIndex;

      tag = 10;
      
      writer.writeU1(&tag,1);
      writer.writeU2(&classIndex,1);
      writer.writeU2(&typeIndex,1);

      break;
      
    }

    case CR_CONSTANT_INTERFACEMETHODREF: { /* InterfaceMethodRef */
      
      int tempClassIndex, tempTypeIndex;
      uint16 classIndex;
      uint16 typeIndex;

      fgets(line,sizeof(line),fp);
      sscanf(line,"\tclassIndex %i typeIndex %i",&tempClassIndex,&tempTypeIndex);
      classIndex = (uint16)tempClassIndex;
      typeIndex = (uint16)tempTypeIndex;

      tag = 11;

      printf("InterfaceMethodRef classIndex: %i typeIndex: %i\n",classIndex,typeIndex);

      writer.writeU1(&tag,1);
      writer.writeU2(&classIndex,1);
      writer.writeU2(&typeIndex,1);

      break;
      
    }

    case CR_CONSTANT_NAMEANDTYPE: { /* NameAndType */

      int tempNameIndex, tempDescIndex;
      uint16 nameIndex;
      uint16 descIndex;

      fgets(line,sizeof(line),fp);
      sscanf(line,"\tnameIndex %i descIndex %i",&tempNameIndex,&tempDescIndex);
      nameIndex = (uint16)tempNameIndex;
      descIndex = (uint16)tempDescIndex;
      
      tag = 12;
      
      writer.writeU1(&tag,1);
      writer.writeU2(&nameIndex,1);
      writer.writeU2(&descIndex,1);
    
      break;
    
    }

    default: {

      printf("Error: problem with constantpool\n");
      printf("%s",line);
      return;

    }
    
    }
    
    /* Read next line. */
    fgets(line,sizeof(line),fp);

    printf("next line: %s\n",line);
    
  }

  printf("Done with constant pool.\n");

  return;

}

/* createInterfaces(FILE *fp, FileWriter &writer)
 *
 *function: Creates the interfaces structure.
 *
 */
void createInterfaces(FILE *fp,FileWriter &writer) {

  char line[256];
  int tempIndex;
  uint16 index;

  fgets(line,sizeof(line),fp); /* #begin_interfaces */
  fgets(line,sizeof(line),fp); /* #<number> */
  
  while (strcmp(line,"#end_interfaces\n") != 0) { 

    fgets(line,sizeof(line),fp);
    sscanf(line,"\tindex %i\n",&tempIndex);
    index = (uint16)tempIndex;

    writer.writeU2(&index, 1);
     
    /* Read next line. */
    fgets(line,sizeof(line),fp);

  }
 
}

/* createFields(FILE *fp, FileWriter &writer, ClassFileReader &reader)
 *
 *function: Creates the fields structure.
 *
 */
void createFields(FILE *fp,FileWriter &writer,ClassFileReader &reader) {

  char line[256];
  uint16 accessFlags;
  uint16 nameIndex;
  uint16 descIndex;
  uint16 attrCount;
  int tempFlags, tempNameIndex, tempDescIndex, tempCount;

  fgets(line,sizeof(line),fp); /* #begin_fields */
  fgets(line,sizeof(line),fp); /* #<number> */  

  while (strcmp(line,"#end_fields\n") != 0) { 

	/* Access flags */
    fgets(line,sizeof(line),fp);
    sscanf(line,"\taccess_flags %x\n",&tempFlags);
    accessFlags = (uint16)tempFlags;
    writer.writeU2(&accessFlags,1);

    /* Name/Descriptor index */
    fgets(line,sizeof(line),fp);
    sscanf(line,"\tnameIndex %i descIndex %i\n",&tempNameIndex,&tempDescIndex);
    nameIndex = (uint16)tempNameIndex;
    descIndex = (uint16)tempDescIndex;
    writer.writeU2(&nameIndex,1);
    writer.writeU2(&descIndex,1);

    /* Attributes count */
    fgets(line,sizeof(line),fp);
    sscanf(line,"\tattributes_count %i\n",&tempCount);
    attrCount = (uint16)tempCount;
    writer.writeU2(&attrCount,1);

    /* Attributes structure */
    if ( tempCount > 0 ) { 

      char attribute[256];
      int attrIndex;

      /* Init attribute. */
      for (int i = 0; i < 256; i++) attribute[i] = '\0';

      fgets(line,sizeof(line),fp); /* #begin_attributes */
      fgets(line,sizeof(line),fp); /* #<number> Attribute */

	while (strcmp(line,"\t#end_attributes\n") != 0) {
	  
	  /* Get the attribute string. */
	  sscanf(line,"\t\t#%i %s\n",&attrIndex,attribute);
	  
	  createAttributes(fp,writer,reader,getAttribute(attribute));

	  /* Read the next line. */
	  fgets(line,sizeof(line),fp);

	}

    }

  fgets(line,sizeof(line),fp);
  
  }

} 

/* createMethods(FILE *fp, FileWriter &writer,ClassFileReader &reader)
 *
 *function: Creates the methods structure.
 *
 */
void createMethods(FILE *fp,FileWriter &writer,ClassFileReader &reader) {

  char line[256];
  uint16 accessFlags;
  uint16 nameIndex;
  uint16 descIndex;
  uint16 attrCount;
  int tempFlags, tempNameIndex, tempDescIndex, tempCount;

  fgets(line,sizeof(line),fp); /* #begin_methods */
  fgets(line,sizeof(line),fp); /* #<number> */
  
  while (strcmp(line,"#end_methods\n") != 0) { 

    /* Access flags */
    fgets(line,sizeof(line),fp);
    sscanf(line,"\taccess_flags %i\n",&tempFlags);
    accessFlags = (uint16)tempFlags;
    writer.writeU2(&accessFlags,1);

    /* Name/Descriptor index */
    fgets(line,sizeof(line),fp);
    sscanf(line,"\tnameIndex %i descIndex %i\n",&tempNameIndex,&tempDescIndex);
    nameIndex = (uint16)tempNameIndex;
    descIndex = (uint16)tempDescIndex;
    writer.writeU2(&nameIndex,1);
    writer.writeU2(&descIndex,1);

    /* Attributes count */
    fgets(line,sizeof(line),fp);
    sscanf(line,"\tattributes_count %i\n",&tempCount);
    attrCount = (uint16)tempCount;
    writer.writeU2(&attrCount,1);

    /* Attributes structure */
    if (tempCount > 0) {

      char attribute[256];
      int attrIndex;

      /* Init attribute. */
      for (int i = 0; i < 256; i++) attribute[i] = '\0';

      fgets(line,sizeof(line),fp); /* #begin_attributes */
      fgets(line,sizeof(line),fp); /* #<number> Attribute */

	while (strcmp(line,"\t#end_attributes\n") != 0) {
	  
	  /* Get the attribute string. */
	  sscanf(line,"\t\t#%i %s\n",&attrIndex,attribute);
	  
	  createAttributes(fp,writer,reader,getAttribute(attribute));

	  /* Read the next line. */
	  fgets(line,sizeof(line),fp);

	}

    }

    /* Read next line. */
    fgets(line,sizeof(line),fp);

  }
  
}

/* createAttributes(FILE *fp, FileWriter &writer,ClassFileReader &reader,int attribute)
 *
 *function: Creates the attribute structure.
 *
 */
void createAttributes(FILE *fp,FileWriter &writer,ClassFileReader &reader,int attribute) {

  char line[256];
  
  /* Init attribute. */
  for (int i = 0; i < 256; i++) line[i] = '\0';
  
  /* Generate the correct attribute. */
  switch(attribute) {
 
  case CR_ATTRIBUTE_SOURCEFILE: { /* SourceFile */
    
    uint16 nameIndex;
    uint32 length;
    uint16 index;
    int tempNameIndex, tempLength, tempIndex;
    
    fgets(line,sizeof(line),fp); /* nameIndex <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"nameIndex %i\n",&tempNameIndex);
    nameIndex = (uint16)tempNameIndex;
    writer.writeU2(&nameIndex,1);
    
    fgets(line,sizeof(line),fp); /* length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"length %i\n",&tempLength);
    length = (uint16)tempLength;
    writer.writeU4(&length,1);
    
    fgets(line,sizeof(line),fp); /* index <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"index %i\n",&tempIndex);
    index = (uint16)tempIndex;
    writer.writeU2(&index,1);
    
    break;
    
  }

  case CR_ATTRIBUTE_CONSTANTVALUE: { /* Constant Value */
    
    uint16 nameIndex;
    uint32 length;
    uint16 index;
    int tempNameIndex, tempLength, tempIndex;
    
    fgets(line,sizeof(line),fp); /* nameIndex <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"nameIndex %i\n",&tempNameIndex);
    nameIndex = (uint16)tempNameIndex;
    writer.writeU2(&nameIndex,1);
    
    fgets(line,sizeof(line),fp); /* length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"length %i\n",&tempLength);
    length = (uint16)tempLength;
    writer.writeU4(&length,1);
    
    fgets(line,sizeof(line),fp); /* index <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"index %i\n",&tempIndex);
    index = (uint16)tempIndex;
    writer.writeU2(&index,1);
    
    break;
    
  }

  case CR_ATTRIBUTE_LINENUMBERTABLE: { /* LineNumberTable Value */
    
    uint16 nameIndex;
    uint32 length;
    uint16 lineNoTableLength;
    int tempNameIndex, tempLength, tempCount;
    
    fgets(line,sizeof(line),fp); /* nameIndex <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"nameIndex %i\n",&tempNameIndex);
    nameIndex = (uint16)tempNameIndex;
    writer.writeU2(&nameIndex,1);
    
    fgets(line,sizeof(line),fp); /* length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"length %i\n",&tempLength);
    length = (uint32)tempLength;
    writer.writeU4(&length,1);
    
    fgets(line,sizeof(line),fp); /* table_length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"table_length %i\n",&tempCount);
    lineNoTableLength = (uint16)tempCount;
    writer.writeU2(&lineNoTableLength,1);

    /* Line number table. */
    if (tempCount > 0) {

      fgets(line,sizeof(line),fp); /* #begin_line_number_table */
      stripLine(line,sizeof(line),'\t'); 

      fgets(line,sizeof(line),fp); /* #<index> */

      while (strcmp(line,"#end_line_number_table\n") != 0) {

	uint16 startPc;
	uint16 lineNo;
	int temp;

	fgets(line,sizeof(line),fp); /* start_pc <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"start_pc %i\n",&temp);
	startPc = (uint16)temp;
	writer.writeU2(&startPc,1);

	fgets(line,sizeof(line),fp); /* line_number <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"line_number %i\n",&temp);
	lineNo = (uint16)temp;
	writer.writeU2(&lineNo,1);

	/* Read next entry. */
	fgets(line,sizeof(line),fp);
	stripLine(line,sizeof(line),'\t');

      }

    }
    
    break;
    
  }
	  
  case CR_ATTRIBUTE_LOCALVARIABLETABLE: { /* LocalVariableTable */
    
    uint16 nameIndex;
    uint32 length;
    uint16 localVarTableLength;
    int tempNameIndex, tempLength, tempCount;
    
    fgets(line,sizeof(line),fp); /* nameIndex <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"nameIndex %i\n",&tempNameIndex);
    nameIndex = (uint16)tempNameIndex;
    writer.writeU2(&nameIndex,1);
    
    fgets(line,sizeof(line),fp); /* length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"length %i\n",&tempLength);
    length = (uint32)tempLength;
    writer.writeU4(&length,1);
    
    fgets(line,sizeof(line),fp); /* table_length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"table_length %i\n",&tempCount);
    localVarTableLength = (uint16)tempCount;
    writer.writeU2(&localVarTableLength,1);

    /* Local variable table. */
    if (tempCount > 0) {

      fgets(line,sizeof(line),fp); /* #begin_local_variable_table */
      stripLine(line,sizeof(line),'\t'); 

      fgets(line,sizeof(line),fp); /* #<index> */

      while (strcmp(line,"#end_local_variable_table\n") != 0) {

	uint16 startPc;
	uint16 length;
	uint16 nameIndex;
	uint16 descIndex;
	uint16 index;
	int temp;

	fgets(line,sizeof(line),fp); /* start_pc <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"start_pc %i\n",&temp);
	startPc = (uint16)temp;
	writer.writeU2(&startPc,1);

	fgets(line,sizeof(line),fp); /* length <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"length %i\n",&temp);
	length = (uint16)temp;
	writer.writeU2(&length,1);

	fgets(line,sizeof(line),fp); /* nameIndex <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"nameIndex %i\n",&temp);
	nameIndex = (uint16)temp;
	writer.writeU2(&nameIndex,1);

	fgets(line,sizeof(line),fp); /* descIndex <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"descIndex %i\n",&temp);
	descIndex = (uint16)temp;
	writer.writeU2(&descIndex,1);

	fgets(line,sizeof(line),fp); /* index <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"index %i\n",&temp);
	index = (uint16)temp;
	writer.writeU2(&index,1);

	/* Read next entry. */
	fgets(line,sizeof(line),fp);
	stripLine(line,sizeof(line),'\t');

      }

    }
    
    break;

  }
  
  case CR_ATTRIBUTE_CODE: { /* Code */
    
    uint16 nameIndex;
    uint32 length;
    uint16 maxStack;
    uint16 maxLocals;
    uint32 codeLength;
    uint16 exceptionTableLength;
    uint16 attrCount;
    int temp, tempCount, bytecodeLength;

    fgets(line,sizeof(line),fp); /* nameIndex <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"nameIndex %i\n",&temp);
    nameIndex = (uint16)temp;
    writer.writeU2(&nameIndex,1);
    
    fgets(line,sizeof(line),fp); /* length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"length %i\n",&temp);
    length = (uint32)temp;
    writer.writeU4(&length,1);
  
    fgets(line,sizeof(line),fp); /* max_stack <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"max_stack %i\n",&temp);
    maxStack = (uint16)temp;
    writer.writeU2(&maxStack,1);
  
    fgets(line,sizeof(line),fp); /* maxLocals <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"max_locals %i\n",&temp);
    maxLocals = (uint16)temp;
    writer.writeU2(&maxLocals,1);
  
    fgets(line,sizeof(line),fp); /* code_length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"code_length %i\n",&bytecodeLength);
    codeLength = (uint32)bytecodeLength;
    writer.writeU4(&codeLength,1);

    /* Temp way for handling bytecodes. */
    if ( bytecodeLength > 0 ) {
      
      unsigned int bytecodeFromFile;
      unsigned char bytecode;

      fgetc(fp);
      fgetc(fp);
      fscanf(fp,"#bytecodes ");

      /* This needs to be refined. */
      for (int i = 0; i < bytecodeLength - 1; i++) {
	
	fscanf(fp,"%x ",&bytecodeFromFile);
	printf("bytcode: %x\n",bytecode);
	bytecode = (unsigned char) bytecodeFromFile;
	writer.writeU1((char *)(&bytecode),1);
    
      }
      fscanf(fp,"%x",&bytecodeFromFile);
      printf("bytcode: %x\n",bytecodeFromFile);
      bytecode = (unsigned char) bytecodeFromFile;
      writer.writeU1((char *)(&bytecode),1);
      fgets(line,sizeof(line),fp);

    }

    fgets(line,sizeof(line),fp); /* #begin_bytecodes */
    stripLine(line,sizeof(line),'\t');
   
    /* Bytecodes */
    parseBytecodes(fp,writer);

    fgets(line,sizeof(line),fp); /* exception_table_length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"exception_table_length %i\n",&tempCount);
    exceptionTableLength = (uint16)tempCount;
    writer.writeU2(&exceptionTableLength,1);

    /* Create exception table length. */
    if ( tempCount > 0 ) {

	uint16 startPc;
	uint16 endPc;
	uint16 handlerPc;
	uint16 catchType;
	int temp1, temp2;

	fgets(line,sizeof(line),fp); /* start_pc <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"start_pc %i end_pc %i\n",&temp1,&temp2);
	startPc = (uint16)temp;
	endPc = (uint16)temp;
	writer.writeU2(&startPc,1);
	writer.writeU2(&endPc,1);
	
	fgets(line,sizeof(line),fp); /* handler_pc <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"handler_pc  %i\n",&temp1);
	handlerPc = (uint16)temp1;
	writer.writeU2(&handlerPc,1);
	
	fgets(line,sizeof(line),fp); /* catch_type <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"catch_type  %i\n",&temp1);
	catchType = (uint16)temp1;
	writer.writeU2(&catchType,1); 
	
    }
    
    fgets(line,sizeof(line),fp); /* attributes_count <value> */
    stripLine(line,sizeof(line),'\t'); 
    sscanf(line,"attributes_count %i\n",&tempCount);
    attrCount = (uint16)tempCount;
    writer.writeU2(&attrCount,1);

    /* Attributes structure */
    if (tempCount > 0) {
      
      char attribute[256]; 
      int attrIndex;
      

      /* Init attribute. */
      for (int j = 0; j < 256; j++) attribute[j] = '\0';
      
      fgets(line,sizeof(line),fp); /* #begin_attributes */
      fgets(line,sizeof(line),fp); /* #<number> Attribute */
      stripLine(line,sizeof(line),'\t');

      while (strcmp(line,"#end_attributes\n") != 0) {
	
	/* Get the attribute string. */
	sscanf(line,"#%i %s\n",&attrIndex,attribute);
	
	createAttributes(fp,writer,reader,getAttribute(attribute));
	
	/* Read the next line. */
	fgets(line,sizeof(line),fp);
	stripLine(line,sizeof(line),'\t');
	
      }
      
    }
    
    break;

  }

  case CR_ATTRIBUTE_EXCEPTIONS: { /* Exceptions */

    uint16 nameIndex;
    uint32 length;
    uint16 numExceptions;
    int tempNameIndex, tempLength, tempCount;
    
    fgets(line,sizeof(line),fp); /* nameIndex <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"nameIndex %i\n",&tempNameIndex);
    nameIndex = (uint16)tempNameIndex;
    writer.writeU2(&nameIndex,1);
    
    fgets(line,sizeof(line),fp); /* length <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"length %i\n",&tempLength);
    length = (uint32)tempLength;
    writer.writeU4(&length,1);
    
    fgets(line,sizeof(line),fp); /* exceptions_count <value> */
    stripLine(line,sizeof(line),'\t');
    sscanf(line,"exceptions_count %i\n",&tempCount);
    numExceptions = (uint16)tempCount;
    writer.writeU2(&numExceptions,1);

    /* Excpetion index table. */
    if (tempCount > 0) {

      fgets(line,sizeof(line),fp); /* #begin_exception_index_table */
      stripLine(line,sizeof(line),'\t'); 

      fgets(line,sizeof(line),fp); /* #<index> */

      while (strcmp(line,"#end_exception_index_table\n") != 0) {

	uint16 index;
	int temp;

	fgets(line,sizeof(line),fp); /* index <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"index %i\n",&temp);
	index = (uint16)temp;
	writer.writeU2(&index,1);

	/* Read next entry. */
	fgets(line,sizeof(line),fp);
	stripLine(line,sizeof(line),'\t');

      }

    }

    break;

  }
    
  default: { /* Ignore everything else. */
    
    printf("Error: problems attribute\n");
    printf("%s",line);
    
    break;
    
  }

  }

}

/* createClass(FILE *fp,FileWriter &writer,ClassFileReader &reader)
 *
 * function: Create the class file.
 *
 */
void createClass(FILE *fp,FileWriter &writer,ClassFileReader &reader) {

  char line[256];

  /* Read line by line. */
  while (fgets(line,sizeof(line),fp) != NULL) { 

    if (!isBlankLine(line)) {

      switch (getItem(line)) {

      case CG_MAGIC: { /* Magic */

	uint32 magic;

	fgets(line, sizeof(line), fp);
	sscanf(line,"%x\n",&magic);
	writer.writeU4(&magic,1);  

	break;

      }

      case CG_MINORVERSION: { /* Minor version */

	int tempMinor;
	uint16 minor;

	fgets(line, sizeof(line), fp);
	sscanf(line,"%i\n",&tempMinor);
	minor = (uint16)tempMinor;
	writer.writeU2(&minor,1);

	break;

      }

      case CG_MAJORVERSION: { /* Major version */

	int tempMajor;
	uint16 major;

	fgets(line, sizeof(line), fp);
	sscanf(line,"%i\n",&tempMajor);
	major = (uint16)tempMajor;
	writer.writeU2(&major,1);

	break;

      }

      case CG_CONSTANTPOOL: { /* Constant pool */

	int tempCount;
	uint16 constantpoolcount;

	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempCount);
	constantpoolcount = (uint16)tempCount;
	writer.writeU2(&constantpoolcount,1);

	createConstantPool(fp,writer);

	printf("Done with ConstantPool\n");

	break;

      }

      case CG_ACCESSFLAGS: { /* Access flags */

	int tempFlags;
	uint16 accessFlags;

	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempFlags);
	accessFlags = (uint16)tempFlags;
	writer.writeU2(&accessFlags,1);

	break;

      }

      case CG_THISCLASS: { /* This class */

	int tempClass;
	uint16 thisClass;

	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempClass);
	thisClass = (uint16)tempClass;
	writer.writeU2(&thisClass,1);

	break;
      
      }

      case CG_SUPERCLASS: { /* Super class */

	int tempClass;
	uint16 superClass;

	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempClass);
	superClass = (uint16)tempClass;
	writer.writeU2(&superClass,1);

	break;

      }

      case CG_INTERFACES: { /* Interfaces */

	int tempCount;
	uint16 interfacesCount;

	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempCount);
	interfacesCount = (uint16)tempCount;
	writer.writeU2(&interfacesCount,1);

	if ( tempCount > 0 ) createInterfaces(fp,writer);

	break;

      }

      case CG_FIELDS: { /* Fields */

	int tempCount;
	uint16 fieldsCount;

	/* Fields count */
	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempCount);
	fieldsCount = (uint16)tempCount;
	writer.writeU2(&fieldsCount,1);

	if ( tempCount > 0 ) createFields(fp,writer,reader);

	break;

      }

      case CG_METHODS: { /* Methods */

	int tempCount;
	uint16 methodsCount;

	/* Mehtods count */
	fgets(line,sizeof(line),fp);
	sscanf(line,"%i\n",&tempCount);
	methodsCount = (uint16)tempCount;
	writer.writeU2(&methodsCount,1);

	if ( tempCount > 0 ) createMethods(fp,writer,reader);

	break;

      }

      case CG_ATTRIBUTES: { /* Attributes */

	int tempCount;
	uint16 attrCount;

	/* Attributes count */
	fgets(line,sizeof(line),fp); /* <value> */
	stripLine(line,sizeof(line),'\t');
	sscanf(line,"%i\n",&tempCount);
	attrCount = (uint16)tempCount;
	writer.writeU2(&attrCount,1);

	/* Attributes structure */
	if (tempCount > 0) {
	  
	  char attribute[256];
	  int attrIndex;
	  
	  /* Init attribute. */
	  for (int i = 0; i < 256; i++) attribute[i] = '\0';
	  
	  fgets(line,sizeof(line),fp); /* #begin_attributes */
	  fgets(line,sizeof(line),fp); /* #<number> Attribute */
	  stripLine(line,sizeof(line),'\t');
	  
	  while (strcmp(line,"#end_attributes\n") != 0) {
	    
	    /* Get the attribute string. */
	    sscanf(line,"#%i %s\n",&attrIndex,attribute);
	    
	    createAttributes(fp,writer,reader,getAttribute(attribute));
	    
	    /* Read the next line. */
	    fgets(line,sizeof(line),fp);
	    stripLine(line,sizeof(line),'\t');
	    
	  }
	  
	}
	

      }

      default: { /* Ignore other strings read. */
	
	break;

      }

      }

    }
    
  }

  return;

}

