//========= Copyright Valve Corporation ============//
#include "envvartools_public.h"
#include "strtools_public.h"
#include <stdlib.h>
#include <string>
#include <cctype>

#if defined(_WIN32)
#include <windows.h>

#undef GetEnvironmentVariable
#undef SetEnvironmentVariable
#endif


std::string GetEnvironmentVariable( const char *pchVarName )
{
#if defined(_WIN32)
	char rchValue[32767]; // max size for an env var on Windows
	DWORD cChars = GetEnvironmentVariableA( pchVarName, rchValue, sizeof( rchValue ) );
	if( cChars == 0 )
		return "";
	else
		return rchValue;
#elif defined(POSIX)
	char *pchValue = getenv( pchVarName );
	if( pchValue )
		return pchValue;
	else
		return "";
#else
#error "Unsupported Platform"
#endif
}

bool GetEnvironmentVariableAsBool( const char *pchVarName, bool bDefault )
{
	std::string sValue = GetEnvironmentVariable( pchVarName );

	if ( sValue.empty() )
	{
		return bDefault;
	}

	sValue = StringToLower( sValue );
	std::string sYesValues[] = { "y", "yes", "true" };
	std::string sNoValues[] = { "n", "no", "false" };

	for ( std::string &sMatch : sYesValues )
	{
		if ( sMatch == sValue )
		{
			return true;
		}
	}

	for ( std::string &sMatch : sNoValues )
	{
		if ( sMatch == sValue )
		{
			return false;
		}
	}

	if ( std::isdigit( sValue.at(0) ) )
	{
		return atoi( sValue.c_str() ) != 0;
	}

	fprintf( stderr,
			 "GetEnvironmentVariableAsBool(%s): Unable to parse value '%s', using default %d\n",
			 pchVarName, sValue.c_str(), bDefault );
	return bDefault;
}

bool SetEnvironmentVariable( const char *pchVarName, const char *pchVarValue )
{
#if defined(_WIN32)
	return 0 != SetEnvironmentVariableA( pchVarName, pchVarValue );
#elif defined(POSIX)
	if( pchVarValue == NULL )
		return 0 == unsetenv( pchVarName );
	else
		return 0 == setenv( pchVarName, pchVarValue, 1 );
#else
#error "Unsupported Platform"
#endif
}
