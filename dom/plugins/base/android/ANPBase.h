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

#include "android_npapi.h"
#include <stdlib.h>
#include "nsAutoPtr.h"
#include "gfxFont.h"
#include "nsISupportsImpl.h"

#define NOT_IMPLEMENTED_FATAL() do {                                    \
    __android_log_print(ANDROID_LOG_ERROR, "GeckoPlugins",              \
                        "%s not implemented %s, %d",                    \
                        __PRETTY_FUNCTION__, __FILE__, __LINE__);       \
    abort();                                                            \
  } while(0)

#define NOT_IMPLEMENTED()                                               \
    __android_log_print(ANDROID_LOG_ERROR, "GeckoPlugins",              \
                        "!!!!!!!!!!!!!!  %s not implemented %s, %d",    \
                        __PRETTY_FUNCTION__, __FILE__, __LINE__);       \

class gfxFont;

void InitAudioTrackInterface(ANPAudioTrackInterfaceV0 *i);
void InitBitmapInterface(ANPBitmapInterfaceV0 *i);
void InitCanvasInterface(ANPCanvasInterfaceV0 *i);
void InitEventInterface(ANPEventInterfaceV0 *i);
void InitLogInterface(ANPLogInterfaceV0 *i);
void InitMatrixInterface(ANPMatrixInterfaceV0 *i);
void InitPaintInterface(ANPPaintInterfaceV0 *i);
void InitPathInterface(ANPPathInterfaceV0 *i);
void InitSurfaceInterface(ANPSurfaceInterfaceV0 *i);
void InitSystemInterface(ANPSystemInterfaceV0 *i);
void InitTypeFaceInterface(ANPTypefaceInterfaceV0 *i);
void InitWindowInterface(ANPWindowInterfaceV0 *i);

struct ANPTypeface {
  gfxFont* mFont;
  nsAutoRefCnt mRefCnt;
};


typedef struct {
  ANPMatrixFlag flags;
  ANPColor color;
  ANPPaintStyle style;
  float strokeWidth;
  float strokeMiter;
  ANPPaintCap paintCap;
  ANPPaintJoin paintJoin;
  ANPTextEncoding textEncoding;
  ANPPaintAlign paintAlign;
  float textSize;
  float textScaleX;
  float textSkewX;
  ANPTypeface typeface;
} ANPPaintPrivate;
