
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


#include "plstr.h"
/* #include "sysdep.h"   jhines--7/9/97 */
/*#include "HTUtils.h" */
#include <stddef.h>
#include "htlist.h"
#include "htstring.h"
#include "csparse.h"
#include "csll.h"
#include "csllst.h"

#define GetCSLabel(A) ((A)->target.pCSLabel)
#define SETNEXTSTATE(target, subState) \
  pCSParse->pTargetObject = target; \
  pCSParse->currentSubState = subState;


/* C H A R A C T E R   S E T   V A L I D A T I O N */
/* The BNF for PICS labels describes the valid character ranges for each of 
 * the label fields. Defining NO_CHAR_TEST will disable the tedious checking
 * of these ranges for a slight performance increase.
 */
#ifdef NO_CHAR_TEST
#define charSetOK(A, B, C)
#define CHECK_CAR_SET(A)
#define SET_CHAR_SET(A)
#else /* !NO_CHAR_TEST */
typedef enum {CharSet_ALPHAS = 1, CharSet_DIGITS = 2, CharSet_PLUSMINUS = 4, CharSet_FORSLASH = 8, 
              CharSet_EXTENS = 0x10, CharSet_BASE64_EXTRAS = 0x20, CharSet_DATE_EXTRAS = 0x40, CharSet_URL_EXTRAS = 0x80, 
              /* ------------------ BNF names are combinations of the above ------------------- */
              CharSet_NUMBER        = CharSet_DIGITS | CharSet_PLUSMINUS, 
              CharSet_ALPHANUMPM    = CharSet_ALPHAS | CharSet_DIGITS | CharSet_PLUSMINUS,
              CharSet_TRANSMIT_NAME = CharSet_ALPHANUMPM | CharSet_URL_EXTRAS, 
              CharSet_EXT_ALPHANUM  = CharSet_ALPHANUMPM | CharSet_EXTENS,
              CharSet_BASE64        = CharSet_ALPHAS | CharSet_DIGITS | CharSet_BASE64_EXTRAS,
              CharSet_URL           = CharSet_ALPHAS | CharSet_DIGITS | CharSet_URL_EXTRAS,
              CharSet_DATE          = CharSet_DIGITS | CharSet_DATE_EXTRAS,
              CharSet_EXT_DATA      = CharSet_DATE | CharSet_URL | CharSet_NUMBER | CharSet_EXT_ALPHANUM
             } CharSet_t; 

PRIVATE PRBool charSetOK(CSParse_t * pCSParse, char * checkMe, CharSet_t set);
#define CHECK_CAR_SET(A) \
    if (!charSetOK(pCSParse, token, A)) \
        return StateRet_ERROR_BAD_CHAR;
#define SET_CHAR_SET(A) pCSLabel->targetCharSet = A;
#endif /* !NO_CHAR_TEST */

/* C S L L S t a t e */
/* This holds label list data and the methods to view it. All application 
 * interface is intended to go through these methods. See User/Parsing.html
 */
struct CSLabel_s {
    CSLLData_t * pCSLLData;

    LabelError_t * pCurrentLabelError;
    LabelOptions_t * pCurrentLabelOptions;
	Extension_t * pCurrentExtension;
	ExtensionData_t * pCurrentExtensionData;

    ServiceInfo_t * pCurrentServiceInfo;
    Label_t * pCurrentLabel;
    int currentLabelNumber;
    HTList * pCurrentLabelTree;
    SingleLabel_t * pCurrentSingleLabel;
    LabelRating_t * pCurrentLabelRating;
    Range_t * pCurrentRange;
#ifndef NO_CHAR_TEST
    CharSet_t targetCharSet;
#endif

	LabelTargetCallback_t * pLabelTargetCallback;
	LLErrorHandler_t * pLLErrorHandler;
};

/* forward references to parser functions */
PRIVATE TargetObject_t LabelList_targetObject;
PRIVATE TargetObject_t ServiceInfo_targetObject;
PRIVATE TargetObject_t ServiceNoRat_targetObject;
PRIVATE TargetObject_t ServiceError_targetObject;
PRIVATE TargetObject_t Label_targetObject;
PRIVATE TargetObject_t LabelError_targetObject;
PRIVATE TargetObject_t LabelTree_targetObject;
PRIVATE TargetObject_t SingleLabel_targetObject;
PRIVATE TargetObject_t LabelRating_targetObject;
PRIVATE TargetObject_t LabelRatingRange_targetObject;
PRIVATE TargetObject_t Extension_targetObject;
PRIVATE TargetObject_t ExtensionData_targetObject;
PRIVATE TargetObject_t Awkward_targetObject;
PRIVATE Check_t hasToken;
PRIVATE Check_t LabelList_getVersion;
PRIVATE Check_t ServiceInfo_getServiceId;
PRIVATE Check_t error_getExpl;
PRIVATE Check_t getOption;
PRIVATE Check_t getOptionValue;
PRIVATE Check_t LabelRating_getId;
PRIVATE Check_t LabelRating_getValue;
PRIVATE Check_t LabelRatingRange_get;
PRIVATE Check_t isQuoted;
PRIVATE Check_t Extension_getURL;
PRIVATE Check_t ExtensionData_getData;
PRIVATE Open_t LabelList_open;
PRIVATE Open_t ServiceInfo_open;
PRIVATE Open_t error_open;
PRIVATE Open_t Label_open;
PRIVATE Open_t LabelTree_open;
PRIVATE Open_t SingleLabel_open;
PRIVATE Open_t LabelRating_open;
PRIVATE Open_t LabelRatingRange_open;
PRIVATE Open_t Awkward_open;
PRIVATE Open_t Extension_open;
PRIVATE Open_t ExtensionData_open;
PRIVATE Close_t LabelList_close;
PRIVATE Close_t ServiceInfo_close;
PRIVATE Close_t error_close;
PRIVATE Close_t Label_close;
PRIVATE Close_t LabelTree_close;
PRIVATE Close_t SingleLabel_close;
PRIVATE Close_t LabelRating_close;
PRIVATE Close_t LabelRatingRange_close;
PRIVATE Close_t Awkward_close;
PRIVATE Close_t Extension_close;
PRIVATE Close_t ExtensionData_close;
PRIVATE Destroy_t LabelList_destroy;
PRIVATE Destroy_t ServiceInfo_destroy;
PRIVATE Destroy_t Label_destroy;
PRIVATE Destroy_t LabelTree_destroy;
PRIVATE Destroy_t SingleLabel_destroy;
PRIVATE Destroy_t LabelRating_destroy;
PRIVATE Destroy_t LabelRatingRange_destroy;
PRIVATE Destroy_t Awkward_destroy;
PRIVATE Destroy_t error_destroy;
PRIVATE Destroy_t Extension_destroy;
PRIVATE Destroy_t ExtensionData_destroy;
PRIVATE Prep_t ServiceInfo_clearOpts;
PRIVATE Prep_t LabelRating_next;
PRIVATE Prep_t Extension_mandatory;
PRIVATE Prep_t Extension_next;
PRIVATE Prep_t ExtensionData_next;
PRIVATE Prep_t SingleLabel_doClose;
PRIVATE Prep_t Label_doClose;

PRIVATE TargetChangeCallback_t targetChangeCallback;
PRIVATE ParseErrorHandler_t parseErrorHandler;

/* CSParse_doc states */
/* L A B E L   L I S T   P A R S E R   S T A T E S */
/* This contains all the states in the BNF for PICS labels. 
 * See User/Parsing.html for details.
 */
