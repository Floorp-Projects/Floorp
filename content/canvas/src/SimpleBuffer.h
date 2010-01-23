/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef SIMPLEBUFFER_H_
#define SIMPLEBUFFER_H_

#include "nsICanvasRenderingContextWebGL.h"
#include "localgl.h"

#include "jsapi.h"

namespace mozilla {

class SimpleBuffer {
public:
    SimpleBuffer()
      : type(LOCAL_GL_FLOAT), data(nsnull), length(0), capacity(0), sizePerVertex(0)
    { }

    SimpleBuffer(PRUint32 typeParam,
                 PRUint32 sizeParam,
                 JSContext *ctx,
                 JSObject *arrayObj,
                 jsuint arrayLen)
      : type(LOCAL_GL_FLOAT), data(nsnull), length(0), capacity(0), sizePerVertex(0)
    {
        InitFromJSArray(typeParam, sizeParam, ctx, arrayObj, arrayLen);
    }

    PRBool InitFromJSArray(PRUint32 typeParam,
                           PRUint32 sizeParam,
                           JSContext *ctx,
                           JSObject *arrayObj,
                           jsuint arrayLen);

    ~SimpleBuffer() {
        Release();
    }

    inline PRBool Valid() {
        return data != nsnull;
    }

    inline PRUint32 ElementSize() {
        if (type == LOCAL_GL_FLOAT) return sizeof(float);
        if (type == LOCAL_GL_SHORT) return sizeof(short);
        if (type == LOCAL_GL_UNSIGNED_SHORT) return sizeof(unsigned short);
        if (type == LOCAL_GL_BYTE) return 1;
        if (type == LOCAL_GL_UNSIGNED_BYTE) return 1;
        if (type == LOCAL_GL_INT) return sizeof(int);
        if (type == LOCAL_GL_UNSIGNED_INT) return sizeof(unsigned int);
        if (type == LOCAL_GL_DOUBLE) return sizeof(double);
        return 1;
    }

    void Clear() {
        Release();
    }

    void Set(PRUint32 t, PRUint32 spv, PRUint32 count, void* vals) {
        Prepare(t, spv, count);

        if (count)
            memcpy(data, vals, count*ElementSize());
    }

    void Prepare(PRUint32 t, PRUint32 spv, PRUint32 count) {
        if (count == 0) {
            Release();
        } else {
            type = t;
            EnsureCapacity(PR_FALSE, count*ElementSize());
            length = count;
            sizePerVertex = spv;
        }
    }

    void Release() {
        if (data)
            free(data);
        length = 0;
        capacity = 0;
        data = nsnull;
    }

    void Zero() {
        memset(data, 0, capacity);
    }

    void EnsureCapacity(PRBool preserve, PRUint32 cap) {
        if (capacity >= cap)
            return;

        void* newdata = malloc(cap);
        if (preserve && length)
            memcpy(newdata, data, length*ElementSize());
        free(data);
        data = newdata;
        capacity = cap;
    }

    PRUint32 type;
    void* data;
    PRUint32 length;        // # of elements
    PRUint32 capacity;      // bytes!
    PRUint32 sizePerVertex; // OpenGL "size" param; num coordinates per vertex
};

}

#endif
