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

/**

  IDL syntax 

  specification     = definition  < specification >  . 
  definition        =  ( type_dcl  / const_dcl  / except_dcl  / interface  / module  )  ";"  . 
  interface         = interface_dcl  / forward_dcl  . 

  forward_dcl       =  "interface" identifier  . 

  interface_dcl     = interface_header  "{" interface_body  "}"  . 
  interface_header  =  "interface" identifier  [ inheritance_spec  ]  . 
  interface_body    =  < export >  . 
  export            =  ( type_dcl  / const_dcl  / except_dcl  / attr_dcl  / op_dcl  )  ";"  . 

  inheritance_spec  =  ":" scoped_name  <  "," scoped_name  >  . 
  scoped_name       = identifier 
                      /  (  "::" identifier  ) 
                      /  ( scoped_name  "::" identifier  )  . 

 */

// initialize cout
#ifdef XP_MAC
#include <iostream>
#endif
#include <ostream.h>

#include "IdlParser.h"

#include "IdlScanner.h"

#include "Exceptions.h"

#include "IdlSpecification.h"
#include "IdlInterface.h"
#include "IdlEnum.h"
#include "IdlVariable.h"
#include "IdlAttribute.h"
#include "IdlFunction.h"
#include "IdlParameter.h"
// not defined yet. We don't need this types for now
// so the forward declaration in IdlParser.h is good enough
/*
#include "IdlTypedef.h"
#include "IdlStruct.h"
#include "IdlUnion.h"
#include "IdlConst.h"
#include "IdlException.h"
*/

/**
 * constructor
 */
IdlParser::IdlParser()
{
  mScanner = new IdlScanner();
}

/**
 * destructor
 */
IdlParser::~IdlParser()
{
  if (mScanner) {
    delete mScanner;
  }
}

/**
 * specification     = definition  < specification >  . 
 * definition        = ( type_dcl  / const_dcl  / except_dcl  / interface  / module  )  ";"  . 
 *
 * We only understand interfaces so go directly to process interfaces.
 * A more complex parsing of specification requires changes in this method
 * (or subclassing if you like and you can let this class process the interfaces only)
 */
void IdlParser::Parse(char *aFileName, IdlSpecification &aSpecification)
{
  // connect the scanner to the file in input 
  if (mScanner->Open(aFileName)) {

    try {
      // read all the interfaces in the idl file
      IdlInterface *idlInterface;
      while (mScanner->CanReadMoreData()) {
        idlInterface = ParseInterface(aSpecification);
        if (idlInterface)
          aSpecification.AddInterface(idlInterface);
      }
    } catch (ParserException &exc) {
      cout << exc;
      throw AbortParser(mScanner->GetFileName(), mScanner->GetLineNumber());
    } 
  }
  else {
    throw FileNotFoundException(aFileName);
  }
}

/**
 * interface         = interface_dcl  / forward_dcl
 * forward_dcl       = "interface" identifier
 * interface_dcl     = interface_header  "{" interface_body  "}"
 * interface_header  = "interface" identifier  [ inheritance_spec  ]
 * interface_body    =  < export >
 *
 * We look for 
 * "interface" identifier
 * and if followed by "{" or ":" we expect a full interface declaration 
 * otherwise it is just a forward declaration
 *
 */
