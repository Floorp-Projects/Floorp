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
#ifndef __PlugletInstanceView_h__
#define __PlugletInstanceView_h__
#include <windows.h>
#include "nsplugindefs.h"
#include "jni.h"

class PlugletInstanceView {
 public:
    PlugletInstanceView(void);
    jobject GetJObject(void);
    BOOLEAN SetWindow(nsPluginWindow* window);
 private:
    static  void Initialize(void);
    static  jclass clazz;
    static  jmethodID initMID;
    HWND    hWND;
    BOOL    isCreated;
    jobject frame;
};
#endif /* __PlugletInstanceView_h__ */










