/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "WebGLContext.h"

using namespace mozilla;

/*
 * Verify that we can read count consecutive elements from each bound VBO.
 */

PRBool
WebGLContext::ValidateBuffers(PRUint32 count)
{
    GLint len = 0;
    GLint enabled = 0, size = 4, type = LOCAL_GL_FLOAT, binding = 0;
    PRBool someEnabled = PR_FALSE;
    GLint currentProgram = -1;
    GLint numAttributes = -1;

    MakeContextCurrent();

    // XXX cache this per program
    gl->fGetIntegerv(LOCAL_GL_CURRENT_PROGRAM, &currentProgram);
    if (currentProgram == -1) {
        // what?
        LogMessage("glGetIntegerv GL_CURRENT_PROGRAM failed: 0x%08x", (uint) gl->fGetError());
        return PR_FALSE;
    }

    gl->fGetProgramiv(currentProgram, LOCAL_GL_ACTIVE_ATTRIBUTES, &numAttributes);
    if (numAttributes == -1) {
        // what?
        LogMessage("glGetProgramiv GL_ACTIVE_ATTRIBUTES failed: 0x%08x", (uint) gl->fGetError());
        return PR_FALSE;
    }

    // is this valid?
    if (numAttributes > (GLint) mAttribBuffers.Length()) {
        // what?
        LogMessage("GL_ACTIVE_ATTRIBUTES > GL_MAX_VERTEX_ATTRIBS");
        return PR_FALSE;
    }
    PRUint32 maxAttribs = numAttributes;

    for (PRUint32 i = 0; i < maxAttribs; ++i) {
      WebGLVertexAttribData& vd = mAttribBuffers[i];

      // is this a problem?
      if (!vd.enabled)
	continue;

      if (vd.buf == nsnull) {
	LogMessage("No VBO bound to index %d (or it's been deleted)!", i);
	return PR_FALSE;
      }

      GLuint needed = vd.offset + (vd.stride ? vd.stride : vd.size) * count;
      if (vd.buf->Count() < needed) {
	LogMessage("VBO too small for bound attrib index %d: need at least %d elements, but have only %d", i, needed, vd.buf->Count());
	return PR_FALSE;
      }
    }

    return PR_TRUE;
}
