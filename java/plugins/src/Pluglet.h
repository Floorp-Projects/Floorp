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
#ifndef __Pluglet_h__
#define __Pluglet_h__
#include "nsplugin.h"
#include "jni.h"

class Pluglet {
 public:
    nsresult CreatePluginInstance(const char* aPluginMIMEType, void **aResult);
    nsresult Initialize(void);
    nsresult Shutdown(void); 
    nsresult GetMIMEDescription(const char* *result);
    /*****************************************/
    ~Pluglet(void);
    int Compare(const char * mimeType);
    static Pluglet * Load(const char * path);
 private:
    jobject jthis;
    static jmethodID createPlugletInstanceMID;
    static jmethodID initializeMID; 
    static jmethodID shutdownMID;
    /*****************************************/
    char *mimeDescription;
    char *path;
    Pluglet(const char *mimeDescription,const char * path);
};    

#endif /* __Pluglet_h__ */