IdlInterface* IdlParser::ParseInterface(IdlSpecification &aSpecification)
{
  IdlInterface *interfaceObj = (IdlInterface*)0;
  // go through all comments and assume the last one is a comment for the
  // interface
  Token *token = (Token*)0;
  while (mScanner->CanReadMoreData() && COMMENT_TOKEN == mScanner->PeekToken()->id) {
    token = mScanner->NextToken();
  }
  if (token) {
    //XXX save last comment somewhere...
  }

  if (mScanner->CanReadMoreData()) {
    try {
      // must be "interface" identifier
      if (INTERFACE_TOKEN == mScanner->NextToken()->id) {
        TrimComments();
        token = mScanner->NextToken();
        if (IDENTIFIER_TOKEN == token->id) {
          interfaceObj = new IdlInterface();
          interfaceObj->SetName(token->stringID);
        }
        else {
          throw InterfaceParsingException("Missing identifier. Interface name undefined.");
        }
      }
      else {
        throw InterfaceParsingException("Interface expected. We only process interfaces specification.");
      }

      TrimComments();

      // look for ":"
      int inheritanceSpec = 0;
      token = mScanner->PeekToken();
      if (INHERITANCE_SPEC_TOKEN == token->id) {
        mScanner->NextToken(); // read ":"
        TrimComments();

        //XXX we deal only with an interface_header structured as follow
        //    interface_header = ":" identifier < "," identifier > 
        token = mScanner->NextToken(); // this must be the base class
        while (IDENTIFIER_TOKEN == token->id) {
          interfaceObj->InheritsFrom(token->stringID);
          TrimComments();
          token = mScanner->PeekToken();
          if (SEPARATOR_TOKEN == token->id) {
            mScanner->NextToken(); // eat ","
            TrimComments();
            token = mScanner->NextToken(); // this must be the next base class
          }
          else {
            // next token isn't a ","
            // we are done with interface_header
            inheritanceSpec = 1;
            break;
          }
        }

        if (0 == inheritanceSpec) {
          delete interfaceObj;
          throw InterfaceParsingException("Bad inheritance specification.");
        }
      }

      // got a "{"
      if (BEGIN_BLOCK_TOKEN == token->id) {
        ParseInterfaceBody(aSpecification, *interfaceObj);
      }
      else if (inheritanceSpec) {
        delete interfaceObj;
        // if interface_header was found a interface_body must be defined
        throw InterfaceParsingException("Missing interface body.");
      }
      else {
        //XXX it's a forward declaration what do we do?
      }
    } catch (EndOfFileException &) {
      if (interfaceObj != 0) {
        delete interfaceObj;
      }
      // unexpected end of file. Trying to read after a EOF has
      // been returned
      throw ParserException("Unexpected End of File.");
    }

    // read the final ";"
    token = mScanner->NextToken();
    if (TERMINATOR_TOKEN != token->id) {
      throw InterfaceParsingException("Missing ';'.");
    }
  }

  return interfaceObj;
}
  
/**
 * interface_body   =  < export > 
 * export           =  ( type_dcl / const_dcl / except_dcl / attr_dcl / op_dcl ) ";" 
 * type_dcl         =  "typedef" type_declarator 
 *                      / struct_type 
 *                      / union_type 
 *                      / enum_type
 * struct_type      =  "struct" identifier "{" member_list "}"
 * union_type       =  "union" identifier "switch" "(" switch_type_spec ")" 
 *                     "{" switch_body  "}"
 * enum_type        =  "enum" identifier  "{" enumerator  <  "," enumerator  >  "}"
 * const_dcl        =  "const" const_type identifier "=" const_exp
 * except_dcl       =  "exception" identifier "{" < member > "}" 
 * attr_dcl         =  [ "readonly" ] [ "xpidl" ] "attribute" param_type_spec identifier
 * op_dcl           =  [ op_attribute  ] op_type_spec identifier parameter_dcls 
 *                     [ raises_expr ] [ context_expr ] 
 * op_attribute     =  "oneway"  . 
 * op_type_spec     =  param_type_spec / "void"
 * param_type_spec  =  base_type_spec / string_type / scoped_name
 * base_type_spec   =  floating_pt_type 
 *                      / integer_type 
 *                      / char_type 
 *                      / boolean_type 
 *                      / octet_type 
 *                      / any_type
 * scoped_name      =  identifier 
 *                      / ( "::" identifier ) 
 *                      / ( scoped_name "::" identifier )
 */