PRIVATE StateToken_t LabelList_stateTokens[] = { 
    /* A: fresh LabelList
       C: expect end */
     {       "open", SubState_N,    Punct_ALL,              0,        0, 0, 0,   &LabelList_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {"get version", SubState_A,  Punct_WHITE, &LabelList_getVersion, 0, 0, 0, &ServiceInfo_targetObject, SubState_N, Command_NONE, 0},
     {"end of list", SubState_C, Punct_RPAREN,              0,        0, 0, 0,   &LabelList_targetObject, SubState_A, Command_MATCHANY|Command_CLOSE, 0}
    };

PRIVATE StateToken_t ServiceInfo_stateTokens[] = {
    /* A: fresh ServiceInfo
       B: has service id
       C: needs option value
       D: call from Awkward or NoRat to close 
       E: call from Awkward to re-enter
       F: call from Awkward to handle no-ratings error */
     {             "open", SubState_N,    Punct_ALL,                0,        0,   0, 0, &ServiceInfo_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {     "error w/o id", SubState_A, Punct_LPAREN,                0, "error",    0, 0, &ServiceNoRat_targetObject, SubState_N, Command_NONE, 0},
     {       "service id", SubState_A,  Punct_WHITE, &ServiceInfo_getServiceId, 0, 0, 0,  &ServiceInfo_targetObject, SubState_B, Command_NONE, 0},
     {    "service error", SubState_B, Punct_LPAREN,                0, "error",    0, 0, &ServiceError_targetObject, SubState_N, Command_NONE, 0},
     {   "service option", SubState_B,  Punct_WHITE,       &getOption,   0,        0, 0,  &ServiceInfo_targetObject, SubState_C, Command_NONE, 0},
     {"service extension", SubState_B, Punct_LPAREN,               0, "extension", 0, 0,    &Extension_targetObject, SubState_N, Command_NONE, 0},
     { "label-mark close", SubState_B, Punct_RPAREN,                0, "l", "labels", 0, &LabelList_targetObject, SubState_C, Command_CLOSE|Command_CHAIN, 0},
     {       "label-mark", SubState_B, Punct_WHITE|Punct_LPAREN,    0, "l", "labels", 0,     &Label_targetObject, SubState_N, Command_CHAIN, &ServiceInfo_clearOpts},
     {     "option value", SubState_C, Punct_WHITE,    &getOptionValue,  0,        0, 0,  &ServiceInfo_targetObject, SubState_B, Command_NONE, 0},

     {            "close", SubState_D, Punct_ALL,                   0,        0,   0, 0,    &LabelList_targetObject, SubState_C, Command_MATCHANY|Command_CLOSE|Command_CHAIN, 0},
     {         "re-enter", SubState_E, Punct_ALL,                   0,        0,   0, 0,  &ServiceInfo_targetObject, SubState_N, Command_MATCHANY|Command_CLOSE|Command_CHAIN, 0},
     {        "to no-rat", SubState_F, Punct_ALL,                   0,        0,   0, 0,  &ServiceInfo_targetObject, SubState_G, Command_MATCHANY|Command_CLOSE|Command_CHAIN, 0},
     {    "no-rat opener", SubState_G, Punct_ALL,                   0,        0,   0, 0, &ServiceNoRat_targetObject, SubState_N, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0}
    };

PRIVATE StateToken_t Label_stateTokens[] = {
    /* A: fresh SingleLabel
       C: route to Awkward from LabelTree and LabelError
       D: from Awkward to LabelError */
     {              "open", SubState_N,    Punct_ALL, 0,        0,    0,  0, &Label_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     { "single label mark", SubState_A,  Punct_WHITE, 0,  "l", "labels",  0,       &Label_targetObject, SubState_A, Command_NONE, 0}, /* stick around */
     {   "tree label mark", SubState_A, Punct_LPAREN, 0,  "l", "labels",  0,   &LabelTree_targetObject, SubState_N, Command_NONE, 0},
     {        "start tree", SubState_A, Punct_LPAREN, 0,         0,   0,  0,   &LabelTree_targetObject, SubState_N, Command_NONE, 0},
     {       "label error", SubState_A, Punct_LPAREN, 0,   "error",   0,  0,  &LabelError_targetObject, SubState_N, Command_NONE, 0},
     {"SingleLabel option", SubState_A, Punct_WHITE, &getOption, 0,   0,  0, &SingleLabel_targetObject, SubState_N, Command_CHAIN, 0},
     {   "label extension", SubState_A, Punct_LPAREN, 0, "extension", 0,  0, &SingleLabel_targetObject, SubState_N, Command_CHAIN, 0},
     {           "ratings", SubState_A, Punct_LPAREN, 0, "r", "ratings",  0, &SingleLabel_targetObject, SubState_N, Command_CHAIN, 0},

     {        "to awkward", SubState_C,    Punct_ALL, 0,        0,    0,  0,     &Awkward_targetObject, SubState_A, Command_MATCHANY|Command_CLOSE, 0},
     {  "awkward to error", SubState_D,    Punct_ALL, 0,        0,    0,  0, &LabelError_targetObject, SubState_N, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0}
    };

PRIVATE StateToken_t LabelTree_stateTokens[] = {
    /* A: LabelTrees have no state */
     {              "open", SubState_N, Punct_ALL,       0,        0, 0, 0, &LabelTree_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {       "label error", SubState_A, Punct_LPAREN, 0, "error", 0,     0, &LabelError_targetObject, SubState_N, Command_NONE, 0},
     {"SingleLabel option", SubState_A, Punct_WHITE, &getOption, 0,   0, 0, &SingleLabel_targetObject, SubState_N, Command_CHAIN, 0},
     {        "ratingword", SubState_A, Punct_LPAREN, 0, "r", "ratings", 0, &SingleLabel_targetObject, SubState_N, Command_CHAIN, 0},
     {       "end of tree", SubState_A, Punct_RPAREN,     0,       0, 0, 0, &Label_targetObject, SubState_C, Command_CLOSE|Command_CHAIN, 0}
    };

PRIVATE StateToken_t SingleLabel_stateTokens[] = {
    /* A: fresh SingleLabel
       B: needs option value */
     {           "open", SubState_N, Punct_ALL, 0,                  0, 0,  0, &SingleLabel_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {"label extension", SubState_A, Punct_LPAREN,     0, "extension", 0,  0,   &Extension_targetObject, SubState_N, Command_NONE, 0},
     {   "label option", SubState_A,  Punct_WHITE, &getOption,      0, 0,  0, &SingleLabel_targetObject, SubState_B, Command_NONE, 0},
     {     "ratingword", SubState_A, Punct_LPAREN,     0, "r", "ratings",  0, &LabelRating_targetObject, SubState_N, Command_NONE, 0},
     {   "option value", SubState_B,  Punct_WHITE, &getOptionValue, 0, 0,  0, &SingleLabel_targetObject, SubState_A, Command_CLOSE, 0}
    };

PRIVATE StateToken_t LabelRating_stateTokens[] = {
    /* A: looking for transmit name
       B: looking for value
       C: return from range (either creates a new rating or ends)
       D: close and re-open */
     {             "open", SubState_N,    Punct_ALL,              0,        0, 0, 0, &LabelRating_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {  "id before value", SubState_A,  Punct_WHITE,    &LabelRating_getId, 0, 0, 0,      &LabelRating_targetObject, SubState_B, Command_NONE, 0},
     {  "id before range", SubState_A, Punct_LPAREN,    &LabelRating_getId, 0, 0, 0, &LabelRatingRange_targetObject, SubState_N, Command_NONE, 0},
     {       "value next", SubState_B,  Punct_WHITE, &LabelRating_getValue, 0, 0, 0,      &LabelRating_targetObject, SubState_D, Command_NONE, 0}, /* opener must close last rating first */
     {      "value close", SubState_B, Punct_RPAREN, &LabelRating_getValue, 0, 0, 0, 0, SubState_X, Command_CLOSE, &LabelRating_next},
     {            "close", SubState_C, Punct_RPAREN,                     0, 0, 0, 0, 0, SubState_X, Command_CLOSE, &LabelRating_next},
     {"value after range", SubState_C, Punct_WHITE|Punct_LPAREN, &hasToken, 0, 0, 0, &LabelRating_targetObject, SubState_D, Command_CHAIN, 0}, /* opener must close last rating first */

     {         "re-enter", SubState_D, Punct_ALL,                 0,        0, 0, 0, &LabelRating_targetObject, SubState_N, Command_MATCHANY|Command_CLOSE|Command_CHAIN, 0}
    };

PRIVATE StateToken_t LabelRatingRange_stateTokens[] = {
     {       "open", SubState_N,    Punct_ALL,              0,        0, 0, 0, &LabelRatingRange_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     { "range data", SubState_A,  Punct_WHITE, &LabelRatingRange_get, 0, 0, 0, &LabelRatingRange_targetObject, SubState_A, Command_NONE, 0},
     {"range close", SubState_A, Punct_RPAREN, &LabelRatingRange_get, 0, 0, 0,      &LabelRating_targetObject, SubState_C, Command_CLOSE, 0}
    };

/* Awkward assumes that the current Label has been closed. It decides whether to chain to LabelTree, Label, or ServiceInfo */
PRIVATE StateToken_t Awkward_stateTokens[] = {
     {           "open", SubState_N,    Punct_ALL, 0,          0,  0, 0, &Awkward_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {     "start tree", SubState_A, Punct_LPAREN,          0, 0,  0, 0,               &LabelTree_targetObject, SubState_N, Command_NONE, 0},
     {    "label error", SubState_A, Punct_LPAREN, 0,    "error",  0, 0,                 &Awkward_targetObject, SubState_B, Command_NONE, 0},
     {   "label option", SubState_A,  Punct_WHITE, &getOption, 0,  0, 0,       &Label_targetObject, SubState_N, Command_CHAIN, 0},
     {"label extension", SubState_A, Punct_LPAREN, 0, "extension", 0, 0,       &Label_targetObject, SubState_N, Command_CHAIN, 0},
     {         "rating", SubState_A, Punct_LPAREN, 0, "r", "ratings", 0,       &Label_targetObject, SubState_N, Command_CHAIN, 0},
     { "new service id", SubState_A,  Punct_WHITE,  &isQuoted, 0,  0, 0, &ServiceInfo_targetObject, SubState_E, Command_CHAIN, 0},
     {          "close", SubState_A, Punct_RPAREN,          0, 0,  0, 0, &ServiceInfo_targetObject, SubState_D, Command_CHAIN, 0}, /* close of LabelList */

     {       "req-denied", SubState_B,  Punct_WHITE, 0, "request-denied", 0, 0, &Label_targetObject, SubState_D, Command_CHAIN, 0},
     { "req-denied close", SubState_B, Punct_RPAREN, 0, "request-denied", 0, 0, &Label_targetObject, SubState_D, Command_CHAIN, 0},
     {      "not-labeled", SubState_B,  Punct_WHITE, 0,    "not-labeled", 0, 0, &Label_targetObject, SubState_D, Command_CHAIN, 0},
     {"not-labeled close", SubState_B, Punct_RPAREN, 0,    "not-labeled", 0, 0, &Label_targetObject, SubState_D, Command_CHAIN, 0},
     {       "no-ratings", SubState_B,  Punct_WHITE, 0, "no-ratings", 0, 0, &ServiceInfo_targetObject, SubState_F, Command_CHAIN, 0},
     { "no-ratings close", SubState_B, Punct_RPAREN, 0, "no-ratings", 0, 0, &ServiceInfo_targetObject, SubState_F, Command_CHAIN, 0}
    };

/* error parsing states */
PRIVATE StateToken_t ServiceNoRat_stateTokens[] = {
     {             "open", SubState_N,    Punct_ALL,   0,            0, 0, 0, &ServiceNoRat_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {       "no-ratings", SubState_A,  Punct_WHITE,   0, "no-ratings", 0, 0, &ServiceNoRat_targetObject, SubState_B, Command_NONE, 0},
     { "no-ratings close", SubState_A, Punct_RPAREN,   0, "no-ratings", 0, 0,    &ServiceInfo_targetObject, SubState_D, Command_CLOSE|Command_CHAIN, 0},
     {      "explanation", SubState_B,  Punct_WHITE, &error_getExpl, 0, 0, 0, &ServiceNoRat_targetObject, SubState_B, Command_NONE, 0},
     {"explanation close", SubState_B, Punct_RPAREN, &error_getExpl, 0, 0, 0,    &ServiceInfo_targetObject, SubState_D, Command_CLOSE|Command_CHAIN, 0}
    };

PRIVATE StateToken_t ServiceError_stateTokens[] = {
     {                 "open", SubState_N,    Punct_ALL, 0,                    0, 0, 0, &ServiceError_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {           "req-denied", SubState_A,  Punct_WHITE, 0,     "request-denied", 0, 0, &ServiceError_targetObject, SubState_B, Command_NONE, 0},
     {     "req-denied close", SubState_A, Punct_RPAREN, 0,     "request-denied", 0, 0,    &ServiceInfo_targetObject, SubState_D, Command_CLOSE|Command_CHAIN, 0},
     {      "service-unavail", SubState_A,  Punct_WHITE, 0,"service-unavailable", 0, 0, &ServiceError_targetObject, SubState_B, Command_NONE, 0},
     {"service-unavail close", SubState_A, Punct_RPAREN, 0,"service-unavailable", 0, 0,    &ServiceInfo_targetObject, SubState_D, Command_CLOSE|Command_CHAIN, 0},
     {          "explanation", SubState_B,  Punct_WHITE, &error_getExpl,       0, 0, 0, &ServiceError_targetObject, SubState_B, Command_NONE, 0},
     {    "explanation close", SubState_B, Punct_RPAREN, &error_getExpl,       0, 0, 0,    &ServiceInfo_targetObject, SubState_D, Command_CLOSE|Command_CHAIN, 0}
    };

PRIVATE StateToken_t LabelError_stateTokens[] = {
     {             "open", SubState_N,    Punct_ALL, 0,                0, 0, 0, &LabelError_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {       "req-denied", SubState_A,  Punct_WHITE, 0, "request-denied", 0, 0, &LabelError_targetObject, SubState_B, Command_NONE, 0},
     { "req-denied close", SubState_A, Punct_RPAREN, 0, "request-denied", 0, 0,    &Label_targetObject, SubState_C, Command_CLOSE|Command_CHAIN, 0},
     {      "not-labeled", SubState_A,  Punct_WHITE, 0,    "not-labeled", 0, 0, &LabelError_targetObject, SubState_B, Command_NONE, 0},
     {"not-labeled close", SubState_A, Punct_RPAREN, 0,    "not-labeled", 0, 0,    &Label_targetObject, SubState_C, Command_CLOSE|Command_CHAIN, 0},
     {      "explanation", SubState_B,  Punct_WHITE, &error_getExpl,   0, 0, 0, &LabelError_targetObject, SubState_B, Command_NONE, 0},
     {"explanation close", SubState_B, Punct_RPAREN, &error_getExpl,   0, 0, 0,    &Label_targetObject, SubState_C, Command_CLOSE|Command_CHAIN, 0}
    };

PRIVATE StateToken_t Extension_stateTokens[] = {
    /* A: looking for mand/opt
       B: looking for URL
       C: back from ExtensionData */
     {      "open", SubState_N,    Punct_ALL,        0,           0, 0, 0, &Extension_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
	 { "mandatory", SubState_A,  Punct_WHITE,        0, "mandatory", 0, 0, &Extension_targetObject, SubState_B, Command_NONE, &Extension_mandatory},
     {  "optional", SubState_A,  Punct_WHITE,        0,  "optional", 0, 0, &Extension_targetObject, SubState_B, Command_NONE, 0},
     {       "URL", SubState_B,  Punct_WHITE,  &Extension_getURL, 0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_NONE, 0},
     {  "URL open", SubState_B, Punct_LPAREN,  &Extension_getURL, 0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_CHAIN|Command_NOTOKEN, 0},
     { "URL close", SubState_B, Punct_RPAREN,  &Extension_getURL, 0, 0, 0, 0, SubState_X, Command_CLOSE, &Extension_next},

     { "more data", SubState_C, Punct_WHITE|Punct_LPAREN|Punct_RPAREN, &hasToken, 0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_CHAIN, 0},
     {      "nest", SubState_C, Punct_LPAREN,           0,         0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_CHAIN, 0},
     {     "close", SubState_C, Punct_RPAREN,           0,         0, 0, 0, 0, SubState_X, Command_CLOSE, &Extension_next}
    };

PRIVATE StateToken_t ExtensionData_stateTokens[] = {
    /* A: looking for data 
       B: back from recursive ExtensionData (identical to Extension B) */
     {      "open", SubState_N,    Punct_ALL,               0,        0, 0, 0, &ExtensionData_targetObject, SubState_A, Command_MATCHANY|Command_OPEN|Command_CHAIN, 0},
     {    "lparen", SubState_A, Punct_LPAREN,                      0, 0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_NONE, 0},
     {     "close", SubState_A, Punct_RPAREN,                      0, 0, 0, 0, 0, SubState_X, Command_CLOSE, &ExtensionData_next},
	 {      "data", SubState_A, Punct_WHITE, &ExtensionData_getData, 0, 0, 0, 0, SubState_X, Command_CLOSE, &ExtensionData_next},
	 {"data punct", SubState_A, Punct_LPAREN|Punct_RPAREN, &ExtensionData_getData, 0, 0, 0, 0, SubState_X, Command_CLOSE|Command_CHAIN|Command_NOTOKEN, &ExtensionData_next},

     { "more data", SubState_B, Punct_WHITE|Punct_LPAREN|Punct_RPAREN, &hasToken, 0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_CHAIN, 0},
     {      "nest", SubState_B, Punct_LPAREN,           0,         0, 0, 0, &ExtensionData_targetObject, SubState_N, Command_CHAIN, 0},
	 {     "close", SubState_B, Punct_RPAREN,           0,         0, 0, 0, 0, SubState_X, Command_CLOSE, &ExtensionData_next}
    };


PRIVATE void init_target_obj(TargetObject_t *obj, char *note, Open_t *pOpen, Close_t *pClose, Destroy_t *pDestroy, StateToken_t *stateToken, int stateTokenCount, CSParseTC_t targetChange)
{
    obj->note = note;
    obj->pOpen = pOpen;
    obj->pClose = pClose;
    obj->pDestroy = pDestroy;
    obj->stateTokens = stateToken;
    obj->stateTokenCount = stateTokenCount;
    obj->targetChange = targetChange;
}

PRIVATE void CSinitialize_global_data(void)
{
	static PRBool first_time=TRUE;
	
	if(first_time)
	{
		first_time = FALSE;
		
		init_target_obj(&LabelList_targetObject, "LabelList", &LabelList_open, &LabelList_close, &LabelList_destroy, LabelList_stateTokens, raysize(LabelList_stateTokens), CSLLTC_LIST);
		init_target_obj(&ServiceInfo_targetObject, "ServiceInfo", ServiceInfo_open, &ServiceInfo_close, &ServiceInfo_destroy, ServiceInfo_stateTokens, raysize(ServiceInfo_stateTokens), CSLLTC_SERVICE);
		init_target_obj(&Label_targetObject, "Label", &Label_open, &Label_close, &Label_destroy, Label_stateTokens, raysize(Label_stateTokens), CSLLTC_LABEL);
		init_target_obj(&LabelTree_targetObject, "LabelTree", &LabelTree_open, &LabelTree_close, &LabelTree_destroy, LabelTree_stateTokens, raysize(LabelTree_stateTokens), CSLLTC_LABTREE);
		init_target_obj(&SingleLabel_targetObject, "SingleLabel", &SingleLabel_open, &SingleLabel_close, &SingleLabel_destroy, SingleLabel_stateTokens, raysize(SingleLabel_stateTokens), CSLLTC_SINGLE);
		init_target_obj(&LabelRating_targetObject, "LabelRating", &LabelRating_open, &LabelRating_close, &LabelRating_destroy, LabelRating_stateTokens, raysize(LabelRating_stateTokens), CSLLTC_RATING);
		init_target_obj(&LabelRatingRange_targetObject, "LabelRatingRange", &LabelRatingRange_open, &LabelRatingRange_close, &LabelRatingRange_destroy, LabelRatingRange_stateTokens, raysize(LabelRatingRange_stateTokens), CSLLTC_RANGE);
		init_target_obj(&Awkward_targetObject, "Awkward", &Awkward_open, &Awkward_close, &Awkward_destroy, Awkward_stateTokens, raysize(Awkward_stateTokens), 0);
		init_target_obj(&ServiceNoRat_targetObject, "ServiceNoRat", &error_open, &error_close, &error_destroy, ServiceNoRat_stateTokens, raysize(ServiceNoRat_stateTokens), CSLLTC_NORAT);
		init_target_obj(&LabelError_targetObject, "LabelError", &error_open, &error_close, &error_destroy, LabelError_stateTokens, raysize(LabelError_stateTokens), CSLLTC_LABERR);
		init_target_obj(&ServiceError_targetObject, "ServiceError", &error_open, &error_close, &error_destroy, ServiceError_stateTokens, raysize(ServiceError_stateTokens), CSLLTC_SRVERR);
		init_target_obj(&Extension_targetObject, "Extension", &Extension_open, &Extension_close, &Extension_destroy, Extension_stateTokens, raysize(Extension_stateTokens), CSLLTC_EXTEN);
		init_target_obj(&ExtensionData_targetObject, "ExtensionData", &ExtensionData_open, &ExtensionData_close, &ExtensionData_destroy, ExtensionData_stateTokens, raysize(ExtensionData_stateTokens), CSLLTC_EXTDATA);
	}
}

/* CSParse_doc end */
/* S T A T E   A S S O C I A T I O N - associate a CSLabel with the label list data 
   The label list data is kept around until all states referencing it are destroyed */
typedef struct {
    CSLabel_t * pCSLabel;
    CSLLData_t * pCSLLData;
    } CSLabelAssoc_t;

PRIVATE HTList * CSLabelAssocs = 0;

PRIVATE void CSLabelAssoc_add(CSLabel_t * pCSLabel, CSLLData_t * pCSLLData)
{
    CSLabelAssoc_t * pElement;
    if ((pElement = (CSLabelAssoc_t *) HT_CALLOC(1, sizeof(CSLabelAssoc_t))) == NULL)
        HT_OUTOFMEM("CSLabelAssoc_t");
    pElement->pCSLabel = pCSLabel;
    pElement->pCSLLData = pCSLLData;
    if (!CSLabelAssocs)
        CSLabelAssocs = HTList_new();
    HTList_appendObject(CSLabelAssocs, (void *)pElement);
}

PRIVATE CSLabelAssoc_t * CSLabelAssoc_findByData(CSLLData_t * pCSLLData)
{
    HTList * assocs = CSLabelAssocs;
    CSLabelAssoc_t * pElement;
    while ((pElement = (CSLabelAssoc_t *) HTList_nextObject(assocs)) != NULL)
        if (pElement->pCSLLData == pCSLLData)
            return pElement;
    return 0;
}

PRIVATE CSLabelAssoc_t * CSLabelAssoc_findByState(CSLabel_t * pCSLabel)
{
    HTList * assocs = CSLabelAssocs;
    CSLabelAssoc_t * pElement;
    while ((pElement = (CSLabelAssoc_t *) HTList_nextObject(assocs)) != NULL)
        if (pElement->pCSLabel == pCSLabel)
            return pElement;
    return 0;
}

PRIVATE void CSLabelAssoc_removeByState(CSLabel_t * pCSLabel)
{
    CSLabelAssoc_t * pElement = CSLabelAssoc_findByState(pCSLabel);
    if (!pElement)
        return;
    HTList_removeObject(CSLabelAssocs, (void *)pElement);
   HT_FREE(pElement);
}

/* P R I V A T E   C O N S T R U C T O R S / D E S T R U C T O R S */
/* These serve the public constructors 
 */
PRIVATE LabelError_t * LabelError_new(void)
{
    LabelError_t * me;
    if ((me = (LabelError_t *) HT_CALLOC(1, sizeof(LabelError_t))) == NULL)
        HT_OUTOFMEM("LabelError_t");
    me->explanations = HTList_new();
    return me;
}

PRIVATE void LabelError_free(LabelError_t * me)
{
    char * explanation;
    if (!me)
        return;
    while ((explanation = (char *) HTList_removeLastObject(me->explanations)) != NULL)
        HT_FREE(explanation);
    HT_FREE(me);
}

PRIVATE LabelOptions_t * LabelOptions_new(LabelOptions_t * pParentLabelOptions)
{
    LabelOptions_t * me;
    if ((me = (LabelOptions_t *) HT_CALLOC(1, sizeof(LabelOptions_t))) == NULL)
        HT_OUTOFMEM("LabelOptions_t");
    me->pParentLabelOptions = pParentLabelOptions;
    return me;
}

PRIVATE void LabelOptions_free(LabelOptions_t * me)
{
	char * comment;
    DVal_clear(&me->at);
    SVal_clear(&me->by);
    SVal_clear(&me->complete_label);
    BVal_clear(&me->generic);
    SVal_clear(&me->fur);
    SVal_clear(&me->MIC_md5);
    DVal_clear(&me->on);
    SVal_clear(&me->signature_PKCS);
    DVal_clear(&me->until);
    while ((comment = HTList_removeLastObject(me->comments)) != NULL)
		HT_FREE(comment);
    HT_FREE(me);
}

PRIVATE ExtensionData_t * ExtensionData_new(void)
{
    ExtensionData_t * me;
    if ((me = (ExtensionData_t *) HT_CALLOC(1, sizeof(ExtensionData_t))) == NULL)
        HT_OUTOFMEM("ExtensionData_t");
    return me;
}

PRIVATE void ExtensionData_free(ExtensionData_t * me)
{
    ExtensionData_t * pExtensionData;
    while ((pExtensionData = (ExtensionData_t *) HTList_removeLastObject(me->moreData)) != NULL)
        ExtensionData_free(pExtensionData);
    HT_FREE(me->text);
    HT_FREE(me);
}

PRIVATE Extension_t * Extension_new(void)
{
    Extension_t * me;
    if ((me = (Extension_t *) HT_CALLOC(1, sizeof(Extension_t))) == NULL)
        HT_OUTOFMEM("Extension_t");
    return me;
}

PRIVATE void Extension_free(Extension_t * me)
{
    ExtensionData_t * pExtensionData;
    while ((pExtensionData = (ExtensionData_t *) HTList_removeLastObject(me->extensionData)) != NULL)
        ExtensionData_free(pExtensionData);
    SVal_clear(&me->url);
    HT_FREE(me);
}

PRIVATE LabelRating_t * LabelRating_new(void)
{
    LabelRating_t * me;
    if ((me = (LabelRating_t *) HT_CALLOC(1, sizeof(LabelRating_t))) == NULL)
        HT_OUTOFMEM("LabelRating_t");
/*  don't initialize HTList me->ranges as it may be just a value */
    return me;
}

PRIVATE void LabelRating_free(LabelRating_t * me)
{
    Range_t * pRange;
    while ((pRange = (Range_t *) HTList_removeLastObject(me->ranges)) != NULL)
        HT_FREE(pRange);
    SVal_clear(&me->identifier);
    HT_FREE(me);
}

PRIVATE SingleLabel_t * SingleLabel_new(LabelOptions_t * pLabelOptions, LabelOptions_t * pParentLabelOptions)
{
    SingleLabel_t * me;
    if ((me = (SingleLabel_t *) HT_CALLOC(1, sizeof(SingleLabel_t))) == NULL)
        HT_OUTOFMEM("SingleLabel_t");
    me->labelRatings = HTList_new();
    me->pLabelOptions = pLabelOptions ? pLabelOptions : LabelOptions_new(pParentLabelOptions);
    return me;
}

PRIVATE void SingleLabel_free(SingleLabel_t * me)
{
    LabelRating_t * pLabelRating;
    while ((pLabelRating = (LabelRating_t *) HTList_removeLastObject(me->labelRatings)) != NULL)
        LabelRating_free(pLabelRating);
    LabelOptions_free(me->pLabelOptions);
    HT_FREE(me);
}

PRIVATE Label_t * Label_new(void)
{
    Label_t * me;
    if ((me = (Label_t *) HT_CALLOC(1, sizeof(Label_t))) == NULL)
        HT_OUTOFMEM("Label_t");
    /* dont initialize HTList me->singleLabels */
    return me;
}

PRIVATE void Label_free(Label_t * me)
{
    SingleLabel_t * pSingleLabel;
    if (me->pSingleLabel)
        SingleLabel_free(me->pSingleLabel);
    else    /* if both of these are (erroneously) defined, mem checkers will pick it up */
        while ((pSingleLabel = (SingleLabel_t *) HTList_removeLastObject(me->singleLabels)) != NULL)
            SingleLabel_free(pSingleLabel);
    LabelError_free(me->pLabelError);
    HT_FREE(me);
}

PRIVATE ServiceInfo_t * ServiceInfo_new()
{
    ServiceInfo_t * me;
    if ((me = (ServiceInfo_t *) HT_CALLOC(1, sizeof(ServiceInfo_t))) == NULL)
        HT_OUTOFMEM("ServiceInfo_t");
    me->labels = HTList_new();
    me->pLabelOptions = LabelOptions_new(0);
    return me;
}

PRIVATE void ServiceInfo_free(ServiceInfo_t * me)
{
    Label_t * pLabel;
    while ((pLabel = (Label_t *) HTList_removeLastObject(me->labels)) != NULL)
        Label_free(pLabel);
    SVal_clear(&me->rating_service);
    LabelOptions_free(me->pLabelOptions);
    LabelError_free(me->pLabelError);
    HT_FREE(me);
}

PRIVATE CSLLData_t * CSLLData_new(void)
{
    CSLLData_t * me;
    if ((me = (CSLLData_t *) HT_CALLOC(1, sizeof(CSLLData_t))) == NULL)
        HT_OUTOFMEM("CSLLData_t");
    me->serviceInfos = HTList_new();
    return me;
}

PRIVATE void CSLLData_free(CSLLData_t * me)
{
    ServiceInfo_t * pServiceInfo;
    if (CSLabelAssoc_findByData(me))
        return;
    while ((pServiceInfo = (ServiceInfo_t *) HTList_removeLastObject(me->serviceInfos)) != NULL)
        ServiceInfo_free(pServiceInfo);
    FVal_clear(&me->version);
    LabelError_free(me->pLabelError);
    HT_FREE(me);
}

/* P U B L I C   C O N S T R U C T O R S / D E S T R U C T O R S */
PRIVATE CSLabel_t * CSLabel_new(CSLLData_t * pCSLLData, LabelTargetCallback_t * pLabelTargetCallback, 
								   LLErrorHandler_t * pLLErrorHandler)
{
    CSLabel_t * me;
    if ((me = (CSLabel_t *) HT_CALLOC(1, sizeof(CSLabel_t))) == NULL)
        HT_OUTOFMEM("CSLabel_t");
    me->pCSLLData = pCSLLData;
    me->pLabelTargetCallback = pLabelTargetCallback;
    me->pLLErrorHandler = pLLErrorHandler;
    CSLabelAssoc_add(me, pCSLLData);
    return me;
}

PUBLIC CSLabel_t * CSLabel_copy(CSLabel_t * old)
{
    CSLabel_t * me = CSLabel_new(old->pCSLLData, old->pLabelTargetCallback, old->pLLErrorHandler);
    memcpy(me, old, sizeof(CSLabel_t));
    
    return me;
}

PUBLIC void CSLabel_free(CSLabel_t * me)
{
    CSLLData_t * pCSLLData = me->pCSLLData;
    CSLabelAssoc_removeByState(me);
    HT_FREE(me);
    CSLLData_free(pCSLLData);
}

PUBLIC CSLLData_t * CSLabel_getCSLLData(CSLabel_t * me)
    {return me->pCSLLData;}
PUBLIC LabelError_t * CSLabel_getLabelError(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentLabelError;}
PUBLIC LabelOptions_t * CSLabel_getLabelOptions(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentLabelOptions;}
PUBLIC ServiceInfo_t * CSLabel_getServiceInfo(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentServiceInfo;}
PUBLIC char * CSLabel_getServiceName(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentServiceInfo ? 
       SVal_value(&pCSLabel->pCurrentServiceInfo->rating_service): 0;}
PUBLIC Label_t * CSLabel_getLabel(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentLabel;}
PUBLIC int CSLabel_getLabelNumber(CSLabel_t * pCSLabel)
    {return pCSLabel->currentLabelNumber;}
PUBLIC SingleLabel_t * CSLabel_getSingleLabel(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentSingleLabel;}
PUBLIC LabelRating_t * CSLabel_getLabelRating(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentLabelRating;}
PUBLIC char * CSLabel_getRatingName(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentLabelRating ? 
       SVal_value(&pCSLabel->pCurrentLabelRating->identifier): 0;}
PUBLIC Range_t * CSLabel_getLabelRatingRange(CSLabel_t * pCSLabel)
    {return pCSLabel->pCurrentRange;}
PUBLIC char * CSLabel_getRatingStr(CSLabel_t * pCSLabel)
{
    HTChunk * pChunk;
    HTList * ranges;
    Range_t * curRange;
    FVal_t fVal;
    int count = 0;
    fVal = CSLabel_getLabelRating(pCSLabel)->value;
    if (FVal_initialized(&fVal))
        return FVal_toStr(&fVal);
    pChunk = HTChunk_new(20);
    ranges = CSLabel_getLabelRating(pCSLabel)->ranges;
    while ((curRange = (Range_t *) HTList_nextObject(ranges)) != NULL) {
        char * ptr;
	count++;
	ptr = Range_toStr(curRange);
	if (count > 1)
	    HTChunk_puts(pChunk, " ");
	HTChunk_puts(pChunk, ptr);
	HT_FREE(ptr);
    }
    return HTChunk_toCString(pChunk);
}

PUBLIC CSParse_t * CSParse_newLabel(LabelTargetCallback_t * pLabelTargetCallback, 
				    LLErrorHandler_t * pLLErrorHandler)
{
    CSParse_t * me = CSParse_new();
    
    CSinitialize_global_data();
    
    me->pParseContext->engineOf = &CSParse_targetParser;
    me->pParseContext->pTargetChangeCallback = &targetChangeCallback;
    me->pParseContext->pParseErrorHandler = &parseErrorHandler;
    me->target.pCSLabel = CSLabel_new(CSLLData_new(), pLabelTargetCallback, pLLErrorHandler);
    me->pTargetObject = &LabelList_targetObject;
    me->currentSubState = SubState_N;
	return me;
}

PUBLIC CSLabel_t * CSParse_getLabel(CSParse_t * me)
{
    return (me->target.pCSLabel);
}

PUBLIC PRBool CSParse_deleteLabel(CSParse_t * pCSParse)
{
    CSLabel_t * me = GetCSLabel(pCSParse);
    CSLLData_free(CSLabel_getCSLLData(me));
    CSLabel_free(me);
	CSParse_delete(pCSParse);
    return YES;
}

/* D E F A U L T   P A R S I N G   H A N D L E R S */
PRIVATE StateRet_t targetChangeCallback(CSParse_t * pCSParse, TargetObject_t * pTargetObject, CSParseTC_t target, PRBool closed, void * pVoid)
{

	CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	if (pCSLabel->pLabelTargetCallback)
		return (*pCSLabel->pLabelTargetCallback)(pCSLabel, pCSParse, (CSLLTC_t)target, closed, pVoid);
    return StateRet_OK;
}

PRIVATE StateRet_t parseErrorHandler(CSParse_t * pCSParse, const char * token, char demark, StateRet_t errorCode)
{
	CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	if (pCSLabel->pLLErrorHandler)
		return (*pCSLabel->pLLErrorHandler)(pCSLabel, pCSParse, token, demark, errorCode);
  return errorCode;
}

/* CSParse_doc methods */
/* P A R S I N G   S T A T E   F U N C T I O N S */
#ifndef NO_CHAR_TEST
PRIVATE PRBool charSetOK(CSParse_t * pCSParse, char * checkMe, CharSet_t set)
{
	if(!checkMe)
		return FALSE;

    for (;*checkMe;checkMe++) {
        if (set & CharSet_ALPHAS && 
            ((*checkMe >= 'A' && *checkMe <= 'Z') || 
             (*checkMe >= 'a' && *checkMe <= 'z')))
            continue;
        if (set & CharSet_DIGITS && 
            ((*checkMe >= '0' && *checkMe <= '9') || *checkMe == '.'))
            continue;
        if (set & CharSet_PLUSMINUS && 
            ((*checkMe == '+' || *checkMe == '-')))
            continue;
        if (set & CharSet_FORSLASH && 
            *checkMe == '/')
            continue;
        if (set & CharSet_BASE64_EXTRAS && 
            ((*checkMe == '+' || *checkMe == '/' || *checkMe == '=')))
            continue;
        if (set & CharSet_DATE_EXTRAS && 
            (*checkMe == '.' || *checkMe == ':' || 
             *checkMe == '-' || *checkMe == 'T'))
            continue;
		/* RFC1738:2.1:"+.-","#%",";/"?:@=&" 2.2:"$-_.+!*'()," */
        if (set & CharSet_URL_EXTRAS && 
            (*checkMe == ':' || *checkMe == '?' || 
             *checkMe == '#' || *checkMe == '%' || 
             *checkMe == '/' || *checkMe == '.' ||
             *checkMe == '-' || *checkMe == '_' ||
	     *checkMe == '~' || *checkMe == '\\'||
	     *checkMe == '@'))
            continue;
/* '.' | ' ' | ',' | ';' | ':' | '&' | '=' | '?' | '!' | '*' | '~' | '@' | '#' */
        if (set & CharSet_EXTENS && 
            (*checkMe == '.' || *checkMe == ' ' || 
             *checkMe == ',' || *checkMe == ';' || 
             *checkMe == ':' || *checkMe == '&' || 
             *checkMe == '=' || *checkMe == '?' || 
             *checkMe == '!' || *checkMe == '*' || 
             *checkMe == '~' || *checkMe == '@' || 
             *checkMe == '#' || *checkMe == '\''|| 
             *checkMe == '/' || *checkMe == '-'))
            continue;
        pCSParse->pParseContext->pTokenError = checkMe;
        return FALSE;
    }
    return TRUE;
}
#endif /* !NO_CHAR_TEST */

PRIVATE StateRet_t isQuoted(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    ParseContext_t * pParseContext = pCSParse->pParseContext;
    if (!pParseContext->observedQuotes)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    return StateRet_OK;
}

PRIVATE StateRet_t hasToken(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    return token ? StateRet_OK : StateRet_WARN_NO_MATCH;
}
#if 0
PRIVATE StateRet_t clearToken(CSParse_t * pCSParse, char * token, char demark)
{
    HTChunk_clear(pCSParse->token);
    return StateRet_OK;
}
#endif
/* getOption - see if token matches an option.
This may be called by:
    ServiceInfo: add option to existent options, pCurrentLabelError is set by ServiceInfo_open
    Label: kick off SingleLabel - SingleLabel_new(), pCurrentLabelError is 0
    SingleLabel: add another option to existent options, pCurrentLabelError is set by SingleLabel_open
 */
#define CSOffsetOf(s,m) (size_t)&(((s *)0)->m)

#define CHECK_OPTION_TOKEN_BVAL1(text, pointer) \
if (!PL_strcasecmp(token, text)) {\
    pCSParse->pParseContext->valTarget.pTargetBVal = pointer;\
    pCSParse->pParseContext->valType = ValType_BVAL;\
    break;\
}

#define CHECK_OPTION_TOKEN_FVAL1(text, pointer) \
if (!PL_strcasecmp(token, text)) {\
    pCSParse->pParseContext->valTarget.pTargetFVal = pointer;\
    pCSParse->pParseContext->valType = ValType_FVAL;\
    break;\
}

#define CHECK_OPTION_TOKEN_SVAL1(text, pointer, charSet) \
if (!PL_strcasecmp(token, text)) {\
    pCSParse->pParseContext->valTarget.pTargetSVal = pointer;\
    pCSParse->pParseContext->valType = ValType_SVAL;\
    SET_CHAR_SET(charSet)\
    break;\
}

#define CHECK_OPTION_TOKEN_DVAL1(text, pointer) \
if (!PL_strcasecmp(token, text)) {\
    pCSParse->pParseContext->valTarget.pTargetDVal = pointer;\
    pCSParse->pParseContext->valType = ValType_DVAL;\
    break;\
}

PRIVATE StateRet_t getOption(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    LabelOptions_t * me = pCSLabel->pCurrentLabelOptions;
    if (!token)
        return StateRet_WARN_NO_MATCH;
    if (!me)
        me = pCSLabel->pCurrentLabelOptions = LabelOptions_new(pCSLabel->pCurrentServiceInfo->pLabelOptions);
    /* match token against legal options */
    pCSParse->pParseContext->valType = ValType_NONE;  /* use valType to flag a match */
    
    do { /* fake do loop for break statements (to religiously avoid the goto) */
        CHECK_OPTION_TOKEN_DVAL1("at", &me->at)
        CHECK_OPTION_TOKEN_SVAL1("by", &me->by, CharSet_EXT_ALPHANUM)
        CHECK_OPTION_TOKEN_SVAL1("complete_label", &me->complete_label, CharSet_URL)
        CHECK_OPTION_TOKEN_SVAL1("full", &me->complete_label, CharSet_URL)
        CHECK_OPTION_TOKEN_SVAL1("for", &me->fur, CharSet_URL)
        CHECK_OPTION_TOKEN_BVAL1("generic", &me->generic)
        CHECK_OPTION_TOKEN_BVAL1("gen", &me->generic)
        CHECK_OPTION_TOKEN_SVAL1("MIC-md5", &me->MIC_md5, CharSet_BASE64)
        CHECK_OPTION_TOKEN_SVAL1("md5", &me->MIC_md5, CharSet_BASE64)
        CHECK_OPTION_TOKEN_DVAL1("on", &me->on)
        CHECK_OPTION_TOKEN_SVAL1("signature-PKCS", &me->signature_PKCS, CharSet_BASE64)
        CHECK_OPTION_TOKEN_DVAL1("until", &me->until)
        CHECK_OPTION_TOKEN_DVAL1("exp", &me->until)
		if (!PL_strcasecmp(token, "comment")) {
			pCSParse->pParseContext->valTarget.pTargetList = &me->comments;
		    pCSParse->pParseContext->valType = ValType_COMMENT;
		    break;
		}
    } while (0);
    
    if (pCSParse->pParseContext->valType == ValType_NONE)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    return StateRet_OK;
}

PRIVATE StateRet_t getOptionValue(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    switch (pCSParse->pParseContext->valType) {
        case ValType_BVAL:
            BVal_readVal(pCSParse->pParseContext->valTarget.pTargetBVal, token);
            pCSParse->pParseContext->valType = ValType_NONE;
            break;
        case ValType_FVAL:
            CHECK_CAR_SET(CharSet_NUMBER)
            FVal_readVal(pCSParse->pParseContext->valTarget.pTargetFVal, token);
            pCSParse->pParseContext->valType = ValType_NONE;
            break;
        case ValType_SVAL:
            CHECK_CAR_SET(pCSLabel->targetCharSet)
            SVal_readVal(pCSParse->pParseContext->valTarget.pTargetSVal, token);
            pCSParse->pParseContext->valType = ValType_NONE;
            break;
        case ValType_DVAL:
            CHECK_CAR_SET(CharSet_DATE)
            DVal_readVal(pCSParse->pParseContext->valTarget.pTargetDVal, token);
            pCSParse->pParseContext->valType = ValType_NONE;
            break;
        case ValType_COMMENT:
            CHECK_CAR_SET(CharSet_EXT_ALPHANUM)
			{
				char * ptr = 0;
				StrAllocCopy(ptr, token);
				HTList_appendObject(*pCSParse->pParseContext->valTarget.pTargetList, (void *)ptr);
			}
            break;
	default:
	    break;
    }
    return StateRet_OK;
}

PRIVATE StateRet_t LabelList_open(CSParse_t * pCSParse, char * token, char demark)
{
    return StateRet_OK;
}

PRIVATE StateRet_t LabelList_getVersion(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
static const char versionPrefix[] = "PICS-";
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    if (!token)
        return StateRet_WARN_NO_MATCH;
    if (PL_strncasecmp(token, versionPrefix, sizeof(versionPrefix)-1))
        return StateRet_WARN_NO_MATCH;
	token += sizeof(versionPrefix)-1;
    CHECK_CAR_SET(CharSet_NUMBER)
    FVal_readVal(&pCSLabel->pCSLLData->version, token);
    return StateRet_OK;
}

PRIVATE StateRet_t LabelList_close(CSParse_t * pCSParse, char * token, char demark)
{
    return StateRet_DONE;
}

PRIVATE void LabelList_destroy(CSParse_t * pCSParse)
{
}

PRIVATE StateRet_t ServiceInfo_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentServiceInfo = ServiceInfo_new();
    pCSLabel->currentLabelNumber = 0;
    HTList_appendObject(pCSLabel->pCSLLData->serviceInfos, (void *)pCSLabel->pCurrentServiceInfo);
    pCSLabel->pCurrentLabelOptions = pCSLabel->pCurrentServiceInfo->pLabelOptions;
	return StateRet_OK;
}

PRIVATE StateRet_t ServiceInfo_getServiceId(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    ParseContext_t * pParseContext = pCSParse->pParseContext;

    if (!token || !pParseContext->observedQuotes)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    CHECK_CAR_SET(CharSet_URL)
    SVal_readVal(&pCSLabel->pCurrentServiceInfo->rating_service, token);
    return StateRet_OK;
}

PRIVATE StateRet_t ServiceInfo_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentServiceInfo = 0;
	return StateRet_OK;
}

PRIVATE void ServiceInfo_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    HTList_removeObject(pCSLabel->pCSLLData->serviceInfos, (void *)pCSLabel->pCurrentServiceInfo);
    ServiceInfo_free(pCSLabel->pCurrentServiceInfo);
    pCSLabel->pCurrentServiceInfo = 0;
}

PRIVATE StateRet_t ServiceInfo_clearOpts(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    pCSLabel->pCurrentLabelOptions = 0; /* needed to flag new SingleLabel started by option */
    return StateRet_OK;
}

PRIVATE StateRet_t Label_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentLabel = Label_new();
    pCSLabel->currentLabelNumber++;
    HTList_appendObject(pCSLabel->pCurrentServiceInfo->labels, (void*)pCSLabel->pCurrentLabel);
    return StateRet_OK;
}

PRIVATE StateRet_t Label_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentLabel = 0;
    return StateRet_OK;
}

PRIVATE void Label_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    HTList_removeObject(pCSLabel->pCurrentServiceInfo->labels, pCSLabel->pCurrentLabel);
    Label_free(pCSLabel->pCurrentLabel);
    pCSLabel->pCurrentLabel = 0;
}

PRIVATE StateRet_t LabelTree_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCSLLData->hasTree = 1;
    pCSLabel->pCurrentLabelTree = pCSLabel->pCurrentLabel->singleLabels = HTList_new();
    return StateRet_OK;
}

PRIVATE StateRet_t LabelTree_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
/*    Label_close(pCSParse, token, demark); */
    pCSLabel->pCurrentLabelTree = 0;
    return StateRet_OK;
}

PRIVATE void LabelTree_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    SingleLabel_t * pSingleLabel;
    while ((pSingleLabel = (SingleLabel_t *) HTList_removeLastObject(pCSLabel->pCurrentLabel->singleLabels)) != NULL)
        SingleLabel_free(pSingleLabel);
    HTList_delete(pCSLabel->pCurrentLabel->singleLabels);
    pCSLabel->pCurrentLabel->singleLabels = 0;
}

PRIVATE StateRet_t SingleLabel_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentSingleLabel = SingleLabel_new(pCSLabel->pCurrentLabelOptions, pCSLabel->pCurrentServiceInfo->pLabelOptions);
    if (pCSLabel->pCurrentLabel->singleLabels)
        HTList_appendObject(pCSLabel->pCurrentLabel->singleLabels, (void*)pCSLabel->pCurrentSingleLabel);
    else
        pCSLabel->pCurrentLabel->pSingleLabel = pCSLabel->pCurrentSingleLabel;
    pCSLabel->pCurrentLabelOptions = pCSLabel->pCurrentSingleLabel->pLabelOptions;
    return StateRet_OK;
}

PRIVATE StateRet_t SingleLabel_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentSingleLabel = 0;
    return StateRet_OK;
}

PRIVATE void SingleLabel_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    if (pCSLabel->pCurrentLabel->pSingleLabel)
        pCSLabel->pCurrentLabel->pSingleLabel = 0;
    else
        HTList_removeObject(pCSLabel->pCurrentLabel->singleLabels, (void *)pCSLabel->pCurrentSingleLabel);
    SingleLabel_free(pCSLabel->pCurrentSingleLabel);
    pCSLabel->pCurrentSingleLabel = 0;
}

