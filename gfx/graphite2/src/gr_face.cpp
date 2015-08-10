/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#include "graphite2/Font.h"
#include "inc/Face.h"
#include "inc/FileFace.h"
#include "inc/GlyphCache.h"
#include "inc/CachedFace.h"
#include "inc/CmapCache.h"
#include "inc/Silf.h"
#include "inc/json.h"

using namespace graphite2;

#if !defined GRAPHITE2_NTRACING
extern json *global_log;
#endif

namespace
{
    bool load_face(Face & face, unsigned int options)
    {
#ifdef GRAPHITE2_TELEMETRY
        telemetry::category _misc_cat(face.tele.misc);
#endif
        Face::Table silf(face, Tag::Silf, 0x00050000);
        if (silf)   options &= ~gr_face_dumbRendering;
        else if (!(options &  gr_face_dumbRendering))
            return false;

        if (!face.readGlyphs(options))
            return false;

        if (silf)
        {
            if (!face.readFeatures() || !face.readGraphite(silf))
            {
#if !defined GRAPHITE2_NTRACING
                if (global_log)
                {
                    *global_log << json::object
                        << "type" << "fontload"
                        << "failure" << face.error()
                        << "context" << face.error_context()
                    << json::close;
                }
#endif
                return false;
            }
            else
                return true;
        }
        else
            return options & gr_face_dumbRendering;
    }
}

