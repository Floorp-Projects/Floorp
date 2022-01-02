//========= Copyright Valve Corporation ============//
#pragma once

#include <string>

std::string GetEnvironmentVariable( const char *pchVarName );
bool GetEnvironmentVariableAsBool( const char *pchVarName, bool bDefault );
bool SetEnvironmentVariable( const char *pchVarName, const char *pchVarValue );
