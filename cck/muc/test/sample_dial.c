/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*	JS reflection of the dialer stack
	8/24/98
*/

#include "lm.h"
#include "prmem.h"

typedef struct JSDialer
{
	JSObject*	obj;
	void*		pstub;
} JSDialer;

/* The following definitions need to be kept synchronized with the
values the MUC DLL accepts, currently, there's no universal header
containing them, but you can look in ns/cmd/dialup/win/muc for
the code that implements MUC, and you can reference also
ns/cmd/winfe/mucproc.h for front-end code that references MUC

There should probably be an xp muc(pub).h or somesuch that contains
a universal MUC API.

*/
enum
{
	kGetPluginVersion,
	kSelectAcctConfig,
	kSelectModemConfig,
	kSelectDialOnDemand,
	kConfigureDialer,
	kConnect,
	kHangup,
	kEditEntry,
	kDeleteEntry,
	kRenameEntry,
	kMonitor,
};

#ifdef XP_WIN
typedef long (STDAPICALLTYPE *FARPEFUNC)(
	long		selectorCode,
	void*		paramBlock,
	void*		returnData ); 

jsint CallMuc( JSDialer* dialer, long selector, void* configData, void* returnData )
{
	FARPEFUNC	mucFunc;
	jsint		error = 0;

	mucFunc = (FARPEFUNC)dialer->pstub;
	if ( !mucFunc )
		return -1;
		
	error = (jsint)( *mucFunc )( selector, configData, NULL );
	return error;
}
#else
/* provide an empty prototype until we have Unix/Mac support */
jsint CallMuc( JSDialer*, long, void*, void* ) { }
#endif




enum dialer_slot
{
	PRIVATE_STUB = -1,
};

static JSPropertySpec dialer_props[] =
{
	{ "privateStub", PRIVATE_STUB, JSPROP_READONLY },
	{0}
};

extern JSClass	lm_dialer_class;

JSBool PR_CALLBACK
dialer_getProperty( JSContext* cx, JSObject* obj, jsval id, jsval* vp)
{
	JSDialer*			dialer;
	enum dialer_slot	dialer_slot;
	JSString*			str = NULL;
	jsint				slot;

	*vp = JS_GetEmptyStringValue( cx );
		
	if ( !JSVAL_IS_INT(id) )
		return JS_TRUE;

	slot = JSVAL_TO_INT( id );
	dialer = JS_GetInstancePrivate( cx, obj, &lm_dialer_class, NULL);

	if ( !dialer )
		return JS_TRUE;
	
	dialer_slot = slot;
	switch ( dialer_slot )
	{
		default:
			return JS_TRUE;
	}
	
	if ( str )
		*vp = STRING_TO_JSVAL( str );
	return JS_TRUE;
}

JSBool PR_CALLBACK
dialer_enumerate( JSContext* cx, JSObject* obj )
{
    return JS_TRUE;
}

void PR_CALLBACK
dialer_finalize( JSContext* cx, JSObject* obj )
{
	JSDialer*		dialer;
	
	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return;
	
	XP_DELETE( dialer );
}

JSBool PR_CALLBACK
dialer_toString( JSContext* cx, JSObject* obj, uint argc, jsval* argv, jsval* rval )
{
    JSString*		str;

    str = JS_NewStringCopyZ( cx, "[object Dialer]" );
    if ( !str )
		return JS_FALSE;

    *rval = STRING_TO_JSVAL( str );
    return JS_TRUE;
}


JSClass lm_dialer_class =
{
    "Dialer",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    dialer_getProperty,
    dialer_getProperty,
    dialer_enumerate,
    JS_ResolveStub,
    JS_ConvertStub,
    dialer_finalize
};