PRIVATE StateRet_t LabelRating_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    if (!pCSLabel->pCurrentSingleLabel) /* switched from label to rating on "r" rather than <option> */
        SingleLabel_open(pCSParse, token, demark);
    pCSLabel->pCurrentLabelRating = LabelRating_new();
    HTList_appendObject(pCSLabel->pCurrentSingleLabel->labelRatings, (void*)pCSLabel->pCurrentLabelRating);
    pCSLabel->pCurrentLabelOptions = 0;
    return StateRet_OK;
}

PRIVATE StateRet_t LabelRating_getId(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    CHECK_CAR_SET(CharSet_TRANSMIT_NAME)
    SVal_readVal(&pCSLabel->pCurrentLabelRating->identifier, token);
    return StateRet_OK;
}

PRIVATE StateRet_t LabelRating_getValue(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    FVal_readVal(&pCSLabel->pCurrentLabelRating->value, token);
    return StateRet_OK;
}

PRIVATE StateRet_t LabelRating_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentLabelRating = 0;
    return StateRet_OK;
}

PRIVATE void LabelRating_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    HTList_removeObject(pCSLabel->pCurrentSingleLabel->labelRatings, (void *)pCSLabel->pCurrentLabelRating);
    LabelRating_free(pCSLabel->pCurrentLabelRating);
    pCSLabel->pCurrentLabelRating = 0;
}

