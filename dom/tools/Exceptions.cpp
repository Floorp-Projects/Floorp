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

#include "Exceptions.h"

#include <stdlib.h>
#include <string.h>
#include <ostream.h>

ostream& operator<<(ostream &s, Exception &e)
{
  return s << e.GetMessage() << '\n';
}

Exception::Exception()
{
  mMessage = (char*)0;
}

Exception::Exception(const char *aMessage)
{
  size_t size = strlen(aMessage) + 1;
  mMessage = new char[size];
  strcpy(mMessage, aMessage);
}

Exception::~Exception()
{
  if (mMessage) {
    delete[] mMessage;
  }
}

Exception::Exception(Exception &aException)
{
  size_t size = strlen(aException.mMessage) + 1;
  mMessage = new char[size];
  strcpy(mMessage, aException.mMessage);
}

void Exception::SetMessage(char *aMessage)
{
  if (mMessage) {
    delete[] mMessage;
    mMessage = (char*)0;
  }

  if (aMessage) {
    size_t size = strlen(aMessage) + 1;
    mMessage = new char[size];
    strcpy(mMessage, aMessage);
  }
}

char* Exception::GetMessage()
{
  return mMessage;
}

AbortParser::AbortParser(char *aFileName, long aLineNumber)
{
  char message[512]; // seems big enough
  char lineNumber[20]; 
  strcpy(message, "File: ");
  strcat(message, aFileName);
  strcat(message, ". Line Number: ");

  itoa(aLineNumber, lineNumber, 10);
  strcat(message, lineNumber);

  SetMessage(message);
}