void IdlParser::ParseInterfaceBody(IdlSpecification &aSpecification, IdlInterface &aInterface)
{
  // eat "{"
  mScanner->NextToken();
  TrimComments();

  // untill "}" parse the interface body
  Token *token = (Token*)0;
  while (END_BLOCK_TOKEN != mScanner->PeekToken()->id) {
    token = mScanner->NextToken();
    TrimComments();

    // parse the proper valid type
    switch (token->id) {
      case IID_TOKEN:
        aInterface.AddIID(token->stringID);
        break;
      case TYPEDEF_TOKEN:
        aInterface.AddTypedef(ParseTypedef(aSpecification));
        break;
      case STRUCT_TOKEN:
        aInterface.AddStruct(ParseStruct(aSpecification));
        break;
      case ENUM_TOKEN:
        aInterface.AddEnum(ParseEnum(aSpecification));
        break;
      case UNION_TOKEN:
        aInterface.AddUnion(ParseUnion(aSpecification));
        break;
      case CONST_TOKEN:
        aInterface.AddConst(ParseConst(aSpecification));
        break;
      case EXCEPTION_TOKEN:
        aInterface.AddException(ParseException(aSpecification));
        break;
      case READONLY_TOKEN:
      case ATTRIBUTE_TOKEN:
        aInterface.AddAttribute(ParseAttribute(aSpecification, token->id));
        break;
      default:
        // it's either a function or an error
        aInterface.AddFunction(ParseFunction(aSpecification, *token));
        break;
    }

    TrimComments();

    // read the final ";"
    if (IID_TOKEN != token->id) {
      token = mScanner->NextToken();
      if (TERMINATOR_TOKEN != token->id) {
        throw InterfaceParsingException("Missing ';'.");
      }
      TrimComments();
    }
  }

  // eat "}"
  mScanner->NextToken();
}

/**
 * XXX NOT IMPLEMENTED YET
 */
IdlTypedef* IdlParser::ParseTypedef(IdlSpecification &aSpecification)
{
  throw NotImplementedException();
  return (IdlTypedef*)0;
}

/**
 * XXX NOT IMPLEMENTED YET
 */
IdlStruct* IdlParser::ParseStruct(IdlSpecification &aSpecification)
{
  throw NotImplementedException();
  return (IdlStruct*)0;
}

/**
 * enum_type      = "enum" identifier  "{" enumerator < "," enumerator > "}" 
 * enumerator     = identifier 
 *
 * We semplify enum's and define them as
 * enum_type      = "enum" identifier  "{" enumerator < "," integer_literal > "}" 
 * enumerator     = identifier 
 */
IdlEnum* IdlParser::ParseEnum(IdlSpecification &aSpecification)
{
  IdlEnum *enumObj = new IdlEnum();

  Token *token;

  // can be an identifier
  token = mScanner->NextToken();
  if (IDENTIFIER_TOKEN == token->id) {
    enumObj->SetName(token->stringID);
    TrimComments();
    token = mScanner->NextToken();
  }
  
  // a "{" must be present anyway
  if (BEGIN_BLOCK_TOKEN == token->id) {

    // untill "}" parse the enum
    token = mScanner->PeekToken();
    while (END_BLOCK_TOKEN != token->id) {

      // must be an identifier
      if (IDENTIFIER_TOKEN == token->id) {
        IdlVariable* enumerator = new IdlVariable();
        enumerator->SetType(TYPE_INT); // it does not really matter which type
        enumerator->SetName(mScanner->NextToken()->stringID);
        enumObj->AddEnumerator(enumerator);
        TrimComments();

        // if "=" is specified an integer must follow
        // NOTE: this is an ad hoc solution for the w3c dom. Enum can
        //       be more complex than this
        token = mScanner->PeekToken();
        if (ASSIGNEMENT_TOKEN == token->id) {
          mScanner->NextToken(); // eat "="
          TrimComments();

          // must be an integer
          token = mScanner->NextToken();
          if (token->id == INTEGER_CONSTANT) {
            enumerator->SetValue(token->value.vLong);
            TrimComments();

            token = mScanner->PeekToken();
          }
          else {
            delete enumObj;
            throw EnumParsingException("The specified enumerator value is not an integer.");
          }
        }
        if (SEPARATOR_TOKEN == token->id) {
          mScanner->NextToken(); // eat ","
          TrimComments();

          token = mScanner->PeekToken();
        }
      }
      else {
        delete enumObj;
        throw EnumParsingException("Missing identifier in enum body.");
      }
    }

    mScanner->NextToken(); // eat "}"
  }
  else {
    delete enumObj;
    throw EnumParsingException("Missing enum body.");
  }

  return enumObj;
}

/**
 * XXX NOT IMPLEMENTED YET
 */
