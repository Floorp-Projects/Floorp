/*--------------------------------------------------------------------
 *  fullsoft.h
 *
 *  Created: 10/15/97
 *  Author:  Matt Kendall
 *
 *  Copyright (C) 1997-98, Full Circle Software, Inc., All Rights Reserved
 *
 *  Full Circle "Spiral" Application API Definition
 *  - mkk 1/19/98 renamed from "spiral.h" to "fullsoft.h"
 *
 *--------------------------------------------------------------------*/
#if !defined(__FULLSOFT_H)
#define __FULLSOFT_H

/* define NO_FC_API to disable all calls to the Full Circle library */
/* define FC_TRACE to enable the Full Circle TRACE macro */
/* define FC_ASSERT to enable the Full Circle ASSERT macro */
/* define FC_TRACE_PARAM to enable the Full Circle TRACE_PARAM macro */
/* define FC_ASSERT_PARAM to enable the Full Circle TRACE_PARAM macro */

#if !defined(FAR)
#define FAR
#endif /* !FAR */

#if !defined(FCAPI)
#define FCAPI
#endif /* defined FCAPI */


typedef const char FAR *      FC_KEY ;
typedef const char FAR *      FC_TRIGGER ;
typedef unsigned long         FC_DATE ;
typedef unsigned long         FC_UINT32 ;
typedef       void FAR *      FC_PVOID ;
typedef const char FAR *      FC_STRING ;
typedef       void FAR *      FC_CONTEXT ;

#define                       FC_CONTEXT_NONE   ((FC_CONTEXT) -1)

typedef enum {
	FC_DATA_TYPE_BINARY,
	FC_DATA_TYPE_STRING,
	FC_DATA_TYPE_INTEGER,
	FC_DATA_TYPE_DATE,
	FC_DATA_TYPE_COUNTER
} FC_DATA_TYPE ;

typedef enum {
	FC_ERROR_OK = 0,
	FC_ERROR_CANT_INITIALIZE,
	FC_ERROR_NOT_INITIALIZED,
	FC_ERROR_ALREADY_INITIALIZED,
	FC_ERROR_FAILED,
	FC_ERROR_OUT_OF_MEMORY,
	FC_ERROR_INVALID_PARAMETER
} FC_ERROR ;

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* define NO_FC_API to disable all calls to the Full Circle library */
	
#if !defined(NO_FC_API)

FC_ERROR FCAPI
FCInitialize( void ) ;

FC_ERROR FCAPI
FCCreateKey(
    FC_KEY key,
    FC_DATA_TYPE type,
    FC_UINT32 first_count,
    FC_UINT32 last_count,
    FC_UINT32 max_element_size) ;

FC_ERROR FCAPI
FCCreatePersistentKey(
    FC_KEY key,
    FC_DATA_TYPE type,
    FC_UINT32 first_count,
    FC_UINT32 last_count,
    FC_UINT32 max_element_size) ;

FC_ERROR FCAPI
FCAddDataToKey(
    FC_KEY key,
    FC_PVOID buffer,
    FC_UINT32 data_length) ;

FC_ERROR FCAPI
FCAddIntToKey(
    FC_KEY key,
    FC_UINT32 data) ;

FC_ERROR FCAPI
FCAddStringToKey(
    FC_KEY key,
    FC_STRING string) ;

FC_ERROR FCAPI
FCAddDateToKey(
	FC_KEY key,
	FC_DATE date) ;

FC_ERROR FCAPI
FCSetCounter(
    FC_KEY key,
    FC_UINT32 value) ;

FC_ERROR FCAPI
FCIncrementCounter(
    FC_KEY key,
    FC_UINT32 value) ;

FC_ERROR FCAPI
FCRegisterMemory(
    FC_KEY key,
    FC_DATA_TYPE type,
    FC_PVOID buffer,
    FC_UINT32 length,
    FC_UINT32 dereference_count,
    FC_CONTEXT context) ;

FC_ERROR FCAPI
FCUnregisterMemory( FC_CONTEXT context ) ;

FC_ERROR FCAPI
FCTrigger( FC_TRIGGER trigger ) ;

void FCAPI
FCTrace(FC_STRING fmt, ... ) ;

void FCAPI
FCAssert() ;

void FCAPI
FCTraceParam(
    FC_UINT32 track,
    FC_UINT32 level,
    FC_STRING fmt,
    ... ) ;

void FCAPI
FCAssertParam(
    FC_UINT32 track,
    FC_UINT32 level ) ;

#if defined(FC_ASSERT)
#if defined(ASSERT)
#undef ASSERT
#endif /* defined ASSERT */
#define ASSERT(a) { if( !(a) ) FCAssert() ; }
#endif /* FC_ASSERT */

#if defined(FC_TRACE)
#if defined(TRACE)
#undef TRACE
#endif /* defined TRACE */
#define TRACE FCTrace
#endif /* FC_TRACE */

#if defined(FC_ASSERT_PARAM)
#if defined(ASSERT_PARAM)
#undef ASSERT_PARAM
#endif /* defined ASSERT_PARAM */
#define ASSERT_PARAM(a,b,c) { if ( !(c) ) FCAssertParam(a,b) ; }
#endif /* FC_ASSERT_PARAM */

#if defined(FC_TRACE_PARAM)
#if defined(TRACE_PARAM)
#undef TRACE_PARAM
#endif /* defined TRACE_PARAM */
#define TRACE_PARAM FCTraceParam
#endif /* FC_TRACE_PARAM */

#else /* NO_FC_API */

#define FCInitialize()                      FC_ERROR_OK
#define FCCreateKey(a,b,c,d,e)              FC_ERROR_OK
#define FCCreatePersistentKey(a,b,c,d,e)    FC_ERROR_OK 
#define FCAddDataToKey(a,b,c)               FC_ERROR_OK 
#define FCAddIntToKey(a,b)                  FC_ERROR_OK 
#define FCAddStringToKey(a,b)               FC_ERROR_OK
#define FCAddDateToKey(a,b)                 FC_ERROR_OK
#define FCRegisterMemory(a,b,c,d,e,f)       FC_ERROR_OK
#define FCUnregisterMemory(a)               FC_ERROR_OK
#define FCTrigger(a)                        FC_ERROR_OK
#define FCSetCounter(a,b)                   FC_ERROR_OK
#define FCIncrementCounter(a,b)             FC_ERROR_OK

#if defined(FC_ASSERT)
#define ASSERT(f)    ((void)0)
#endif /* FC_ASSERT */

#if defined(FC_TRACE)
void FCAPI FCTrace(FC_STRING fmt,...) ;
#define TRACE 1 ? (void)0 : FCTrace
#endif /* FC_TRACE */

#if defined(FC_ASSERT_PARAM)
#define ASSERT_PARAM(a,b,c)    ((void)0)
#endif /* FC_ASSERT_PARAM */

#if defined(FC_TRACE_PARAM)
void FCAPI FCTraceParam(
    FC_UINT32 track,
    FC_UINT32 level,
    FC_STRING fmt,
    ... ) ;

#define TRACE_PARAM 1 ? (void) 0 : FCTraceParam
#endif /* FC_TRACE_PARAM */

#endif /* NO_FC_API */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* __FULLSOFT_H */
