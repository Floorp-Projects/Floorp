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