PR_STATIC_CALLBACK(PRBool)
dialer_config( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSObject*	stringArray;
	jsuint		count = 0;
	jsval		temp;
	JSString*	stringObject;
	jsuint		stringArrayLength;
	PRBool		result;
	JSDialer*	dialer;
	jsint		error;
	
	char**		configData;
	char*		buffer;

	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	if ( argc != 1 )
		return JS_FALSE;
	
	if ( !JSVAL_IS_OBJECT( argv[ 0 ] ) )
		return JS_FALSE;
	
	JS_ValueToObject( cx, argv[ 0 ], &stringArray );
	if ( !JS_IsArrayObject( cx, stringArray ) )
		return JS_FALSE;
	
	JS_GetArrayLength( cx, stringArray, &stringArrayLength );
	
	configData = JS_malloc( cx, ( ( stringArrayLength + 1 ) * sizeof( char* ) ) );
	if ( !configData )
		return JS_FALSE;
	
	configData[	0 ] = NULL;
	
	while ( count < stringArrayLength )
	{
		if ( JS_GetElement( cx, stringArray, count, &temp ) != JS_TRUE )
			continue;
			
		if ( !JSVAL_IS_STRING( temp ) )
			continue;
		
		stringObject = JS_ValueToString( cx, temp );
		buffer = JS_strdup( cx, JS_GetStringBytes( stringObject ) );
		if ( !buffer )
		{
			configData[ count ] = 0;
			result = JS_FALSE;
			goto fail;
		}
		
		configData[ count ] = buffer;
		buffer = NULL;
		configData[ ++count ] = NULL;
	}

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
	{
		result = JS_FALSE;
		goto fail;
	}
	
	error = CallMuc( dialer, kConfigureDialer, (void*)configData, (void*)NULL );
	if ( error )
	{
		result = JS_FALSE;
		goto fail;
	}
	
	result = JS_TRUE;
	
fail:
	if ( buffer )
		JS_free( cx, buffer );
	count = 0;
	while ( configData[ count ] )
		JS_free( cx, configData[ count++ ] );
	return result;
}

