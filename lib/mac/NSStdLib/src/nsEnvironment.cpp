/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Patrick Beard <beard@netscape.com>
 */

/*
	This file must precede sysenv.c in the link order, so that this implementation of
	getenv() will override Metrowerks' standard, useless version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**
 * Provide a simple environment variable table, that gets loaded once per run of a program.
 */
class Environment {
public:
	Environment();
	~Environment();

	char* get(const char* name);

private:
	struct Variable {
		Variable* mNext;
		char* mName;
		char* mValue;
		
		Variable(const char* name, const char* value)
			: mNext(NULL), mName(NULL), mValue(NULL)
		{
			mName = new char[::strlen(name) + 1];
			if (mName != NULL)
				::strcpy(mName, name);
			if (value != NULL) {
				mValue = new char[::strlen(value) + 1];
				if (mValue != NULL)
					::strcpy(mValue, value);
			}
		}
		
		~Variable() {
			if (mName) delete[] mName;
			if (mValue) delete[] mValue;
		}
	};
	Variable* mVariables;
};

Environment::Environment()
	:	mVariables(NULL)
{
	// opens "ENVIRONMENT" file in current application's directory.
	FILE* f = ::fopen("ENVIRONMENT", "r");
	if (f != NULL) {
		char line[1024];
		while (::fgets(line, sizeof(line), f) != NULL) {
			// allow comments starting with "#" or "//"
			if (line[0] == '#' || (line[0] == '/' && line[1] == '/'))
				continue;
			
			// trim trailing linefeed.
			int len = ::strlen(line);
			if (line[len - 1] == '\n')
				line[--len] = '\0';

			// ignore leading white space.
			char* name = line;
			while (isspace(*name)) ++name;
			
			// look for value.
			char* value = "1";
			char* eq = ::strchr(name, '=');
			if (eq != NULL) {
				value = eq + 1;
				*eq = '\0';
			}
			
			Variable* var = new Variable(name, value);
			var->mNext = mVariables;
			mVariables = var;
		}
		::fclose(f);
	}
}

Environment::~Environment()
{
	Variable* var = mVariables;
	while (var != NULL) {
		Variable* next = var->mNext;
		delete var;
		var = next;
	}
}

char* Environment::get(const char* name)
{
	Variable** link = &mVariables;
	Variable* var = *link;
	while (var != NULL) {
		if (::strcmp(var->mName, name) == 0)
			return var->mValue;
		link = &var->mNext;
		var = *link;
	}
	return NULL;
}

char* std::getenv(const char * name)
{
	static Environment env;
	return env.get(name);
}
