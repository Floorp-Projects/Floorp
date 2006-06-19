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
#include <windows.h>
#include "PlugletViewWindows.h"
#include "iPlugletEngine.h"
#include "nsCOMPtr.h"


jclass   PlugletViewWindows::clazz = NULL;
jmethodID  PlugletViewWindows::initMID = NULL;

PlugletViewWindows::PlugletViewWindows() {
    hWND = NULL;
    frame = NULL;
    isCreated = FALSE;
}
void PlugletViewWindows::Initialize() {
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return;
    }

    clazz = env->FindClass("sun/awt/windows/WEmbeddedFrame");
    if (!clazz) {
        env->ExceptionDescribe(); 
        return;
    }
    initMID = env->GetMethodID(clazz, "<init>", "(I)V");
    if (!initMID) {
        env->ExceptionDescribe();
        clazz = NULL;
        return;
    }
}

PRBool  PlugletViewWindows::SetWindow(nsPluginWindow* window) {
    if (!window || !window->window) {
        if (isCreated) {
            isCreated = FALSE;
            hWND = NULL;
            frame = NULL;
            return TRUE;
        }
    }
    if (hWND == reinterpret_cast<HWND>(window->window) && isCreated) {
        return PR_FALSE;
    }
    
    DWORD dwStyle = ::GetWindowLong(reinterpret_cast<HWND>(window->window), GWL_STYLE);
    SetWindowLong(reinterpret_cast<HWND>(window->window), GWL_STYLE, dwStyle | WS_CHILD 
                  | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    if(!clazz) {
        Initialize();
        if (!clazz) {
            return PR_FALSE;
        }
    }
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return PR_FALSE;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return PR_FALSE;
    }

    frame = env->NewObject(clazz,initMID,(jint)window->window);
    if (!frame) {
        env->ExceptionDescribe();
    }
    hWND = (HWND) window->window;
    isCreated = TRUE;
    return PR_TRUE;
}

jobject PlugletViewWindows::GetJObject() {
    return frame;
}







