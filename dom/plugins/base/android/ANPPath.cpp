#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_path_##name


// maybe this should store a list of actions (lineTo,
// moveTo), and when canvas_drawPath() we apply all of these
// actions to the gfxContext.

ANPPath*
anp_path_newPath()
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
  return 0;
}


void
anp_path_deletePath(ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


void
anp_path_copy(ANPPath* dst, const ANPPath* src)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


bool
anp_path_equal(const ANPPath* path0, const ANPPath* path1)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
  return false;
}


void
anp_path_reset(ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


bool
anp_path_isEmpty(const ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
  return false;
}


void
anp_path_getBounds(const ANPPath* p, ANPRectF* bounds)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);

  bounds->left = 0;
  bounds->top = 0;
  bounds->right = 1000;
  bounds->left = 1000;
}


void
anp_path_moveTo(ANPPath* p, float x, float y)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_lineTo(ANPPath* p, float x, float y)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_quadTo(ANPPath* p, float x0, float y0, float x1, float y1)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_cubicTo(ANPPath* p, float x0, float y0, float x1, float y1,
                      float x2, float y2)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_close(ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


void
anp_path_offset(ANPPath* src, float dx, float dy, ANPPath* dst)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


void
anp_path_transform(ANPPath* src, const ANPMatrix*, ANPPath* dst)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}



void InitPathInterface(ANPPathInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newPath);
  ASSIGN(i, deletePath);
  ASSIGN(i, copy);
  ASSIGN(i, equal);
  ASSIGN(i, reset);
  ASSIGN(i, isEmpty);
  ASSIGN(i, getBounds);
  ASSIGN(i, moveTo);
  ASSIGN(i, lineTo);
  ASSIGN(i, quadTo);
  ASSIGN(i, cubicTo);
  ASSIGN(i, close);
  ASSIGN(i, offset);
  ASSIGN(i, transform);
}
