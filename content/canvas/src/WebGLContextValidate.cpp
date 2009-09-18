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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Mark Steele <mwsteele@gmail.com>
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
