
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
/*                                                                     PICS library utilities
                                  PICS LIBRARY UTILITIES
                                             
 */
/*
**      (c) COPYRIGHT MIT 1996.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   This module defines the PICS library interface.
   
 */
#ifndef CSLUTILS_H
#define CSLUTILS_H

/*

 */
#include "htutils.h"
#include "htlist.h"

PR_BEGIN_EXTERN_C
/*

                                PRIMITAVE DATA STRUCTURES
                                             
   BVal_t, FVal_t, SVal_t, DVal_t - hold a boolean, float (not double), string, or date
   value (respectively). These data structures are designed so that they may be
   initialized to all 0s (and hence included directly within larger structures, rather
   than allocated and initialized individually). You must, however, call their clear
   method to deallocate any additional memory used to store the actual value once they
   have been initialized. The following methods are defined on all four data types ("X"
   should be either "B" "F" "S" or "D", XType is "PRBool" "float" "char *" or "char *",
   respectively):
   
      PRBool XVal_readVal(XVal_t, char *), etc. - convert the string to a value of the
      specified type. Returns TRUE on success, FALSE on failure. If successful, may
      allocate additional storage.
      
      PRBool XVal_initialized(XVal_t) - Returns TRUE if the value has been initialized
      (hence contains a legitimate value and may have additional storage allocated
      internally), FALSE otherwise.
      
      XType XVal_value(XVal_t) -- Returns the value stored in the object.
      
      void XVal_clear(XVal_t) -- Mark the object as uninitialized and release any memory
      associated with the value currently stored in the object.
      
BVAL

   - Boolean value.
   
  definition
  
 */
typedef struct {
    enum {BVal_UNINITIALIZED = 0,BVal_YES = 1, BVal_INITIALIZED = 2} state;
    } BVal_t;

extern PRBool BVal_readVal(BVal_t * pBVal, const char * valueStr);
extern PRBool BVal_initialized(const BVal_t * pBVal);
extern PRBool BVal_value(const BVal_t * pBVal);
extern void BVal_clear(BVal_t * pBVal);
/*

  additional methods
  
      void set - assign value
      
 */
extern void BVal_set(BVal_t * pBVal, PRBool value);
/*

FVAL

   - Float value with negative and positive infinity values
   
  definition
  
 */
typedef struct {
    float value;
    enum {FVal_UNINITIALIZED = 0, FVal_VALUE = 1, FVal_NEGATIVE_INF = 2,
          FVal_POSITIVE_INF = 3} stat;
    } FVal_t;

extern PRBool FVal_readVal(FVal_t * pFVal, const char * valueStr);
extern PRBool FVal_initialized(const FVal_t * pFVal);
extern float FVal_value(const FVal_t * pFVal);
extern void FVal_clear(FVal_t * pFVal);
/*

  additional methods
  
      void set - assign a float value
      
      void setInfinite - set to negative or positive infinity
      
      PRBool isZero - see if value is zero
      
      int isInfinite - -1 or 1 for negative or positive infinity
      
      PRBool nearerZero - see if check is nearer zero than check
      
      FVal_t FVal_minus - subtract small from big
      
      char * FVal_toStr - convert to allocated CString, caller must free
      
 */
extern void FVal_set(FVal_t * pFVal, float value);
extern void FVal_setInfinite(FVal_t * pFVal, PRBool negative);
extern PRBool FVal_isZero(const FVal_t * pFVal);
extern int FVal_isInfinite(const FVal_t * pFVal);
extern PRBool FVal_nearerZero(const FVal_t * pRef, const FVal_t * pCheck);
extern FVal_t FVal_minus(const FVal_t * pBig, const FVal_t * pSmall);
extern char * FVal_toStr(FVal_t * pFVal);
/*

  initializers
  
   FVal intializers may be used when creating an FVal
   eg. FVal_t localFVal = FVal_NEGATIVE_INF;
   
 */
#define FVal_NEW_UNINITIALIZED {(float) 0.0, FVal_UNINITIALIZED}
#define FVal_NEW_NEGATIVE_INF {(float) 0.0, FVal_NEGATIVE_INF}
#define FVal_NEW_POSITIVE_INF {(float) 0.0, FVal_POSITIVE_INF}
#define FVal_NEW_ZERO {(float) 0.0, FVal_VALUE}

/*

SVAL

   - String value.
   
  definition
  
 */
typedef struct {
    char * value;
    PRBool initialized;
    } SVal_t;

extern PRBool SVal_readVal(SVal_t * pSVal, const char * valueStr);
extern PRBool SVal_initialized(const SVal_t * pSVal);
extern char * SVal_value(const SVal_t * pSVal);
extern void SVal_clear(SVal_t * pSVal);
/*

DVAL

   - Date value.
   
  definition
  
 */
typedef struct {
    char * value; /* keep the string around for debugging and output */
    PRBool initialized;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int timeZoneHours;
    int timeZoneMinutes;
    } DVal_t;

extern PRBool DVal_readVal(DVal_t * pDVal, const char * valueStr);
extern PRBool DVal_initialized(const DVal_t * pDVal);
extern char * DVal_value(const DVal_t * pDVal);
extern void DVal_clear(DVal_t * pDVal);
/*

  additional methods
  
      int compare - -1 or 1 for a before or after b, 0 for equivilence
      
 */
extern int DVal_compare(const DVal_t * a, const DVal_t * b);
/*

RANGE

   - Range of FVals.
   
  definition
  
 */
typedef struct {
    FVal_t min;
    FVal_t max;
    } Range_t;
