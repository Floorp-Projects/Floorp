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

#ifndef _Exception_h__
#define _Exception_h__

class Exception
{
private:
  char *mMessage;

public:
          Exception();
          Exception(const char *aMessage);
          Exception(Exception &aException);
          ~Exception();

  void    SetMessage(char *aMessage);
  char*   GetMessage();
};

class NotImplementedException : public Exception
{
public:
          NotImplementedException() : Exception("Function not yet implemented.") {}
          ~NotImplementedException() {}
};

class AbortParser : public Exception
{
public:
          AbortParser(char *aFileName, long aLineNumber);
          ~AbortParser() {}
};

class ParserException : public Exception
{
public:
          ParserException(char *aMessage) : Exception(aMessage) {}
          ~ParserException() {}
};

class InterfaceParsingException : public ParserException
{
public:
          InterfaceParsingException(char *aMessage) : ParserException(aMessage) {}
          ~InterfaceParsingException() {}
};

class EnumParsingException : public ParserException
{
public:
          EnumParsingException(char *aMessage) : ParserException(aMessage) {}
          ~EnumParsingException() {}
};

class AttributeParsingException : public ParserException
{
public:
          AttributeParsingException(char *aMessage) : ParserException(aMessage) {}
          ~AttributeParsingException() {}
};

class FunctionParsingException : public ParserException
{
public:
          FunctionParsingException(char *aMessage) : ParserException(aMessage) {}
          ~FunctionParsingException() {}
};

class ParameterParsingException : public ParserException
{
public:
          ParameterParsingException(char *aMessage) : ParserException(aMessage) {}
          ~ParameterParsingException() {}
};

class ConstParsingException : public ParserException
{
public:
          ConstParsingException(char *aMessage) : ParserException(aMessage) {}
          ~ConstParsingException() {}
};

class EndOfFileException : public Exception
{
public:
          EndOfFileException(char *aMessage) : Exception(aMessage) {}
          ~EndOfFileException() {}
};

class ScannerUnknownCharacter : public Exception
{
public:
          ScannerUnknownCharacter(char *aMessage) : Exception(aMessage) {}
          ~ScannerUnknownCharacter() {}
};

class ostream;
ostream& operator<<(ostream &s, Exception &e);

#endif