IdlUnion* IdlParser::ParseUnion(IdlSpecification &aSpecification)
{
  throw NotImplementedException();
  return (IdlUnion*)0;
}

/**
 * 
 */
IdlVariable* IdlParser::ParseConst(IdlSpecification &aSpecification)
{
  IdlVariable *constObj = new IdlVariable();
  Token *token;

  TrimComments();
  token = mScanner->NextToken();

  // must be the type
  switch(token->id) {
    // base type
    case BOOLEAN_TOKEN:
      constObj->SetType(TYPE_BOOLEAN);
      break;
    case FLOAT_TOKEN:
      constObj->SetType(TYPE_FLOAT);
      break;
    case DOUBLE_TOKEN:
      constObj->SetType(TYPE_DOUBLE);
      break;
    case LONG_TOKEN:
      constObj->SetType(TYPE_LONG);
      break;
    case SHORT_TOKEN:
      constObj->SetType(TYPE_SHORT);
      break;
    case ULONG_TOKEN:
      constObj->SetType(TYPE_ULONG);
      break;
    case USHORT_TOKEN:
      constObj->SetType(TYPE_USHORT);
      break;
    case CHAR_TOKEN:
      constObj->SetType(TYPE_CHAR);
      break;
    case INT_TOKEN:
      constObj->SetType(TYPE_INT);
      break;
    case UINT_TOKEN:
      constObj->SetType(TYPE_UINT);
      break;
    // string type
    case STRING_TOKEN:
      constObj->SetType(TYPE_STRING);
      break;
    // scoped name
    case IDENTIFIER_TOKEN:
      //if (aSpecification.ContainInterface(aToken.stringID)) {
        constObj->SetType(TYPE_OBJECT);
        constObj->SetTypeName(token->stringID);
        break;
      //}
    default:
      delete constObj;
      throw ConstParsingException("Unknown type.");
  }

  TrimComments();
  token = mScanner->NextToken();

  // an identifier must follow
  if (IDENTIFIER_TOKEN == token->id) {
    constObj->SetName(token->stringID);
  }
  else {
    delete constObj;
    throw ConstParsingException("Missing identifire. Const name undefined.");
  }

  // the "=" sign
  TrimComments();
  token = mScanner->NextToken();
  if (ASSIGNEMENT_TOKEN != token->id) {
    delete constObj;
    throw ConstParsingException("Missing identifire. Const name undefined.");
  }

  TrimComments();
  token = mScanner->NextToken();
  // must be some kind of constant. Constants token ids start at 
  // position INTEGER_CONSTANT
  if (token->id < INTEGER_CONSTANT) {
    delete constObj;
    throw ConstParsingException("Missing identifier. Const name undefined.");
  }
  else if (INTEGER_CONSTANT == token->id) {
    constObj->SetValue(token->value.vLong);
  }
  else if (STRING_CONSTANT == token->id) {
    constObj->SetValue(token->value.vString);
  }

  return constObj;
}

/**
 * XXX NOT IMPLEMENTED YET
 */
IdlException* IdlParser::ParseException(IdlSpecification &aSpecification)
{
  throw NotImplementedException();
  return (IdlException*)0;
}

/**
 * attr_dcl           = [ "readonly" ] "attribute" param_type_spec identifier
 * param_type_spec    = base_type_spec / string_type / scoped_name
 */