PR_STATIC_CALLBACK(PRBool)
dialer_connect( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSDialer*	dialer;
	jsint		error;
	
	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return JS_FALSE;

	error = CallMuc( dialer, kConnect, (void*)NULL, (void*)NULL );

	if ( error )
		return JS_FALSE;

	return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
dialer_hangup( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSDialer*	dialer;
	jsint		error;
	
	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return JS_FALSE;
		
	error = CallMuc( dialer, kHangup, (void*)NULL, (void*)NULL );
	if ( error )
		return JS_FALSE;

	return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
dialer_rename( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSDialer*	dialer;
	JSString*	string = NULL;
	jsint		error;

	struct
	{
		char*	from;
		char*	to;
	} b;
	
	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return JS_FALSE;
	
	if ( argc != 2 )
		return JS_FALSE;
	
	if ( 	!JSVAL_IS_STRING( argv[ 0 ] ) ||
			!JSVAL_IS_STRING( argv[ 1 ] ) )
		return JS_FALSE;
		
	string = JS_ValueToString( cx, argv[ 0 ] );
	b.from = JS_strdup( cx, JS_GetStringBytes( string ) );
	string = JS_ValueToString( cx, argv[ 1 ] );
	b.to = JS_strdup( cx, JS_GetStringBytes( string ) );

	error = CallMuc( dialer, kRenameEntry, (void*)&b, (void*)NULL );
	
	if ( error )
		return JS_FALSE;

	return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
dialer_delete( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSDialer*	dialer;
	JSString*	string = NULL;
	char*		name = NULL;
	jsint		error;
	
	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return JS_FALSE;
	
	if ( argc != 1 )
		return JS_FALSE;
	
	if ( !JSVAL_IS_STRING( argv[ 0 ] ) )
		return JS_FALSE;
		
	string = JS_ValueToString( cx, argv[ 0 ] );
	name = JS_strdup( cx, JS_GetStringBytes( string ) );
	
	error = CallMuc( dialer, kDeleteEntry, (void*)name, (void*)NULL );	
	
	if ( error )
		return JS_FALSE;

	return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
dialer_monitor( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSDialer*	dialer;
	JSString*	string = NULL;
	char*		name = NULL;
	jsint		error;
	
	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return JS_FALSE;
	
	error = CallMuc( dialer, kMonitor, (void*)NULL, (void*)NULL );

	if ( error )
		return JS_FALSE;

	return JS_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
dialer_edit( JSContext* cx, JSObject* obj, uint argc,
	jsval* argv, jsval* rval )
{
	JSDialer*	dialer;
	JSString*	string = NULL;
	char*		name = NULL;
	jsint		error;
	
	if ( !lm_CanAccessTarget( cx, JSTARGET_UNIVERSAL_DIALER_ACCESS ) )
		return JS_FALSE;

	dialer = JS_GetPrivate( cx, obj );
	if ( !dialer )
		return JS_FALSE;
	
	if ( argc != 1 )
		return JS_FALSE;
	
	if ( !JSVAL_IS_STRING( argv[ 0 ] ) )
		return JS_FALSE;
		
	string = JS_ValueToString( cx, argv[ 0 ] );
	name = JS_strdup( cx, JS_GetStringBytes( string ) );
	
	error = CallMuc( dialer, kEditEntry, (void*)name, (void*)NULL );

	if ( error )
		return JS_FALSE;

	return JS_TRUE;
}

static JSFunctionSpec dialer_methods[] = {
    {"configure",           dialer_config,              1},
	{"connect",				dialer_connect,				1},
	{"hangup",				dialer_hangup,				0},
	{"rename",				dialer_rename,				2},
	{"delete",				dialer_delete,				1},
	{"monitor",				dialer_monitor,				0},
	{"edit",				dialer_edit,				1},
	{"toString",			dialer_toString,			0},
	{0}
};

JSBool PR_CALLBACK
Dialer( JSContext* cx, JSObject* obj, uint argc, jsval* argv, jsval* rval )
{
	return JS_TRUE;
}

JSDialer* create_dialer( JSContext* cx, JSObject* parent_obj )
{
    MochaDecoder*	decoder;
	JSObject*		obj;
	JSDialer*		dialer = NULL;
	
#ifdef XP_WIN
	HINSTANCE		mucDll = NULL;
	FARPEFUNC		stub = NULL;
	long			version;
#endif

    decoder = JS_GetPrivate( cx, JS_GetGlobalObject( cx ) );
	dialer = JS_malloc( cx, sizeof *dialer );
	if ( !dialer )
		return NULL;
	XP_BZERO( dialer, sizeof *dialer );
	
	obj = JS_InitClass( cx, parent_obj, NULL, &lm_dialer_class,
	                   Dialer, 0, dialer_props, dialer_methods, NULL, NULL);
	if ( !obj || !JS_SetPrivate( cx, obj, dialer ) )
	{
	    JS_free( cx, dialer );
	    return NULL;
	}
	
	dialer->obj = obj;
	
	dialer->pstub = NULL;
	
#ifdef XP_WIN
	mucDll = LoadLibrary( "muc.dll" );

	/* apparently LoadLibrary can return failure codes instead of a null ptr. */
	if ( !mucDll || mucDll < 0x00000030 )
		goto done;
		
	stub = (FARPEFUNC)GetProcAddress( mucDll, "PEPluginFunc" );
	if ( !stub )
		goto done;
		
	( *stub )( kGetPluginVersion, NULL, &version );
	if ( version >= 0x00010001 )
		dialer->pstub = (void*)stub;
#endif
done:
	return dialer;
}

JSObject* lm_NewDialer( JSContext* cx, JSObject* parent_obj )
{
    JSDialer*			dialer;
    
	dialer = create_dialer( cx, parent_obj );
	return ( dialer ? dialer->obj : NULL);
}