PRIVATE StateRet_t LabelRating_next(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    SingleLabel_doClose(pCSParse, token, demark);
    if (pCSLabel->pCurrentLabelTree) {
        SETNEXTSTATE(&LabelTree_targetObject, SubState_A);
    } else {
	Label_doClose(pCSParse, token, demark);
        SETNEXTSTATE(&Awkward_targetObject, SubState_A);
    }
    return StateRet_OK;
}

PRIVATE StateRet_t LabelRatingRange_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentRange = 0;
    pCSLabel->pCurrentLabelRating->ranges = HTList_new();
    return StateRet_OK;
}

PRIVATE StateRet_t LabelRatingRange_get(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    LabelRating_t * pLabelRating = pCSLabel->pCurrentLabelRating;
    Range_t * me;
    char * ptr, * backPtr;
    if (!token)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
	if ((me = (Range_t *) HT_CALLOC(1, sizeof(Range_t))) == NULL)
	    HT_OUTOFMEM("Range_t");
/*    me = Range_new(); */
    HTList_appendObject(pLabelRating->ranges, (void *)me);
    backPtr = ptr = token;
    while (*ptr) {
        if (*ptr == ':') {
            *ptr = 0;
            ptr++;
            break;
        }
        ptr++;
    }
    FVal_readVal(&me->min, backPtr);
    if (*ptr)
        FVal_readVal(&me->max, ptr);
    return StateRet_OK;
}