IdlAttribute* IdlParser::ParseAttribute(IdlSpecification &aSpecification, int aTokenID)
{
  Token *token;
  int isReadOnly = 0;

  // if it was a readonly keyword read the attribute keyword
  if (READONLY_TOKEN == aTokenID) {
    isReadOnly = 1;
    TrimComments();
    token = mScanner->NextToken();
    aTokenID = token->id;
  }
  if (ATTRIBUTE_TOKEN != aTokenID) {
    throw AttributeParsingException("Missing attribute specifier.");
  }
  TrimComments();

  // create the attribute object
  IdlAttribute *attrObj = new IdlAttribute();
  attrObj->SetReadOnly(isReadOnly);

  // this must be the attribute type
  token = mScanner->NextToken();

  switch(token->id) {
    // base type
    case BOOLEAN_TOKEN:
      attrObj->SetType(TYPE_BOOLEAN);
      break;
    case FLOAT_TOKEN:
      attrObj->SetType(TYPE_FLOAT);
      break;
    case DOUBLE_TOKEN:
      attrObj->SetType(TYPE_DOUBLE);
      break;
    case LONG_TOKEN:
      attrObj->SetType(TYPE_LONG);
      break;
    case SHORT_TOKEN:
      attrObj->SetType(TYPE_SHORT);
      break;
    case ULONG_TOKEN:
      attrObj->SetType(TYPE_ULONG);
      break;
    case USHORT_TOKEN:
      attrObj->SetType(TYPE_USHORT);
      break;
    case CHAR_TOKEN:
      attrObj->SetType(TYPE_CHAR);
      break;
    case INT_TOKEN:
      attrObj->SetType(TYPE_INT);
      break;
    case UINT_TOKEN:
      attrObj->SetType(TYPE_UINT);
      break;
    // string type
    case STRING_TOKEN:
      attrObj->SetType(TYPE_STRING);
      break;
    // scoped name
    case IDENTIFIER_TOKEN:
      //if (aSpecification.ContainInterface(token->stringID)) {
        attrObj->SetType(TYPE_OBJECT);
        attrObj->SetTypeName(token->stringID);
        break;
      //}
    case XPIDL_TOKEN:
      token = mScanner->NextToken();
      if (IDENTIFIER_TOKEN == token->id) {
        attrObj->SetType(TYPE_XPIDL_OBJECT);
        attrObj->SetTypeName(token->stringID);
        break;
      }
    default:
      delete attrObj;
      throw AttributeParsingException("Unknow attribute type.");
  }

  TrimComments();

  // an identifier must follow
  token = mScanner->NextToken();
  if (IDENTIFIER_TOKEN == token->id) {
    attrObj->SetName(token->stringID);
  }
  else {
    delete attrObj;
    throw AttributeParsingException("Missing identifire. Attribute name undefined.");
  }

  return attrObj;
}

