/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2004 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "mozilla-decoder.h"

G_DEFINE_TYPE (MozillaDecoder, mozilla_decoder, MOZILLA_TYPE_DECODER)

static FcCharSet  *mozilla_decoder_get_charset (PangoFcDecoder *decoder,
						PangoFcFont    *fcfont);
static PangoGlyph  mozilla_decoder_get_glyph   (PangoFcDecoder *decoder,
						PangoFcFont    *fcfont,
						guint32         wc);

void
mozilla_decoder_init (MozillaDecoder *decoder)
{
}

void
mozilla_decoder_class_init (MozillaDecoderClass *klass)
{
  /* GObjectClass *object_class = G_OBJECT_CLASS(klass); */
  PangoFcDecoderClass *parent_class = PANGO_FC_DECODER_CLASS (klass);

  /*   object_class->finalize = test_finalize; */

  parent_class->get_charset = mozilla_decoder_get_charset;
  parent_class->get_glyph = mozilla_decoder_get_glyph;
}

MozillaDecoder *
mozilla_decoder_new(void)
{
  return (MozillaDecoder *)g_object_new (MOZILLA_TYPE_DECODER, NULL);
}

FcCharSet *
mozilla_decoder_get_charset (PangoFcDecoder *decoder,
			     PangoFcFont    *fcfont)
{
  return NULL;
}

PangoGlyph
mozilla_decoder_get_glyph   (PangoFcDecoder *decoder,
			     PangoFcFont    *fcfont,
			     guint32         wc)
{
  return 0;
}
