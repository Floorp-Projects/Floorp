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
#ifndef __PlugletStreamListener_h__
#define __PlugletStreamListener_h__
#include "nsIPluginStreamListener.h"
#include "jni.h"

class PlugletStreamListener : public nsIPluginStreamListener {
 public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo);
    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length);
    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName);
    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result);
    
    PlugletStreamListener(jobject jthis);
    virtual ~PlugletStreamListener(void);
    static void Initialize(void);
 private:
    jobject jthis;
    static jmethodID onStartBindingMID;
    static jmethodID onDataAvailableMID;
    static jmethodID onFileAvailableMID;
    static jmethodID onStopBindingMID;
    static jmethodID getStreamTypeMID;
};

#endif /* __PlugletStreanListener_h__ */






