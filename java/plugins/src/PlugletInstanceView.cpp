/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include <windows.h>
#include "PlugletInstanceView.h"
#include "PlugletEngine.h"


jclass   PlugletInstanceView::clazz = NULL;
jmethodID  PlugletInstanceView::initMID = NULL;

PlugletInstanceView::PlugletInstanceView() {
    hWND = NULL;
    frame = NULL;
    isCreated = FALSE;
}
void PlugletInstanceView::Initialize() {
    JNIEnv *env = PlugletEngine::GetJNIEnv();
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

BOOLEAN  PlugletInstanceView::SetWindow(nsPluginWindow* window) {
    if (!window || !window->window) {
        if (isCreated) {
            isCreated = FALSE;
            hWND = NULL;
            frame = NULL;
            return TRUE;
        }
    }
    if (hWND == reinterpret_cast<HWND>(window->window) && isCreated) {
        return FALSE;
    }
    
    DWORD dwStyle = ::GetWindowLong(reinterpret_cast<HWND>(window->window), GWL_STYLE);
    SetWindowLong(reinterpret_cast<HWND>(window->window), GWL_STYLE, dwStyle | WS_CHILD 
                  | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    if(!clazz) {
        Initialize();
        if (!clazz) {
            return FALSE;
        }
    }
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    frame = env->NewObject(clazz,initMID,(jint)window->window);
    if (!frame) {
        env->ExceptionDescribe();
    }
    hWND = (HWND) window->window;
    isCreated = TRUE;
    return TRUE;
}

jobject PlugletInstanceView::GetJObject() {
    return frame;
}







