/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is character set conversion utility macros.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alec Flett <alecf@flett.org>
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
