/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include <stdio.h>
#include <fstream.h>
#include <ctype.h>
#include "FileGen.h"
#include "IdlObject.h"
#include "IdlVariable.h"
#include "IdlParameter.h"
#include "IdlInterface.h"

static const char *kNPLStr =  \
"/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-\n *\n * The contents of this file are subject to the Netscape Public License\n * Version 1.0 (the \"NPL\"); you may not use this file except in\n * compliance with the NPL.  You may obtain a copy of the NPL at\n * http://www.mozilla.org/NPL/\n *\n * Software distributed under the NPL is distributed on an \"AS IS\" basis,\n * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL\n * for the specific language governing rights and limitations under the\n * NPL.\n *\n * The Initial Developer of this code under the NPL is Netscape\n * Communications Corporation.  Portions created by Netscape are\n * Copyright (C) 1998 Netscape Communications Corporation.  All Rights\n * Reserved.\n */\n";
static const char *kDisclaimerStr = "/* AUTO-GENERATED. DO NOT EDIT!!! */\n";
static const char *kObjTypeStr = "nsIDOM%s*";
static const char *kUuidStr = "NS_IDOM%s_IID";

FileGen::FileGen()
{
    mOutputFile = new ofstream();
}

FileGen::~FileGen()
{
    delete mOutputFile;
}

int
FileGen::OpenFile(char *aFileName, char *aOutputDirName,
		  const char *aFilePrefix, const char *aFileSuffix)
{
  char file_buf[512];

  if (aOutputDirName) {
    strcpy(file_buf, aOutputDirName);
    strcat(file_buf, "/");
  }
  else {
    file_buf[0] = '\0';
  }
  strcat(file_buf, aFilePrefix);
  strcat(file_buf, strtok(aFileName, "."));
  strcat(file_buf, ".");
  strcat(file_buf, aFileSuffix);

  mOutputFile->open(file_buf);
  return mOutputFile->is_open();
}

void
FileGen::CloseFile()
{
  mOutputFile->close();
  mOutputFile->clear();
}

void            
FileGen::GenerateNPL()
{
  *mOutputFile << kNPLStr;
  *mOutputFile << kDisclaimerStr;
}

void
FileGen::GetVariableType(char *aBuffer, IdlVariable &aVariable)
{
    switch (aVariable.GetType()) {
      case TYPE_BOOLEAN:
	strcpy(aBuffer, "PRBool");
	break;
      case TYPE_LONG:
	strcpy(aBuffer, "PRInt32");
	break;
      case TYPE_SHORT:
	strcpy(aBuffer, "PRInt16");
	break;
      case TYPE_ULONG:
	strcpy(aBuffer, "PRUint32");
	break;
      case TYPE_USHORT:
	strcpy(aBuffer, "PRUint16");
	break;
      case TYPE_CHAR:
	strcpy(aBuffer, "PRUint8");
	break;
      case TYPE_INT:
	strcpy(aBuffer, "PRInt32");
	break;
      case TYPE_UINT:
	strcpy(aBuffer, "PRUint32");
	break;
      case TYPE_STRING:
	strcpy(aBuffer, "nsString&");
	break;
      case TYPE_OBJECT:
	sprintf(aBuffer, kObjTypeStr, aVariable.GetTypeName());
	break;
      default:
	// XXX Fail for other cases
	break;
    }
}

void
FileGen::GetParameterType(char *aBuffer, IdlParameter &aParameter)
{
  GetVariableType(aBuffer, aParameter);
  
  switch (aParameter.GetAttribute()) {
    case IdlParameter::OUTPUT:
    case IdlParameter::INOUT:
      if (aParameter.GetType() != TYPE_STRING) {
	strcat (aBuffer, "*");
      }
      break;
  }
}

void
FileGen::GetInterfaceIID(char *aBuffer, IdlInterface &aInterface)
{
  char buf[256];
  char *cur_ptr = buf;

  strcpy(buf, aInterface.GetName());
  while (*cur_ptr != 0) {
    if ((*cur_ptr >= 'a') && (*cur_ptr <= 'z')) {
      *cur_ptr += ('A' - 'a');
    }
    cur_ptr++;
  }

  sprintf(aBuffer, kUuidStr, buf);
}

void
FileGen::GetCapitalizedName(char *aBuffer, IdlObject &aObject)
{
  strcpy(aBuffer, aObject.GetName());
  if ((aBuffer[0] >= 'a') && (aBuffer[0] <= 'z')) {
    aBuffer[0] += ('A' - 'a');
  }
}