extern "C" {

gr_face* gr_make_face_with_ops(const void* appFaceHandle/*non-NULL*/, const gr_face_ops *ops, unsigned int faceOptions)
                  //the appFaceHandle must stay alive all the time when the gr_face is alive. When finished with the gr_face, call destroy_face    
{
    if (ops == 0)   return 0;

    Face *res = new Face(appFaceHandle, *ops);
    if (res && load_face(*res, faceOptions))
        return static_cast<gr_face *>(res);

    delete res;
    return 0;
}

gr_face* gr_make_face(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn tablefn, unsigned int faceOptions)
{
    const gr_face_ops ops = {sizeof(gr_face_ops), tablefn, NULL};
    return gr_make_face_with_ops(appFaceHandle, &ops, faceOptions);
}

#ifndef GRAPHITE2_NSEGCACHE
gr_face* gr_make_face_with_seg_cache_and_ops(const void* appFaceHandle/*non-NULL*/, const gr_face_ops *ops, unsigned int cacheSize, unsigned int faceOptions)
                  //the appFaceHandle must stay alive all the time when the GrFace is alive. When finished with the GrFace, call destroy_face
{
    if (ops == 0)   return 0;

    CachedFace *res = new CachedFace(appFaceHandle, *ops);
    if (res && load_face(*res, faceOptions)
            && res->setupCache(cacheSize))
        return static_cast<gr_face *>(static_cast<Face *>(res));

    delete res;
    return 0;
}

gr_face* gr_make_face_with_seg_cache(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable, unsigned int cacheSize, unsigned int faceOptions)
{
    const gr_face_ops ops = {sizeof(gr_face_ops), getTable, NULL};
    return gr_make_face_with_seg_cache_and_ops(appFaceHandle, &ops, cacheSize, faceOptions);
}
#endif

gr_uint32 gr_str_to_tag(const char *str)
{
    uint32 res = 0;
    int i = strlen(str);
    if (i > 4) i = 4;
    while (--i >= 0)
        res = (res >> 8) + (str[i] << 24);
    return res;
}

void gr_tag_to_str(gr_uint32 tag, char *str)
{
    int i = 4;
    while (--i >= 0)
    {
        str[i] = tag & 0xFF;
        tag >>= 8;
    }
}

inline
uint32 zeropad(const uint32 x)
{
    if (x == 0x20202020)                    return 0;
    if ((x & 0x00FFFFFF) == 0x00202020)     return x & 0xFF000000;
    if ((x & 0x0000FFFF) == 0x00002020)     return x & 0xFFFF0000;
    if ((x & 0x000000FF) == 0x00000020)     return x & 0xFFFFFF00;
    return x;
}

gr_feature_val* gr_face_featureval_for_lang(const gr_face* pFace, gr_uint32 langname/*0 means clone default*/) //clones the features. if none for language, clones the default
{
    assert(pFace);
    langname = zeropad(langname);
    return static_cast<gr_feature_val *>(pFace->theSill().cloneFeatures(langname));
}


const gr_feature_ref* gr_face_find_fref(const gr_face* pFace, gr_uint32 featId)  //When finished with the FeatureRef, call destroy_FeatureRef
{
    assert(pFace);
    featId = zeropad(featId);
    const FeatureRef* pRef = pFace->featureById(featId);
    return static_cast<const gr_feature_ref*>(pRef);
}

unsigned short gr_face_n_fref(const gr_face* pFace)
{
    assert(pFace);
    return pFace->numFeatures();
}

const gr_feature_ref* gr_face_fref(const gr_face* pFace, gr_uint16 i) //When finished with the FeatureRef, call destroy_FeatureRef
{
    assert(pFace);
    const FeatureRef* pRef = pFace->feature(i);
    return static_cast<const gr_feature_ref*>(pRef);
}

unsigned short gr_face_n_languages(const gr_face* pFace)
{
    assert(pFace);
    return pFace->theSill().numLanguages();
}

gr_uint32 gr_face_lang_by_index(const gr_face* pFace, gr_uint16 i)
{
    assert(pFace);
    return pFace->theSill().getLangName(i);
}


void gr_face_destroy(gr_face *face)
{
    delete face;
}


gr_uint16 gr_face_name_lang_for_locale(gr_face *face, const char * locale)
{
    if (face)
    {
        return face->languageForLocale(locale);
    }
    return 0;
}

unsigned short gr_face_n_glyphs(const gr_face* pFace)
{
    return pFace->glyphs().numGlyphs();
}

const gr_faceinfo *gr_face_info(const gr_face *pFace, gr_uint32 script)
{
    if (!pFace) return 0;
    const Silf *silf = pFace->chooseSilf(script);
    if (silf) return silf->silfInfo();
    return 0;
}

int gr_face_is_char_supported(const gr_face* pFace, gr_uint32 usv, gr_uint32 script)
{
    const Cmap & cmap = pFace->cmap();
    gr_uint16 gid = cmap[usv];
    if (!gid)
    {
        const Silf * silf = pFace->chooseSilf(script);
        gid = silf->findPseudo(usv);
    }
    return (gid != 0);
}

#ifndef GRAPHITE2_NFILEFACE
gr_face* gr_make_file_face(const char *filename, unsigned int faceOptions)
{
    FileFace* pFileFace = new FileFace(filename);
    if (*pFileFace)
    {
      gr_face* pRes = gr_make_face_with_ops(pFileFace, &FileFace::ops, faceOptions);
      if (pRes)
      {
        pRes->takeFileFace(pFileFace);        //takes ownership
        return pRes;
      }
    }
    
    //error when loading

    delete pFileFace;
    return NULL;
}

#ifndef GRAPHITE2_NSEGCACHE
gr_face* gr_make_file_face_with_seg_cache(const char* filename, unsigned int segCacheMaxSize, unsigned int faceOptions)   //returns NULL on failure. //TBD better error handling
                  //when finished with, call destroy_face
{
    FileFace* pFileFace = new FileFace(filename);
    if (*pFileFace)
    {
      gr_face * pRes = gr_make_face_with_seg_cache_and_ops(pFileFace, &FileFace::ops, segCacheMaxSize, faceOptions);
      if (pRes)
      {
        pRes->takeFileFace(pFileFace);        //takes ownership
        return pRes;
      }
    }

    //error when loading

    delete pFileFace;
    return NULL;
}
#endif
#endif      //!GRAPHITE2_NFILEFACE


} // extern "C"