PRIVATE StateRet_t LabelRatingRange_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentRange = 0;
    return StateRet_OK;
}

PRIVATE void LabelRatingRange_destroy(CSParse_t * pCSParse)
{
}

PRIVATE StateRet_t Awkward_open(CSParse_t * pCSParse, char * token, char demark)
{
    return StateRet_OK;
}

PRIVATE StateRet_t Awkward_close(CSParse_t * pCSParse, char * token, char demark)
{
    return StateRet_OK;
}

PRIVATE void Awkward_destroy(CSParse_t * pCSParse)
{
}

PRIVATE StateRet_t error_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentLabelError = LabelError_new();
    if (pCSLabel->pCurrentLabel)
        pCSLabel->pCurrentLabel->pLabelError = pCSLabel->pCurrentLabelError;
    else
        pCSLabel->pCurrentServiceInfo->pLabelError = pCSLabel->pCurrentLabelError;
    return StateRet_OK;
}

PRIVATE StateRet_t error_getExpl(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    ParseContext_t * pParseContext = pCSParse->pParseContext;
    char * explaination = 0;

    if (!token || !pParseContext->observedQuotes)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    CHECK_CAR_SET(CharSet_EXT_ALPHANUM)
    StrAllocCopy(explaination, token);
    HTList_appendObject(pCSLabel->pCurrentLabelError->explanations, explaination);
    return StateRet_OK;
}

