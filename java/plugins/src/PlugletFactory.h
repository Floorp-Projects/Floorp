/* 
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef __PlugletFactory_h__
#define __PlugletFactory_h__
#include "nsplugin.h"

#include "iPlugletEngine.h"

class PlugletFactory {
 public:
    nsresult CreatePluginInstance(const char* aPluginMIMEType, void **aResult);
    nsresult Initialize(void);
    nsresult Shutdown(void); 
    nsresult GetMIMEDescription(const char* *result);
    /*****************************************/
    ~PlugletFactory(void);
    int Compare(const char * mimeType);
    static PlugletFactory * Load(const char * path);
 private:
    jobject jthis;
    static jmethodID createPlugletMID;
    static jmethodID initializeMID; 
    static jmethodID shutdownMID;
    /*****************************************/
    char *mimeDescription;
    char *path;
    PlugletFactory(const char *mimeDescription,const char * path);
    nsCOMPtr<iPlugletEngine> plugletEngine;
};    

#endif /* __PlugletFactory_h__ */
