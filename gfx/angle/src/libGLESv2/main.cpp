//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// main.cpp: DLL entry point and management of thread-local data.

#include "libGLESv2/main.h"
#include "libGLESv2/Context.h"

#include "common/tls.h"

static TLSIndex currentTLS = TLS_INVALID_INDEX;

namespace gl
{

// TODO(kbr): figure out how these are going to be managed on
// non-Windows platforms. These routines would need to be exported
// from ANGLE and called cooperatively when users create and destroy
// threads -- or the initialization of the TLS index, and allocation
// of thread-local data, will have to be done lazily. Will have to use
// destructor function with pthread_create_key on POSIX platforms to
// clean up thread-local data.

// Call this exactly once at process startup.
bool CreateThreadLocalIndex()
{
    currentTLS = CreateTLSIndex();
    if (currentTLS == TLS_INVALID_INDEX)
    {
        return false;
    }
    return true;
}

// Call this exactly once at process shutdown.
void DestroyThreadLocalIndex()
{
    DestroyTLSIndex(currentTLS);
    currentTLS = TLS_INVALID_INDEX;
}

// Call this upon thread startup.
Current *AllocateCurrent()
{
    ASSERT(currentTLS != TLS_INVALID_INDEX);
    if (currentTLS == TLS_INVALID_INDEX)
    {
        return NULL;
    }

    Current *current = new Current();
    current->context = NULL;
    current->display = NULL;

    if (!SetTLSValue(currentTLS, current))
    {
        ERR("Could not set thread local storage.");
        return NULL;
    }

    return current;
}

// Call this upon thread shutdown.
void DeallocateCurrent()
{
    Current *current = reinterpret_cast<Current*>(GetTLSValue(currentTLS));
    SafeDelete(current);
    SetTLSValue(currentTLS, NULL);
}

}

#ifdef ANGLE_PLATFORM_WINDOWS
extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
      case DLL_PROCESS_ATTACH:
        {
            if (!gl::CreateThreadLocalIndex())
            {
                return FALSE;
            }

#ifdef ANGLE_ENABLE_DEBUG_ANNOTATIONS
            gl::InitializeDebugAnnotations();
#endif
        }
        // Fall through to initialize index
      case DLL_THREAD_ATTACH:
        {
            gl::AllocateCurrent();
        }
        break;
      case DLL_THREAD_DETACH:
        {
            gl::DeallocateCurrent();
        }
        break;
      case DLL_PROCESS_DETACH:
        {
            gl::DeallocateCurrent();
            gl::DestroyThreadLocalIndex();

#ifdef ANGLE_ENABLE_DEBUG_ANNOTATIONS
            gl::UninitializeDebugAnnotations();
#endif
        }
        break;
      default:
        break;
    }

    return TRUE;
}
#endif

namespace gl
{

Current *GetCurrentData()
{
    Current *current = reinterpret_cast<Current*>(GetTLSValue(currentTLS));

    // ANGLE issue 488: when the dll is loaded after thread initialization,
    // thread local storage (current) might not exist yet.
    return (current ? current : AllocateCurrent());
}

void makeCurrent(Context *context, egl::Display *display, egl::Surface *surface)
{
    Current *current = GetCurrentData();

    current->context = context;
    current->display = display;

    if (context && display && surface)
    {
        context->makeCurrent(surface);
    }
}

Context *getContext()
{
    Current *current = GetCurrentData();

    return current->context;
}

Context *getNonLostContext()
{
    Context *context = getContext();

    if (context)
    {
        if (context->isContextLost())
        {
            gl::error(GL_OUT_OF_MEMORY);
            return NULL;
        }
        else
        {
            return context;
        }
    }
    return NULL;
}

egl::Display *getDisplay()
{
    Current *current = GetCurrentData();

    return current->display;
}

// Records an error code
void error(GLenum errorCode)
{
    gl::Context *context = glGetCurrentContext();
    context->recordError(Error(errorCode));

    switch (errorCode)
    {
      case GL_INVALID_ENUM:
        TRACE("\t! Error generated: invalid enum\n");
        break;
      case GL_INVALID_VALUE:
        TRACE("\t! Error generated: invalid value\n");
        break;
      case GL_INVALID_OPERATION:
        TRACE("\t! Error generated: invalid operation\n");
        break;
      case GL_OUT_OF_MEMORY:
        TRACE("\t! Error generated: out of memory\n");
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        TRACE("\t! Error generated: invalid framebuffer operation\n");
        break;
      default: UNREACHABLE();
    }
}

}
