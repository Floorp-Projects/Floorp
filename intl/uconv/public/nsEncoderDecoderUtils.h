/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEncoderDecoderUtils_h__
#define nsEncoderDecoderUtils_h__

#define NS_ERROR_UCONV_NOCONV \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_UCONV, 0x01)

#define NS_SUCCESS_USING_FALLBACK_LOCALE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_UCONV, 0x02)

#define NS_UNICODEDECODER_NAME "Charset Decoders"
#define NS_UNICODEENCODER_NAME "Charset Encoders"

#define NS_DATA_BUNDLE_CATEGORY     "uconv-charset-data"
#define NS_TITLE_BUNDLE_CATEGORY    "uconv-charset-titles"

#define NS_CONVERTER_REGISTRY_START \
  static const mozilla::Module::CategoryEntry kUConvCategories[] = {

#define NS_CONVERTER_REGISTRY_END \
  { NULL } \
  };

#define NS_UCONV_REG_UNREG_DECODER(_Charset, _CID)          \
  { NS_UNICODEDECODER_NAME, _Charset, "" },
  
#define NS_UCONV_REG_UNREG_ENCODER(_Charset, _CID)          \
  { NS_UNICODEENCODER_NAME, _Charset, "" },

#define NS_UCONV_REG_UNREG(_Charset, _DecoderCID, _EncoderCID) \
  NS_UCONV_REG_UNREG_DECODER(_Charset, *) \
  NS_UCONV_REG_UNREG_ENCODER(_Charset, *)

#endif
