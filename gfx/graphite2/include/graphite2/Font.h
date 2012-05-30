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

    Alternatively, the contents of this file may be used under the terms
    of the Mozilla Public License (http://mozilla.org/MPL) or the GNU
    General Public License, as published by the Free Software Foundation,
    either version 2 of the License or (at your option) any later version.
*/
#pragma once

#include "graphite2/Types.h"

#define GR2_VERSION_MAJOR   1
#define GR2_VERSION_MINOR   1
#define GR2_VERSION_BUGFIX  3

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct gr_face          gr_face;
typedef struct gr_font          gr_font;
typedef struct gr_feature_ref   gr_feature_ref;
typedef struct gr_feature_val   gr_feature_val;

/**
* Returns version information on this engine
*/
GR2_API void gr_engine_version(int *nMajor, int *nMinor, int *nBugFix);

/**
* The Face Options allow the application to require that certain tables are
* read during face construction. This may be of concern if the appFaceHandle
* used in the gr_get_table_fn may change.
* The values can be combined 
*/
enum gr_face_options {
    /** No preload, no cmap caching, fail if the graphite tables are invalid */
    gr_face_default = 0,
    /** Dumb rendering will be enabled if the graphite tables are invalid */
    gr_face_dumbRendering = 1,
    /** preload glyphs at construction time */
    gr_face_preloadGlyphs = 2,
    /** Cache the lookup from code point to glyph ID at construction time */
    gr_face_cacheCmap = 4,
    /** Preload everything */
    gr_face_preloadAll = 6
};

/** type describing function to retrieve font table information
  *
  * @return a pointer to the table in memory. The pointed to memory must exist as
  *          long as the gr_face which makes the call.
  * @param appFaceHandle is the unique information passed to gr_make_face()
  * @param name is a 32bit tag to the table name.
  * @param len returned by this function to say how long the table is in memory.
  */
typedef const void *(*gr_get_table_fn)(const void* appFaceHandle, unsigned int name, size_t *len);

/** Create a gr_face object given application information and a getTable function
  *
  * @return gr_face or NULL if the font fails to load for some reason.
  * @param appFaceHandle This is application specific information that is passed to the getTable
  *                      function. The appFaceHandle must stay alive as long as the gr_face is alive.
  * @param getTable  This function is called whenever graphite needs access to a table of data
  *                  in the font.
  * @param faceOptions   Bitfield describing various options. See enum gr_face_options for details.
  */
GR2_API gr_face* gr_make_face(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable, unsigned int faceOptions);

//#ifndef GRAPHITE2_NSEGCACHE
/** Create a gr_face object given application information, with subsegmental caching support
  *
  * @return gr_face or NULL if the font fails to load.
  * @param appFaceHandle is a pointer to application specific information that is passed to getTable.
  *                      This may not be NULL and must stay alive as long as the gr_face is alive.
  * @param getTable  The function graphite calls to access font table data
  * @param segCacheMaxSize   How large the segment cache is.
  * @param faceOptions   Bitfield of values from enum gr_face_options
  */
GR2_API gr_face* gr_make_face_with_seg_cache(const void* appFaceHandle, gr_get_table_fn getTable, unsigned int segCacheMaxSize, unsigned int faceOptions);
//#endif

/** Convert a tag in a string into a gr_uint32
  *
  * @return gr_uint32 tag, zero padded
  * @param str a nul terminated string of which at most the first 4 characters are read
  */
GR2_API gr_uint32 gr_str_to_tag(const char *str);

/** Convert a gr_uint32 tag into a string
  *
  * @param tag contains the tag to convert
  * @param str is a pointer to a char array of at least size 4 bytes. The first 4 bytes of this array
  *            will be overwritten by this function. No nul is appended.
  */
GR2_API void gr_tag_to_str(gr_uint32 tag, char *str);

/** Get feature values for a given language or default
  *
  * @return a copy of the default feature values for a given language. The application must call
  *          gr_featureval_destroy() to free this object when done.
  * @param pFace The font face to get feature values from
  * @param langname The language tag to get feature values for. If there is no such language or
  *                  langname is 0, the default feature values for the font are returned.
  *                  langname is right 0 padded and assumes lowercase. Thus the en langauge
  *                  would be 0x656E0000. Langname may also be space padded, thus 0x656E2020.
  */
GR2_API gr_feature_val* gr_face_featureval_for_lang(const gr_face* pFace, gr_uint32 langname);

/** Get feature reference for a given feature id from a face
  *
  * @return a feature reference corresponding to the given id. This data is part of the gr_face and
  *          will be freed when the face is destroyed.
  * @param pFace Font face to get information on.
  * @param featId    Feature id tag to get reference to.
  */
GR2_API const gr_feature_ref* gr_face_find_fref(const gr_face* pFace, gr_uint32 featId);

/** Returns number of feature references in a face **/
GR2_API gr_uint16 gr_face_n_fref(const gr_face* pFace);

/** Returns feature reference at given index in face **/
GR2_API const gr_feature_ref* gr_face_fref(const gr_face* pFace, gr_uint16 i);

/** Return number of languages the face knows about **/
GR2_API unsigned short gr_face_n_languages(const gr_face* pFace);

/** Returns a language id corresponding to a language of given index in the face **/
GR2_API gr_uint32 gr_face_lang_by_index(const gr_face* pFace, gr_uint16 i);

/** Destroy the given face and free its memory **/
GR2_API void gr_face_destroy(gr_face *face);

/** Returns the number of glyphs in the face **/
GR2_API unsigned short gr_face_n_glyphs(const gr_face* pFace);

#ifndef GRAPHITE2_NFILEFACE
/** Create gr_face from a font file
  *
  * @return gr_face that accesses a font file directly. Returns NULL on failure.
  * @param filename Full path and filename to font file
  * @param faceOptions Bitfile from enum gr_face_options to control face options.
  */
GR2_API gr_face* gr_make_file_face(const char *filename, unsigned int faceOptions);

//#ifndef GRAPHITE2_NSEGCACHE
/** Create gr_face from a font file, with subsegment caching support.
  *
  * @return gr_face that accesses a font file directly. Returns NULL on failure.
  * @param filename Full path and filename to font file
  * @param segCacheMaxSize Specifies how big to make the cache in segments.
  * @param faceOptions   Bitfield from enum gr_face_options to control face options.
  */
GR2_API gr_face* gr_make_file_face_with_seg_cache(const char *filename, unsigned int segCacheMaxSize, unsigned int faceOptions);
//#endif
#endif      // !GRAPHITE2_NFILEFACE

/** Create a font from a face
  *
  * @return gr_font Call font_destroy to free this font
  * @param ppm Resolution of the font in pixels per em
  * @param face Face this font corresponds to. This must stay alive as long as the font is alive.
  */
GR2_API gr_font* gr_make_font(float ppm, const gr_face *face);

/** query function to find the hinted advance width of a glyph **/
typedef float (*gr_advance_fn)(const void* appFontHandle, gr_uint16 glyphid);

/** Creates a font with hinted advance width query function
  *
  * @return gr_font to be destroyed via font_destroy
  * @param ppm size of font in pixels per em
  * @param appFontHandle font specific information that must stay alive as long as the font does
  * @param advance function to call with appFontHandle and glyphid to get horizontal advance in pixels.
  * @param face the face this font corresponds to. Must stay alive as long as the font does.
  */
GR2_API gr_font* gr_make_font_with_advance_fn(float ppm, const void* appFontHandle, gr_advance_fn advance, const gr_face *face);

/** Free a font **/
GR2_API void gr_font_destroy(gr_font *font);

/** get a feature value
  *
  * @return value of specific feature or 0 if any problems.
  * @param pfeatureref   gr_feature_ref to the feature
  * @param feats gr_feature_val containing all the values
  */
GR2_API gr_uint16 gr_fref_feature_value(const gr_feature_ref* pfeatureref, const gr_feature_val* feats);

/** set a feature value
  *
  * @return false if there were any problems (value out of range, etc.)
  * @param pfeatureref   gr_feature_ref to the feature
  * @param val   value to set the feature to
  * @param pDest the gr_feature_val containing all the values for all the features
  */
GR2_API int gr_fref_set_feature_value(const gr_feature_ref* pfeatureref, gr_uint16 val, gr_feature_val* pDest);

/** Returns the id tag for a gr_feature_ref **/
GR2_API gr_uint32 gr_fref_id(const gr_feature_ref* pfeatureref);

/** Returns number of values a feature may take, given a gr_feature_ref **/
GR2_API gr_uint16 gr_fref_n_values(const gr_feature_ref* pfeatureref);

/** Returns the value associated with a particular value in a feature
  *
  * @return value
  * @param pfeatureref gr_feature_ref of the feature of interest
  * @param settingno   Index up to the return value of gr_fref_n_values() of the value
  */
GR2_API gr_int16 gr_fref_value(const gr_feature_ref* pfeatureref, gr_uint16 settingno);   

/** Returns a string of the UI name of a feature
  *
  * @return string of the UI name, in the encoding form requested. Call gr_label_destroy() after use.
  * @param pfeatureref   gr_feature_ref of the feature
  * @param langId    This is a pointer since the face may not support a string in the requested
  *                  language. The actual language of the string is returned in langId
  * @param utf   Encoding form for the string
  * @param length    Used to return the length of the string returned in bytes.
  */
GR2_API void* gr_fref_label(const gr_feature_ref* pfeatureref, gr_uint16 *langId, enum gr_encform utf, gr_uint32 *length);

/** Return a UI string for a possible value of a feature
  *
  * @return string of the UI name, in the encoding form requested. nul terminated. Call gr_label_destroy()
  *          after use.
  * @param pfeatureref   gr_feature_ref of the feature
  * @param settingno     Value setting index
  * @param langId        This is a pointer to the requested language. The requested language id is
  *                      replaced by the actual language id of the string returned.
  * @param utf   Encoding form for the string
  * @param length    Returns the length of the string returned in bytes.
  */
GR2_API void* gr_fref_value_label(const gr_feature_ref* pfeatureref, gr_uint16 settingno/*rather than a value*/, gr_uint16 *langId, enum gr_encform utf, gr_uint32 *length);

/** Destroy a previously returned label string **/
GR2_API void gr_label_destroy(void * label);

/** Copies a gr_feature_val **/
GR2_API gr_feature_val* gr_featureval_clone(const gr_feature_val* pfeatures);

/** Destroys a gr_feature_val **/
GR2_API void gr_featureval_destroy(gr_feature_val *pfeatures);

#ifdef __cplusplus
}
#endif

