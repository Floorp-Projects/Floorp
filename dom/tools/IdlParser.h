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

#ifndef _IdlParser_h__
#define _IdlParser_h__

#include "IdlObject.h"
#include "IdlScanner.h"

class IdlSpecification;
class IdlInterface;
class IdlTypedef;
class IdlStruct;
class IdlEnum;
class IdlUnion;
class IdlException;
class IdlAttribute;
class IdlFunction;
class IdlParameter;
class IdlVariable;

class IdlParser {
private:
  IdlScanner *mScanner;

public:
                  IdlParser();
                  ~IdlParser();

  void            Parse(char *aFileName, IdlSpecification &aSpecification);

protected:

  IdlInterface*   ParseInterface(IdlSpecification &aSpecification);
  void            ParseInterfaceBody(IdlSpecification &aSpecification, IdlInterface &aInterface);

  IdlTypedef*     ParseTypedef(IdlSpecification &aSpecification);
  IdlStruct*      ParseStruct(IdlSpecification &aSpecification);
  IdlEnum*        ParseEnum(IdlSpecification &aSpecification);
  IdlUnion*       ParseUnion(IdlSpecification &aSpecification);
  IdlVariable*    ParseConst(IdlSpecification &aSpecification);
  IdlException*   ParseException(IdlSpecification &aSpecification);
  IdlAttribute*   ParseAttribute(IdlSpecification &aSpecification, int aTokenID);
  IdlFunction*    ParseFunction(IdlSpecification &aSpecification, Token &aToken);

  IdlParameter*   ParseFunctionParameter(IdlSpecification &aSpecification);

  void            TrimComments();

};

#endif // _IdlParser_h__