/*

  methods
  
      rangeToStr - print range to malloced string. This string must be freed by caller
      
      gap - find the difference between a and b
      
 */
extern char * Range_toStr(Range_t * pRange);
extern FVal_t Range_gap(Range_t * a, Range_t * b);
/*

  initializers
  
 */
#define Range_NEW_UNINITIALIZED {FVal_NEW_UNINITIALIZED, \
                                 FVal_NEW_UNINITIALIZED}

/*

                                          PARSER
                                             
CSPARSE_PARSECHUNK

   CSParse_t - ephemeral parser data, the CSParse structure is defined in CSParse.html.
   CSDoMore_t - tells caller whether parseChunk expects more or encountered an error
   
 */
typedef struct CSParse_s CSParse_t;
typedef enum {CSDoMore_more, CSDoMore_done, CSDoMore_error} CSDoMore_t;
extern CSDoMore_t CSParse_parseChunk (CSParse_t * pCSParse, const char * ptr,
                                      int len, void * pVoid);
/*

                                     PARSE CALLBACKS
                                             
   During parsing, the parser makes callbacks to tell the caller that an error has been
   encountered or that the parser is reading into a new data structure.
   
CSPARSETC

   The TC, or TargetChange, type is a way of itemizing the different targets in a parsable
   object. It is used in the TargetChangeCallback
   
 */
typedef unsigned int CSParseTC_t;


/*

STATERET

 */
typedef enum {StateRet_OK = 0, StateRet_DONE = 1, StateRet_WARN = 0x10,
              StateRet_WARN_NO_MATCH = 0x11, StateRet_WARN_BAD_PUNCT = 0x12,
              StateRet_ERROR = 0x100, StateRet_ERROR_BAD_CHAR = 0x101
} StateRet_t;

/*

TARGETCHANGECALLBACK

   These callbacks keep the caller abreast of what type of object the parser is currently
   reading. TargetChangeCallbacks are made whenever the parser starts or finishes reading
   one of these objects. The actual values of targetChange, and what objects they
   correlate to, can be found in the modules for the object being parsed.
   
      CSLL.html for PICS labels.
      
      CSMR.html for machine-readable service descriptions.
      
      CSUser.html for PICS user profiles.
      
   Example: When reading a CSLabel, the callback will be called with pTargetObject =
   CSLLTC_SERVICE when reading a service, CSLLTC_LABEL when reading a label, etc.
   
 */
typedef struct TargetObject_s TargetObject_t;
typedef StateRet_t TargetChangeCallback_t(CSParse_t * pCSParse,
                                         TargetObject_t * pTargetObject,
                                         CSParseTC_t targetChange, PRBool closed,
                                         void * pVoid);
/*

PARSEERRORHANDLER

 */
typedef StateRet_t ParseErrorHandler_t(CSParse_t * pCSParse,
                                       const char * token,
                                       char demark, StateRet_t errorCode);

/*

CSLIST_ACCEPTLABELS

   get a malloced HTTP Protocol-Request string requesting PICS labels for all services in
   pServiceList
   
 */
typedef enum {CSCompleteness_minimal, CSCompleteness_short,
              CSCompleteness_full, CSCompleteness_signed} CSCompleteness_t;
extern char * CSList_acceptLabels(HTList * pServiceList,
                                  CSCompleteness_t completeness);

/*

CSLIST_GETLABELS

   get a malloced HTTP GET string requesting PICS labels for all services in pServiceList
   
 */
typedef enum {CSOption_generic, CSOption_normal, CSOption_tree,
              CSOption_genericTree} CSOption_t;
extern char * CSList_getLabels(HTList * pServiceList, CSOption_t option,
                               CSCompleteness_t completeness);
/*

CSLIST_POSTLABELS

   get a malloced HTTP GET string requesting PICS labels for all services in pServiceList
   
 */
extern char * CSList_postLabels(HTList * pServiceList, char * url,
                                CSOption_t option,
                                CSCompleteness_t completeness);
/*

INDIVIDUAL PARSERS

CSLABEL

   PICS label list
   
 */
typedef struct CSLabel_s CSLabel_t;
/*

CSUSER

   PICS user profile
   
 */
typedef struct CSUser_s CSUser_t;
/*

CSMACHREAD

   PICS machine readable system description
   
 */
typedef struct CSMachRead_s CSMachRead_t;
/*

   for reading label error codes
   
 */
typedef enum {
    labelError_NA = 0,
    labelError_NO_RATINGS,
    labelError_UNAVAILABLE,
    labelError_DENIED,
    labelError_NOT_LABELED,
    labelError_UNKNOWN
    } LabelErrorCode_t;

/*

   State_Parms - obsolete parameter exchange for iterators
   
 */
typedef struct State_Parms_s State_Parms_t;

typedef enum {
    CSError_OK = 0,
    CSError_YES = 0,
    CSError_NO = 1,
    CSError_BUREAU_NONE,
    CSError_RATING_VALUE,
    CSError_RATING_RANGE,
    CSError_RATING_MISSING,
    CSError_SINGLELABEL_MISSING,
    CSError_LABEL_MISSING,
    CSError_SERVICE_MISSING,
    CSError_CATEGORY_MISSING,
    CSError_ENUM_MISSING,
    CSError_BAD_PARAM,
    CSError_BAD_DATE,
    CSError_SERVICE_NONE,
    CSError_RATING_NONE,
    CSError_APP
    } CSError_t;
/*

 */

PR_END_EXTERN_C

#endif /* CSLUTILS_H */
/*

   End of Declaration */