PRIVATE StateRet_t error_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);

    pCSLabel->pCurrentLabelError = 0;
    if (pCSLabel->pCurrentLabel)
        pCSLabel->pCurrentLabel->pLabelError = pCSLabel->pCurrentLabelError;
    else
        pCSLabel->pCurrentServiceInfo->pLabelError = pCSLabel->pCurrentLabelError;
    return StateRet_OK;
}

PRIVATE void error_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    if (pCSLabel->pCurrentLabel)
        pCSLabel->pCurrentLabel->pLabelError = 0;
    else
        pCSLabel->pCurrentServiceInfo->pLabelError = 0;
    LabelError_free(pCSLabel->pCurrentLabelError);
}

PRIVATE StateRet_t Extension_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	Extension_t * me = Extension_new();
	pCSLabel->pCurrentExtension = me;
	if (!pCSLabel->pCurrentLabelOptions->extensions)
		pCSLabel->pCurrentLabelOptions->extensions = HTList_new();
	HTList_appendObject(pCSLabel->pCurrentLabelOptions->extensions, (void *)me);
	return StateRet_OK;
}

PRIVATE StateRet_t Extension_mandatory(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	pCSLabel->pCurrentExtension->mandatory = 1;
	pCSLabel->pCSLLData->mandatoryExtensions++;
    return StateRet_OK;
}

