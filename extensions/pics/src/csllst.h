
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
/*                                                                       Label data internals
                                   LABEL DATA INTERNALS
                                             
 */
/*
**      (c) COPYRIGHT MIT 1996.
**      Please first read the full copyright statement in the file COPYRIGH.
*/
/*

   This module defines the Label data structures read by CSParser.c. Applications will
   include this if they want direct access to the data (as opposed to using iterator
   methods).
   
   The following data structures relate to the data encapsulated in a PICS Label. Each
   data type correlates to a time in the BNF for the label description. See PICS Labels
   spec for more details.
   
 */
#ifndef CSLLST_H
#define CSLLST

PR_BEGIN_EXTERN_C

/*


LABEL ERROR

   combination of:
   
   _label-error_
   
   _service-error_
   
   _service-info_ 'no-ratings'
   
 */
typedef struct {
    LabelErrorCode_t errorCode;
    HTList * explanations; /* HTList of (char *) */
    } LabelError_t;
/*

EXTENSION DATA

   called _data_ in the BNF
   
 */
typedef struct ExtensionData_s ExtensionData_t;
struct ExtensionData_s {
    char * text;
    PRBool quoted;
    HTList * moreData;
    ExtensionData_t * pParentExtensionData;
    };
/*

EXTENSION

   _option_ 'extension'
   
 */
typedef struct {
    PRBool mandatory;
    SVal_t url;
    HTList * extensionData;
    } Extension_t;
/*

LABEL OPTIONS

   called _option_ in the BNF
   
 */
typedef struct LabelOptions_s LabelOptions_t;
struct LabelOptions_s {
    DVal_t at;
    SVal_t by;
    SVal_t complete_label;
    BVal_t generic;
    SVal_t fur; /* for is a reserved word */
    SVal_t MIC_md5;
    DVal_t on;
    SVal_t signature_PKCS;
    DVal_t until;
    HTList * comments;
    HTList * extensions;
    /* find service-level label options */
    LabelOptions_t * pParentLabelOptions;
    };

/*

RATING

   called _rating_ in the BNF
   
 */
typedef struct {
    SVal_t identifier;
    FVal_t value;
    HTList * ranges;
    } LabelRating_t;
/*

SINGLELABEL

   called _single-label_ in the BNF
   
 */
typedef struct {
    LabelOptions_t * pLabelOptions;
    HTList * labelRatings;
    } SingleLabel_t;

/*

LABEL

   also called _label_
   
 */
typedef struct {
    LabelError_t * pLabelError;
    HTList * singleLabels;
    SingleLabel_t * pSingleLabel;
    } Label_t;
/*

SERVICEINFO

   called _service-info_ in the BNF
   
 */
typedef struct {
    SVal_t rating_service;
    LabelOptions_t * pLabelOptions;
    LabelError_t * pLabelError;
    HTList * labels;
    } ServiceInfo_t;
/*

CSLLDATA

   The whole shebang.
   
 */
struct CSLLData_s {
    FVal_t version;
    LabelError_t * pLabelError;
    HTList * serviceInfos;

    /* some usefull flags */
    PRBool complete;
    PRBool hasTree; /* so it can't make a list of labels */
    int mandatoryExtensions;
    };
/*

   --------------these need the above structures--------------
   
 */
extern CSLLData_t * CSLabel_getCSLLData(CSLabel_t * me);
extern LabelError_t * CSLabel_getLabelError(CSLabel_t * pCSLabel);
extern LabelOptions_t * CSLabel_getLabelOptions(CSLabel_t * pCSLabel);
extern ServiceInfo_t * CSLabel_getServiceInfo(CSLabel_t * pCSLabel);
extern Label_t * CSLabel_getLabel(CSLabel_t * pCSLabel);
extern SingleLabel_t * CSLabel_getSingleLabel(CSLabel_t * pCSLabel);
extern LabelRating_t * CSLabel_getLabelRating(CSLabel_t * pCSLabel);
/*

 */

PR_END_EXTERN_C

#endif /* CSLLST_H */
/*

   End of Declaration */
