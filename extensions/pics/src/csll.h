
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
/*                                                                   Label parser for libpics
                                       LABEL PARSER
                                             
 */
/*
**      (c) COPYRIGHT MIT 1996.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   This module provides the interface to CSLabel.c. Labels are parsed from strings (see
   CSParse.html). These labels may then be kept in a CSLabel_t structure for inspection by
   the application or compared to the values in a CSUser_t structure (see CSUser.html).
   
 */
#ifndef CSLL_H
#define CSLL_H

PR_BEGIN_EXTERN_C

/*

  State Change Enumeration
  
   Call to the TargetChangeCallback will have one of the following values.
   
 */
typedef enum {
        CSLLTC_LIST = 1,
        CSLLTC_SERVICE,
        CSLLTC_LABEL,
        CSLLTC_LABTREE,
        CSLLTC_SINGLE,
        CSLLTC_RATING,
        CSLLTC_RANGE,
        CSLLTC_AWKWARD,
        CSLLTC_NORAT,
        CSLLTC_SRVERR,
        CSLLTC_LABERR,
        CSLLTC_EXTEN,
        CSLLTC_EXTDATA,
        CSLLTC_COUNT
} CSLLTC_t;
/*

  Data shell
  
   All PICS label data is stored in a CSLLData_t
   
 */
typedef struct CSLLData_s CSLLData_t;
#define CSLabel_labelNumber(S) (S->currentLabelNumber)
/*

  TargetChangeCallback
  
   As the label is parsed, it will call the assigned TargetChangeCallback as it passes
   from state to state.
   
 */
typedef StateRet_t LabelTargetCallback_t(CSLabel_t * pCSLabel,
                                         CSParse_t * pCSParse,
                                         CSLLTC_t target, PRBool closed,
                                         void * pVoid);
/*

  ErrorHandler
  
   All parsing error will be passed to the Apps LLErrorHandler for user display or
   automatic dismissal.
   
 */
typedef StateRet_t LLErrorHandler_t(CSLabel_t * pCSLabel,
                                    CSParse_t * pCSParse, const char * token,
                                    char demark, StateRet_t errorCode);
/*

  Construction/Destruction
  
   These methods allow the user to create and get access to both the label and the state.
   CSLabels may be cloned so that one saves state while another continues to iterate or
   parse. The states mus all be freed. Label data will only be freed after all the
   CSLabels that refer to it are deleted.
   
 */
extern CSParse_t * CSParse_newLabel(
                            LabelTargetCallback_t * pLabelTargetCallback,
                            LLErrorHandler_t * pLLErrorHandler);
extern PRBool CSParse_deleteLabel(CSParse_t *);
extern CSLabel_t * CSParse_getLabel(CSParse_t * me);
extern CSLabel_t * CSLabel_copy(CSLabel_t * old);
extern void CSLabel_free(CSLabel_t * me);

extern char * CSLabel_getServiceName(CSLabel_t * pCSLabel);
extern int CSLabel_getLabelNumber(CSLabel_t * pCSLabel);
extern char * CSLabel_getRatingName(CSLabel_t * pCSLabel);
extern char * CSLabel_getRatingStr(CSLabel_t * pCSLabel);
extern Range_t * CSLabel_getLabelRatingRange(CSLabel_t * pCSLabel);
/*

  Iterating methods
  
  Callback function
  
   The Iterators are passed a callback function to be called for each matching element.
   For instance, when iterating through ranges, the callback function is called once for
   each range, or, if a match is requested, only for the matching range.
   
 */
typedef CSError_t CSLabel_callback_t(CSLabel_t *, State_Parms_t *,
                                             const char *, void * pVoid);
typedef CSError_t CSLabel_iterator_t(CSLabel_t *,
                                             CSLabel_callback_t *,
                                             State_Parms_t *, const char *,
                                             void * pVoid);
/*

  Iterators
  
 */
extern CSLabel_iterator_t CSLabel_iterateServices;
extern CSLabel_iterator_t CSLabel_iterateLabels;
extern CSLabel_iterator_t CSLabel_iterateSingleLabels;
extern CSLabel_iterator_t CSLabel_iterateLabelRatings;
/*

  Range Utilities
  
   These funtions allow the application to test the value of a given user parameter
   against those in the label.
   
 */
extern HTList * CSLLData_getAllSingleLabels(CSLabel_t * pCSLabel);
extern FVal_t CSLabel_ratingsIncludeFVal(CSLabel_t * pCSLabel,
                                           FVal_t * userValue);
extern FVal_t CSLabel_ratingsIncludeRange(CSLabel_t * pCSLabel,
                                            Range_t * pUserRange);
extern FVal_t CSLabel_ratingsIncludeRanges(CSLabel_t * pCSLabel,
                                             HTList * userRanges);
/*


 */

PR_END_EXTERN_C

#endif /* CSLL_H */
/*

   End of Declaration */