PRIVATE StateRet_t Extension_getURL(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    if (!token || !pCSParse->pParseContext->observedQuotes)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    CHECK_CAR_SET(CharSet_URL)
	SVal_readVal(&pCSLabel->pCurrentExtension->url, token);
	return StateRet_OK;
}

PRIVATE StateRet_t Extension_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	pCSLabel->pCurrentExtension = 0;
	return StateRet_OK;
}

PRIVATE void Extension_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    HTList_removeObject(pCSLabel->pCurrentLabelOptions->extensions, (void *)pCSLabel->pCurrentExtension);
	if (!HTList_count(pCSLabel->pCurrentLabelOptions->extensions)) {
        HTList_delete(pCSLabel->pCurrentLabelOptions->extensions);
        pCSLabel->pCurrentLabelOptions->extensions = 0;
    }
    Extension_free(pCSLabel->pCurrentExtension);
    pCSLabel->pCurrentExtension = 0;
}

PRIVATE StateRet_t Extension_next(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    if (pCSLabel->pCurrentSingleLabel) {
        SETNEXTSTATE(&SingleLabel_targetObject, SubState_A);
    } else {
        SETNEXTSTATE(&ServiceInfo_targetObject, SubState_B);
    }
	return StateRet_OK;
}

PRIVATE StateRet_t ExtensionData_open(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	ExtensionData_t * me = ExtensionData_new();

	me->pParentExtensionData = pCSLabel->pCurrentExtensionData;
	if(pCSLabel->pCurrentExtensionData) {
		if (!pCSLabel->pCurrentExtensionData->moreData)
			pCSLabel->pCurrentExtensionData->moreData = HTList_new();
		HTList_appendObject(pCSLabel->pCurrentExtensionData->moreData, (void *)me);
	} else {
		if (!pCSLabel->pCurrentExtension->extensionData)
			pCSLabel->pCurrentExtension->extensionData = HTList_new();
		HTList_appendObject(pCSLabel->pCurrentExtension->extensionData, (void *)me);
	}
	pCSLabel->pCurrentExtensionData = me;
	return StateRet_OK;
}

PRIVATE StateRet_t ExtensionData_next(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	/* close has already set recursed to the parentExtensionData */
    if (pCSLabel->pCurrentExtensionData) {
        SETNEXTSTATE(&ExtensionData_targetObject, SubState_B);
    } else {
        SETNEXTSTATE(&Extension_targetObject, SubState_C);
    }
    return StateRet_OK;
}

PRIVATE StateRet_t ExtensionData_close(CSParse_t * pCSParse, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    pCSLabel->pCurrentExtensionData = pCSLabel->pCurrentExtensionData->pParentExtensionData;
    return StateRet_OK;
}

PRIVATE void ExtensionData_destroy(CSParse_t * pCSParse)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
    HTList ** pHolderList = pCSLabel->pCurrentExtensionData->pParentExtensionData ? 
                            &pCSLabel->pCurrentExtensionData->pParentExtensionData->moreData : 
                            &pCSLabel->pCurrentExtension->extensionData;
    HTList_removeObject(*pHolderList, (void *)pCSLabel->pCurrentExtensionData);
	if (!HTList_count(*pHolderList)) {
        HTList_delete(*pHolderList);
        *pHolderList = 0;
    }
    ExtensionData_free(pCSLabel->pCurrentExtensionData);
    pCSLabel->pCurrentExtensionData = 0;
}