IdlFunction* IdlParser::ParseFunction(IdlSpecification &aSpecification, Token &aToken)
{
  // NOTE: we don't process the function specifier "oneway"

  IdlFunction *funcObj = new IdlFunction();

  // a function name starts with the return value
  switch(aToken.id) {
    // base type
    case BOOLEAN_TOKEN:
      funcObj->SetReturnValue(TYPE_BOOLEAN);
      break;
    case FLOAT_TOKEN:
      funcObj->SetReturnValue(TYPE_FLOAT);
      break;
    case DOUBLE_TOKEN:
      funcObj->SetReturnValue(TYPE_DOUBLE);
      break;
    case LONG_TOKEN:
      funcObj->SetReturnValue(TYPE_LONG);
      break;
    case SHORT_TOKEN:
      funcObj->SetReturnValue(TYPE_SHORT);
      break;
    case ULONG_TOKEN:
      funcObj->SetReturnValue(TYPE_ULONG);
      break;
    case USHORT_TOKEN:
      funcObj->SetReturnValue(TYPE_USHORT);
      break;
    case CHAR_TOKEN:
      funcObj->SetReturnValue(TYPE_CHAR);
      break;
    case INT_TOKEN:
      funcObj->SetReturnValue(TYPE_INT);
      break;
    case UINT_TOKEN:
      funcObj->SetReturnValue(TYPE_UINT);
      break;
    // string type
    case STRING_TOKEN:
      funcObj->SetReturnValue(TYPE_STRING);
      break;
    case VOID_TOKEN:
      funcObj->SetReturnValue(TYPE_VOID);
      break;
    // scoped name
    case IDENTIFIER_TOKEN:
      //if (aSpecification.ContainInterface(aToken.stringID)) {
        funcObj->SetReturnValue(TYPE_OBJECT, aToken.stringID);
        break;
      //}
    case XPIDL_TOKEN:
      {
        Token* token = mScanner->NextToken();
        if (IDENTIFIER_TOKEN == token->id) {
          funcObj->SetReturnValue(TYPE_XPIDL_OBJECT, token->stringID);
          break;
        }
      }
    default:
      delete funcObj;
      throw AttributeParsingException("Unknown type.");
  }

  Token *token;
  TrimComments();

  // must be the function name
  token = mScanner->NextToken();
  if (IDENTIFIER_TOKEN == token->id) {
    funcObj->SetName(token->stringID);

    TrimComments();

    // must be "("
    token = mScanner->PeekToken();
    if (FUNC_PARAMS_SPEC_BEGIN_TOKEN == token->id) {

      try {
        int anyOptional = 0;
        while (FUNC_PARAMS_SPEC_END_TOKEN != token->id && 
               ELLIPSIS_TOKEN != token->id &&
                (FUNC_PARAMS_SPEC_BEGIN_TOKEN == token->id || 
                 SEPARATOR_TOKEN == token->id)
               ) {
          mScanner->NextToken(); // eat the last peeked symbol
          TrimComments();

          if ((FUNC_PARAMS_SPEC_END_TOKEN == mScanner->PeekToken()->id) ||
              (ELLIPSIS_TOKEN == mScanner->PeekToken()->id)) {
            break;
          }

          // parse all arguments
          IdlParameter *param = ParseFunctionParameter(aSpecification);
          if (param->GetIsOptional()) {
            anyOptional = 1;
          }
          else if (anyOptional) {
            throw ParameterParsingException("Non-optional parameter cannot follow optional parameters");
          }
          funcObj->AddParameter(param);

          TrimComments();
          token = mScanner->PeekToken();
        }
        if (mScanner->PeekToken()->id == ELLIPSIS_TOKEN) {
          mScanner->NextToken();
          funcObj->SetHasEllipsis(1);
          TrimComments();
        }
        mScanner->NextToken(); // eat ')'
      }
      catch (ParameterParsingException &) {
        delete funcObj;
        throw;
      }

      // is it throwing an exception?
      TrimComments();
      token = mScanner->PeekToken();

      //XXX this is how exceptions are declared now. This should change
      //    to the correct way so that this while loop can be deleted
      while (INTERFACE_TOKEN == token->id) {
        mScanner->NextToken(); // eat "interface"
        TrimComments();
        token = mScanner->NextToken(); // must be the exception name
        if (IDENTIFIER_TOKEN == token->id) {
          funcObj->AddException(token->stringID);
          TrimComments();
          mScanner->NextToken(); // eat '{'
          TrimComments();
          mScanner->NextToken(); // eat '}'
          TrimComments();
          token = mScanner->PeekToken();
          if (SEPARATOR_TOKEN == token->id) {
            mScanner->NextToken(); // eat ','
            TrimComments();
            token = mScanner->PeekToken(); // should be next exception
          }
        }
        else {
          delete funcObj;
          throw FunctionParsingException("Undeclared exception name.");
        }
      }

      if (RAISES_TOKEN == token->id) {
        mScanner->NextToken(); // eat "raises"
        TrimComments();
        token = mScanner->PeekToken(); 
        if (IDENTIFIER_TOKEN == token->id) {
          while (IDENTIFIER_TOKEN == token->id) {
            mScanner->NextToken(); // eat exception name
            funcObj->AddException(token->stringID);
            TrimComments();
            token = mScanner->PeekToken();
            if (SEPARATOR_TOKEN == token->id) {
              mScanner->NextToken(); // eat ','
              TrimComments();
              token = mScanner->PeekToken(); // should be next exception
            }
          }
        }
        else {
          mScanner->NextToken(); // Must be '('
          while (FUNC_PARAMS_SPEC_BEGIN_TOKEN == token->id || 
                 SEPARATOR_TOKEN == token->id) {
            TrimComments();
            token = mScanner->NextToken(); // must be the exception name
            if (IDENTIFIER_TOKEN == token->id) {
              funcObj->AddException(token->stringID);
              TrimComments();
              token = mScanner->NextToken(); // must be ',' or ')'
            }
            else {
              delete funcObj;
              throw FunctionParsingException("Undeclared exception name.");
            }
          }

          if (FUNC_PARAMS_SPEC_END_TOKEN != token->id) {
            delete funcObj;
            throw FunctionParsingException("Missing ')'. Exceptions declaration non terminated.");
          }

          TrimComments();
          token = mScanner->PeekToken();
        }
      }
    }
    else {
      delete funcObj;
      throw FunctionParsingException("Missing '('. Parameters list undefined.");
    }
  }
  else {
    delete funcObj;
    throw FunctionParsingException("Missing identifier. Function name undefined.");
  }

  return funcObj;
}

