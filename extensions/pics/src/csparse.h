
/*  W3 Copyright statement 
Copyright 1995 by: Massachusetts Institute of Technology (MIT), INRIA</H2>

This W3C software is being provided by the copyright holders under the
following license. By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with the
following terms and conditions: 

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee or royalty is hereby
granted, provided that the full text of this NOTICE appears on
<EM>ALL</EM> copies of the software and documentation or portions
thereof, including modifications, that you make. 

<B>THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF EXAMPLE,
BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS.
COPYRIGHT HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE
OR DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used
in advertising or publicity pertaining to the software without
specific, written prior permission.  Title to copyright in this
software and any associated documentation will at all times remain
with copyright holders. 
*/
/*                                                                         Parser for libpics
                                    PARSER FOR LIBPICS
                                             
 */
/*
**      (c) COPYRIGHT MIT 1996.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   This module provides the interface to CSParse.c. The parser is used to parse labels,
   machine-readable descriptions, and users. The application creates one of these and
   iteratevely calls CSParse_parseChunk until it returns a done or an error.
   
 */
#ifndef CSPARSE_H
#define CSPARSE_H
#include "cslutils.h"
#include "htchunk.h"


/*

NOWIN

   tells CSParse where it is in the task of tokenizing
   
 */
typedef enum {
    NowIn_INVALID = 0,
    NowIn_NEEDOPEN,
    NowIn_ENGINE,
    NowIn_NEEDCLOSE,
    NowIn_END,
    NowIn_MATCHCLOSE,
    NowIn_ERROR,
    NowIn_CHAIN
    } NowIn_t;
/*

  Construction/Destruction
  
   The parse objects are never created by the application, but instead by one of the
   objects that it is used to parse.
   
 */
extern CSParse_t * CSParse_new(void);
extern void CSParse_delete(CSParse_t * me);
/*

  some handy definitions
  
 */
#define LPAREN '('
#define RPAREN ')'
#define LCURLY '{'
#define RCURLY '}'
#define LBRACKET '['
#define RBRACKET ']'
#define SQUOTE 0x27 /* avoid confusing parens checking editors */
#define DQUOTE 0x22
#define LPARENSTR "("
#define RPARENSTR ")"
#define raysize(A) (sizeof(A)/sizeof(A[0]))
/*

                                      SUBPARSER DATA
                                             
PUNCT

   valid punctuation
   
 */
typedef enum {Punct_ZERO = 1, Punct_WHITE = 2, Punct_LPAREN = 4,
              Punct_RPAREN = 8, Punct_ALL = 0xf} Punct_t;
/*

SUBSTATE

   Enumerated bits that are used to mark a parsing state. Because they are bits, as
   opposed to sequential numbers, a StateToken may or more than one together and serve
   more than one state. They must have identical outcomes if this is to be exploited.
   
   By convention, the following SubState names are used:
   
   X - has no state
   
   N - is a newly created object
   
   A-H - substate definitions. Because they are non-conflicting bits, a subparser may have
   options that sit in more than state. For instance, the string "error" may be matched in
   states A and C with:
   
   {"error test", SubState_A|SubState_C, Punct_LPAREN, 0, "error"} *probs* I meant to keep
   these 16 bit caompatible, but ran up short at the end of one StateToken list. This can
   be fixed if anyone needs a 16 bit enum.
   
 */
typedef enum {SubState_X = -1, SubState_N = 0x4000, SubState_A = 1,
              SubState_B = 2, SubState_C = 4, SubState_D = 8,
              SubState_E = 0x10, SubState_F = 0x20, SubState_G = 0x40,
              SubState_H = 0x80, SubState_I = 0x100} SubState_t;
/*

   forward declaration for StateToken_t
   
 */
typedef struct StateToken_s StateToken_t;
/*

ENGINE

   called by CSParse to process tokens and punctuation
   
 */
typedef NowIn_t Engine_t(CSParse_t * pCSParse, char demark, void * pVoid);
/*

   Engine employed by the Label, MacRed, and User parsers
   
 */
Engine_t CSParse_targetParser;
/*

SUBSTATE METHODS

   All methods return a StateRet.
   
  Check
  
   see if a value is legitimate, may also record it
   
 */
typedef StateRet_t Check_t(CSParse_t * pCSParse, StateToken_t * pStateToken,
                           char * token, char demark);
/*

   Punctuation checker to be employed by Check_t functions
   
 */