PRIVATE StateRet_t ExtensionData_getData(CSParse_t * pCSParse, StateToken_t * pStateToken, char * token, char demark)
{
    CSLabel_t * pCSLabel = GetCSLabel(pCSParse);
	ExtensionData_t * me;
    if (!token)
        return StateRet_WARN_NO_MATCH;
    if (Punct_badDemark(pStateToken->validPunctuation, demark))
        return StateRet_WARN_BAD_PUNCT;
    CHECK_CAR_SET(CharSet_EXT_DATA)
	me = pCSLabel->pCurrentExtensionData;
/*    SVal_readVal(&me->text, token); */
	StrAllocCopy(me->text, token);
	me->quoted = pCSParse->pParseContext->observedQuotes;
	return StateRet_OK;
}
#if 0
PRIVATE StateRet_t LabelRating_doClose(CSParse_t * pCSParse, char * token, char demark)
{
    if (pCSParse->pParseContext->pTargetChangeCallback && 
		(*pCSParse->pParseContext->pTargetChangeCallback)(pCSParse, &LabelRating_targetObject, CSLLTC_RATING, 2) == StateRet_ERROR)
			return NowIn_ERROR;
    return LabelRating_close(pCSParse, token, demark);
}
#endif
PRIVATE StateRet_t SingleLabel_doClose(CSParse_t * pCSParse, char * token, char demark)
{
    if (pCSParse->pParseContext->pTargetChangeCallback && 
       	(*pCSParse->pParseContext->pTargetChangeCallback)(pCSParse, &SingleLabel_targetObject, CSLLTC_SINGLE, 2, 0) == StateRet_ERROR) /* !!! - pVoid */
			return NowIn_ERROR;
	return SingleLabel_close(pCSParse, token, demark);
}
#if 0
PRIVATE StateRet_t LabelTree_doClose(CSParse_t * pCSParse, char * token, char demark)
{
    if (pCSParse->pParseContext->pTargetChangeCallback && 
		(*pCSParse->pParseContext->pTargetChangeCallback)(pCSParse, &LabelTree_targetObject, CSLLTC_LABTREE, 2) == StateRet_ERROR)
			return NowIn_ERROR;
	return LabelTree_close(pCSParse, token, demark);
}
#endif
PRIVATE StateRet_t Label_doClose(CSParse_t * pCSParse, char * token, char demark)
{
    if (pCSParse->pParseContext->pTargetChangeCallback && 
       	(*pCSParse->pParseContext->pTargetChangeCallback)(pCSParse, &Label_targetObject, CSLLTC_LABEL, 2, 0) == StateRet_ERROR) /* !!! - pVoid */
			return NowIn_ERROR;
	return Label_close(pCSParse, token, demark);
}
#if 0
PRIVATE StateRet_t ServiceInfo_doClose(CSParse_t * pCSParse, char * token, char demark)
{
    if (pCSParse->pParseContext->pTargetChangeCallback && 
		(*pCSParse->pParseContext->pTargetChangeCallback)(pCSParse, &ServiceInfo_targetObject, CSLLTC_SERVICE, 2) == StateRet_ERROR)
			return NowIn_ERROR;
    return ServiceInfo_close(pCSParse, token, demark);
}
#endif
/* CSParse_doc end */
/* I T E R A T O R S - scan through the CSLabel data structures for <identifier> */
/* CSLabel_iterateServices - look for rating service in a label list
	   (pCSLabel->pCurrentServiceInfo = (ServiceInfo_t *) HTList_nextObject(serviceInfos)) && 
	   SVal_initialized(&pCSLabel->pCurrentServiceInfo->rating_service))
        if (!identifier || !PL_strcasecmp(SVal_value(&pCSLabel->pCurrentServiceInfo->rating_service), identifier)) {
	    ret = (*pIteratorCB)(pCSLabel, pParms, identifier, pVoid);
            count++;
 */

PUBLIC CSError_t CSLabel_iterateServices(CSLabel_t * pCSLabel, CSLabel_callback_t * pIteratorCB, State_Parms_t * pParms, const char * identifier, void * pVoid)
{
    HTList * serviceInfos;
    CSError_t ret = CSError_OK;
    int count = 0;
    if (!pIteratorCB ||
        !pCSLabel ||
        !pCSLabel->pCSLLData->serviceInfos)
        return CSError_BAD_PARAM;
    serviceInfos = pCSLabel->pCSLLData->serviceInfos;
    while (ret == CSError_OK && 
	   (pCSLabel->pCurrentServiceInfo = (ServiceInfo_t *) HTList_nextObject(serviceInfos)) != NULL) {
        if (identifier && 
	    (!SVal_initialized(&pCSLabel->pCurrentServiceInfo->rating_service) || 
	     PL_strcasecmp(SVal_value(&pCSLabel->pCurrentServiceInfo->rating_service), identifier)))
	    continue;
    ret = (*pIteratorCB)(pCSLabel, pParms, identifier, pVoid);
	count++;
    }
    if (!count)
        return CSError_SERVICE_MISSING;
    return ret;
}

/* CSLabel_iterateLabels - look through all labels in current ServiceInfo
 */
PUBLIC CSError_t CSLabel_iterateLabels(CSLabel_t * pCSLabel, CSLabel_callback_t * pIteratorCB, State_Parms_t * pParms, const char * identifier, void * pVoid)
{
    HTList * labels;
    CSError_t ret= CSError_OK;
    int count = 0;
    if (!pIteratorCB ||
        !pCSLabel ||
        !pCSLabel->pCurrentServiceInfo ||
        !pCSLabel->pCurrentServiceInfo->labels)
        return CSError_BAD_PARAM;
    labels = pCSLabel->pCurrentServiceInfo->labels;
	while (ret == CSError_OK && (pCSLabel->pCurrentLabel = (Label_t *) HTList_nextObject(labels)) != NULL) {
            ret = (*pIteratorCB)(pCSLabel, pParms, identifier, pVoid);
            count++;
        }
    if (!count)
        return CSError_LABEL_MISSING;
    return ret;
}

/* CSLabel_iterateSingleLabels - look through all single labels in current label
 */
PUBLIC CSError_t CSLabel_iterateSingleLabels(CSLabel_t * pCSLabel, CSLabel_callback_t * pIteratorCB, State_Parms_t * pParms, const char * identifier, void * pVoid)
{
    CSError_t ret= CSError_OK;
    int count = 0;
    if (!pIteratorCB ||
        !pCSLabel ||
        !pCSLabel->pCurrentServiceInfo ||
        !pCSLabel->pCurrentServiceInfo->labels)
        return CSError_BAD_PARAM;
    {
    if (pCSLabel->pCurrentLabel->pSingleLabel) {
        pCSLabel->pCurrentSingleLabel = pCSLabel->pCurrentLabel->pSingleLabel;
            ret = (*pIteratorCB)(pCSLabel, pParms, identifier, pVoid);
            count++;
    }
    else {
        HTList * singleLabels = pCSLabel->pCurrentLabel->singleLabels;
    	while (ret == CSError_OK && (pCSLabel->pCurrentSingleLabel = (SingleLabel_t *) HTList_nextObject(singleLabels)) != NULL) {
                ret = (*pIteratorCB)(pCSLabel, pParms, identifier, pVoid);
                count++;
            }
        }
    }
    if (!count)
        return CSError_SINGLELABEL_MISSING;
    return ret;
}

/* CSLabel_iterateLabelRatings - look for rating in current single label
 */
PUBLIC CSError_t CSLabel_iterateLabelRatings(CSLabel_t * pCSLabel, CSLabel_callback_t * pIteratorCB, State_Parms_t * pParms, const char * identifier, void * pVoid)
{
    HTList * labelRatings;
    CSError_t ret = CSError_OK;
    int count = 0;
    if (!pIteratorCB ||
        !pCSLabel ||
        !pCSLabel->pCurrentServiceInfo ||
        !pCSLabel->pCurrentServiceInfo->labels ||
        !pCSLabel->pCurrentLabel ||
        !pCSLabel->pCurrentSingleLabel ||
        !pCSLabel->pCurrentSingleLabel->labelRatings)
        return CSError_BAD_PARAM;
    labelRatings = pCSLabel->pCurrentSingleLabel->labelRatings;
	while (ret == CSError_OK && (pCSLabel->pCurrentLabelRating = (LabelRating_t *) HTList_nextObject(labelRatings)) != NULL)
        if (!identifier || !PL_strcasecmp(SVal_value(&pCSLabel->pCurrentLabelRating->identifier), identifier)) {
            ret = (*pIteratorCB)(pCSLabel, pParms, identifier, pVoid);
            count++;
        }
    if (!count)
        return CSError_RATING_MISSING;
    return ret;
}

/* R A N G E   T E S T I N G - check that label values fall within acceptable user ranges */
/* CSLabel_ratingsIncludeFVal - find out if current rating in pCSLabel encompases userValue
 * return: int stating how far it is from fitting.
 */
PUBLIC FVal_t CSLabel_ratingsIncludeFVal(CSLabel_t * pCSLabel, FVal_t * userValue)
{
    Range_t parm = Range_NEW_UNINITIALIZED;
    parm.min = *userValue;
    return CSLabel_ratingsIncludeRange(pCSLabel, &parm);
}

PUBLIC FVal_t CSLabel_ratingsIncludeRange(CSLabel_t * pCSLabel, Range_t * pUserRange)
{
    HTList * labelRanges = pCSLabel->pCurrentLabelRating->ranges;
    FVal_t value = pCSLabel->pCurrentLabelRating->value;
    FVal_t ret;
    Range_t * pLabelRange;
    if (FVal_initialized(&value)) {
        Range_t parm = Range_NEW_UNINITIALIZED;
        parm.min = value;
        return Range_gap(&parm, pUserRange);
    }
    while ((pLabelRange = (Range_t *)HTList_nextObject(labelRanges)) != NULL) {
        FVal_t thisOne = Range_gap(pLabelRange, pUserRange);
        if (FVal_isZero(&thisOne))
            return thisOne;
        if (FVal_nearerZero(&thisOne, &ret))
            ret = thisOne;
    }
    return ret;
}

PUBLIC FVal_t CSLabel_ratingsIncludeRanges(CSLabel_t * pCSLabel, HTList * userRanges)
{
    FVal_t ret;
    Range_t * pUserRange;
    PRBool retInitialized = NO;
    while ((pUserRange = (Range_t *)HTList_nextObject(userRanges)) != NULL) {
        FVal_t thisOne = CSLabel_ratingsIncludeRange(pCSLabel, pUserRange);
        if (FVal_isZero(&thisOne))
            return thisOne;
	if (retInitialized) {
	    if (FVal_nearerZero(&thisOne, &ret))
	        ret = thisOne;
	} else {
	    ret = thisOne;
	    retInitialized = YES;
	}
    }
    return ret;
}

