/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */
#include"PlugletsDir.h"
#include "prenv.h"

PlugletsDir::PlugletsDir(void) {
    list = NULL;
    //nb ???
}

PlugletsDir::~PlugletsDir(void) {
    if (list) {
	for (PlugletListIterator *iter = list->GetIterator();iter->Get();delete iter->Get(),iter->Next())
	    ;
	delete list;
    }
}

void PlugletsDir::LoadPluglets() {
    if (!list) {
	list = new PlugletList();
	char * path = PR_GetEnv("PLUGLET");
	int pathLength = strlen(path);
	Pluglet *pluglet;
	for (nsDirectoryIterator iter(nsFileSpec(path),PR_TRUE); iter.Exists(); iter++) {
	    const nsFileSpec& file = iter;
	    const char* name = file.GetCString();
	    int length;
	    if((length = strlen(name)) <= 4  // if it's shorter than ".jar"
	       || strcmp(name+length - 4,".jar") ) {
		continue;
	    }
	    if ( (pluglet = Pluglet::Load(name)) ) {
		list->Add(pluglet);
	    }
	}
    }
}

nsresult PlugletsDir::GetPluglet(const char * mimeType, Pluglet **pluglet) {
    if(!pluglet) {
	return NS_ERROR_NULL_POINTER;
    }
    *pluglet = NULL;
    nsresult res = NS_OK;
    if(!list) {
	LoadPluglets();
    }
    for (PlugletListIterator *iter = list->GetIterator();iter->Get();iter->Next()) {
	if(iter->Get()->Compare(mimeType)) {
	    *pluglet = iter->Get();
	    break;
	}
    }
    return res;
}