extern PRBool Punct_badDemark(Punct_t validPunctuation, char demark);
/*

  Open
  
   create a new data structure to be filled by the parser
   
 */
typedef StateRet_t Open_t(CSParse_t * pCSParse, char * token, char demark);
/*

  Close
  
   tell the state that the data structure is no longer current
   
 */
typedef StateRet_t Close_t(CSParse_t * pCSParse, char * token, char demark);
/*

  Prep
  
   get ready for next state
   
 */
typedef StateRet_t Prep_t(CSParse_t * pCSParse, char * token, char demark);
/*

  Destroy
  
   something went wrong, throw away the current object
   
 */
typedef void Destroy_t(CSParse_t * pCSParse);
/*

COMMAND

   substate commands
   
   open - call the open function for the current data structure
   
   close - call the close
   
   chain - call again on the next state without re-reading data
   
   notoken - clear the token before a chain (so next state just gets punct)
   
   matchany - match any string
   
 */
typedef enum {Command_NONE = 0, Command_OPEN = 1, Command_CLOSE = 2,
              Command_CHAIN = 4, Command_NOTOKEN = 8,
              Command_MATCHANY = 0x10} Command_t;
/*

STATETOKEN STRUCTURE

   Contains all the information about what tokens are expected in what substates. The
   StateTokens are kept in array referenced by a TargetObject.
   
 */
struct StateToken_s {
    char * note;                /* some usefull text that describes the state - usefulll f
or debugging */
    SubState_t validSubStates;
    Punct_t validPunctuation;
    Check_t * pCheck;   /* call this function to check token */
    char * name1;       /* or compare to this name */
    char * name2;               /* many strings have 2 spellings ("ratings" vs. "r") */
    CSParseTC_t targetChange; /* whether target change implies diving or climbing from cur
rent state */
    TargetObject_t * pNextTargetObject;
    SubState_t nextSubState;
    Command_t command;  /* open, close, chain, etc. */
    Prep_t * pPrep;             /* prepare for next state */
    };
/*

TARGETOBJECT STRUCTURE

   Methods and a lists of StateTokens associated with a data structure. The methods know
   how to read data into current object and the StateTokens tell when to proceed to the
   next object.
   
 */
struct TargetObject_s {
    char * note;
    Open_t * pOpen;   /* call this function to open structure */
    Close_t * pClose;   /* call this function to close structure */
    Destroy_t * pDestroy;
    StateToken_t * stateTokens; /* array of sub states */
    int stateTokenCount;        /* number of sub states */
    CSParseTC_t targetChange; /* target change signal for opening this parse state */
    };
/*

VALTARGET

 */
typedef union {
    BVal_t * pTargetBVal;
    FVal_t * pTargetFVal;
    SVal_t * pTargetSVal;
    DVal_t * pTargetDVal;
    HTList ** pTargetList;
    } ValTarget_t;
/*

VALTYPE

   Write down what value is to be read, and what type it is
   
 */
typedef enum {ValType_NONE, ValType_BVAL, ValType_FVAL,
              ValType_SVAL, ValType_DVAL,
              ValType_COMMENT} ValType_t;
/*

PARSECONTEXT

   Part of a CSParse. The boundry is a litte fuzzy. Maybe it should not exist.
   
 */
typedef struct {
    Engine_t * engineOf;
    TargetChangeCallback_t * pTargetChangeCallback;
    ParseErrorHandler_t * pParseErrorHandler;

    /* for reading [BFSD]Val_t */
    ValTarget_t valTarget;
    ValType_t valType;

    char * pTokenError;

    PRBool observeQuotes; 
    PRBool observedQuotes;
    char * legalChars;
    int legalCharCount;
    } ParseContext_t;
/*

CSPARSE STRUCTURE

   Full parser state and pointer to the object that it is reading.
   
 */
struct CSParse_s {
    char quoteState;
    NowIn_t nowIn;
    HTChunk * token;
    char demark;
    int offset;
    int depth;
    ParseContext_t * pParseContext;
    union { /* all the types this parse engine fills */
        CSMachRead_t * pCSMachRead; /* defined in CSMacRed.c */
        CSLabel_t * pCSLabel; /* defined in CSLabel.c */
        CSUser_t * pCSUser; /* defined in CSUser.c */
        } target;
    TargetObject_t * pTargetObject;
    SubState_t currentSubState;
    StateToken_t * pStateToken;
    };
/*

 */



#endif /* CSPARSE_H */
/*

   End of Declaration */
