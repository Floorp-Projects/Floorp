/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
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

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_matrix_##name

/** Return a new identity matrix
 */
ANPMatrix*
anp_matrix_newMatrix()
{
  NOT_IMPLEMENTED();
  return 0;
}

/** Delete a matrix previously allocated by newMatrix()
 */
void
anp_matrix_deleteMatrix(ANPMatrix*)
{
  NOT_IMPLEMENTED();
}


ANPMatrixFlag
anp_matrix_getFlags(const ANPMatrix*)
{
  NOT_IMPLEMENTED();
  return 0;
}


void
anp_matrix_copy(ANPMatrix* dst, const ANPMatrix* src)
{
  NOT_IMPLEMENTED();
}


void
anp_matrix_get3x3(const ANPMatrix*, float[9])
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_set3x3(ANPMatrix*, const float[9])
{
  NOT_IMPLEMENTED();
}


void
anp_matrix_setIdentity(ANPMatrix*)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_preTranslate(ANPMatrix*, float tx, float ty)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_postTranslate(ANPMatrix*, float tx, float ty)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_preScale(ANPMatrix*, float sx, float sy)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_postScale(ANPMatrix*, float sx, float sy)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_preSkew(ANPMatrix*, float kx, float ky)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_postSkew(ANPMatrix*, float kx, float ky)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_preRotate(ANPMatrix*, float degrees)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_postRotate(ANPMatrix*, float degrees)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_preConcat(ANPMatrix*, const ANPMatrix*)
{
  NOT_IMPLEMENTED();
}

void
anp_matrix_postConcat(ANPMatrix*, const ANPMatrix*)
{
  NOT_IMPLEMENTED();
}


bool
anp_matrix_invert(ANPMatrix* dst, const ANPMatrix* src)
{
  NOT_IMPLEMENTED();
  return false;
}

void
anp_matrix_mapPoints(ANPMatrix*, float dst[], const float src[],
                                 int32_t count)
{
  NOT_IMPLEMENTED();
}


void InitMatrixInterface(ANPMatrixInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newMatrix);
  ASSIGN(i, deleteMatrix);
  ASSIGN(i, getFlags);
  ASSIGN(i, copy);
  ASSIGN(i, get3x3);
  ASSIGN(i, set3x3);
  ASSIGN(i, setIdentity);
  ASSIGN(i, preTranslate);
  ASSIGN(i, postTranslate);
  ASSIGN(i, preScale);
  ASSIGN(i, postScale);
  ASSIGN(i, preSkew);
  ASSIGN(i, postSkew);
  ASSIGN(i, preRotate);
  ASSIGN(i, postRotate);
  ASSIGN(i, preConcat);
  ASSIGN(i, postConcat);
  ASSIGN(i, invert);
  ASSIGN(i, mapPoints);
}