/**
 * Parse a function argument
 *
 */
IdlParameter* IdlParser::ParseFunctionParameter(IdlSpecification &aSpecification)
{
  IdlParameter *argObj = new IdlParameter();

  Token *token = mScanner->NextToken();
  if (token->id == OPTIONAL_TOKEN) {
    argObj->SetIsOptional(1);
    token = mScanner->NextToken();
  }
  
  // the paramenter attribute (in, out, inout)
  switch(token->id) {
    // base type
    case INPUT_PARAM_TOKEN:
#if defined XP_UNIX || defined XP_MAC
      argObj->SetAttribute(IdlParameter::INPUT);
#else
      argObj->SetAttribute(IdlParameter.INPUT);
#endif
      break;
    case OUTPUT_PARAM_TOKEN:
#if defined XP_UNIX || defined XP_MAC
      argObj->SetAttribute(IdlParameter::OUTPUT);
#else
      argObj->SetAttribute(IdlParameter.OUTPUT);
#endif
      break;
    case INOUT_PARAM_TOKEN:
#if defined XP_UNIX || defined XP_MAC
      argObj->SetAttribute(IdlParameter::INOUT);
#else
      argObj->SetAttribute(IdlParameter.INOUT);
#endif
      break;
    default:
      delete argObj;
      throw ParameterParsingException("Parameter attribute (in, out or inout) undefined.");
  }

  TrimComments();
  token = mScanner->NextToken();

  // must be a known type
  switch(token->id) {
    // base type
    case BOOLEAN_TOKEN:
      argObj->SetType(TYPE_BOOLEAN);
      break;
    case FLOAT_TOKEN:
      argObj->SetType(TYPE_FLOAT);
      break;
    case DOUBLE_TOKEN:
      argObj->SetType(TYPE_DOUBLE);
      break;
    case LONG_TOKEN:
      argObj->SetType(TYPE_LONG);
      break;
    case SHORT_TOKEN:
      argObj->SetType(TYPE_SHORT);
      break;
    case ULONG_TOKEN:
      argObj->SetType(TYPE_ULONG);
      break;
    case USHORT_TOKEN:
      argObj->SetType(TYPE_USHORT);
      break;
    case CHAR_TOKEN:
      argObj->SetType(TYPE_CHAR);
      break;
    case INT_TOKEN:
      argObj->SetType(TYPE_INT);
      break;
    case UINT_TOKEN:
      argObj->SetType(TYPE_UINT);
      break;
    // string type
    case STRING_TOKEN:
      argObj->SetType(TYPE_STRING);
      break;
    // scoped name
    case IDENTIFIER_TOKEN:
      //if (aSpecification.ContainInterface(token->stringID)) {
        argObj->SetType(TYPE_OBJECT);
        argObj->SetTypeName(token->stringID);
        break;
      //}
    case XPIDL_TOKEN:
      token = mScanner->NextToken();
      if (IDENTIFIER_TOKEN == token->id) {
        argObj->SetType(TYPE_XPIDL_OBJECT);
        argObj->SetTypeName(token->stringID);
        break;
      }
    case FUNC_TOKEN:
      token = mScanner->NextToken();
      if (IDENTIFIER_TOKEN == token->id) {
        argObj->SetType(TYPE_FUNC);
        argObj->SetTypeName(token->stringID);
        break;
      }
    default:
      delete argObj;
      throw ParameterParsingException("Unknow type in parameters list.");
  }

  TrimComments();

  // must be the argument name
  token = mScanner->NextToken();
  if (IDENTIFIER_TOKEN == token->id) {
   argObj->SetName(token->stringID);
  }
  else {
    delete argObj;
    throw ParameterParsingException("Missing identifier. Parameter name undefined.");
  }

  return argObj;
}

/**
 * Skip all following comments 
 *
 */
void IdlParser::TrimComments()
{
  while (COMMENT_TOKEN == mScanner->PeekToken()->id) {
    mScanner->NextToken();
  }
}



