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

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "mozilla-decoder.h"
#include <pango/pangoxft.h>
#include <pango/pangofc-fontmap.h>
#include <pango/pangofc-font.h>
#include <gdk/gdkx.h>

#include "nsString.h"
#include "nsIPersistentProperties2.h"
#include "nsNetUtil.h"
#include "nsReadableUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsICharRepresentable.h"
#include "nsCompressedCharMap.h"

#undef DEBUG_CUSTOM_ENCODER

G_DEFINE_TYPE (MozillaDecoder, mozilla_decoder, PANGO_TYPE_FC_DECODER)

MozillaDecoder *mozilla_decoder_new      (void);

static FcCharSet  *mozilla_decoder_get_charset (PangoFcDecoder *decoder,
                                                PangoFcFont    *fcfont);
static PangoGlyph  mozilla_decoder_get_glyph   (PangoFcDecoder *decoder,
                                                PangoFcFont    *fcfont,
                                                guint32         wc);

static PangoFcDecoder *mozilla_find_decoder    (FcPattern *pattern,
                                                gpointer   user_data);

typedef struct _MozillaDecoderPrivate MozillaDecoderPrivate;

#define MOZILLA_DECODER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MOZILLA_TYPE_DECODER, MozillaDecoderPrivate))

struct _MozillaDecoderPrivate {
    char *family;
    char *encoder;
    char *cmap;
    gboolean is_wide;
    FcCharSet *charset;
    nsCOMPtr<nsIUnicodeEncoder> uEncoder;
};

static nsICharsetConverterManager *gCharsetManager = NULL;

static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);

// Hash tables that hold the custom encodings and custom cmaps used in
// various fonts.
GHashTable *encoder_hash = NULL;
GHashTable *cmap_hash = NULL;
GHashTable *wide_hash = NULL;

void
mozilla_decoder_init (MozillaDecoder *decoder)
{
}

void
mozilla_decoder_class_init (MozillaDecoderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    PangoFcDecoderClass *parent_class = PANGO_FC_DECODER_CLASS (klass);

    /*   object_class->finalize = test_finalize; */

    parent_class->get_charset = mozilla_decoder_get_charset;
    parent_class->get_glyph = mozilla_decoder_get_glyph;

    g_type_class_add_private (object_class, sizeof (MozillaDecoderPrivate));
}

MozillaDecoder *
mozilla_decoder_new(void)
{
    return (MozillaDecoder *)g_object_new(MOZILLA_TYPE_DECODER, NULL);
}

#ifdef DEBUG_CUSTOM_ENCODER
void
dump_hash(char *key, char *val, void *arg)
{
    printf("%s -> %s\n", key, val);
}
#endif

/**
 * mozilla_decoders_init:
 *
 * #mozilla_decoders_init:
 *
 * This initializes all of the application-specific custom decoders
 * that Mozilla uses.  This should only be called once during the
 * lifetime of the application.
 *
 * Return value: zero on success, not zero on failure.
 *
 **/

int
mozilla_decoders_init(void)
{
    static PRBool initialized = PR_FALSE;
    if (initialized)
        return 0;

    encoder_hash = g_hash_table_new(g_str_hash, g_str_equal);
    cmap_hash = g_hash_table_new(g_str_hash, g_str_equal);
    wide_hash = g_hash_table_new(g_str_hash, g_str_equal);

    PRBool dumb = PR_FALSE;
    nsCOMPtr<nsIPersistentProperties> props;
    nsCOMPtr<nsISimpleEnumerator> encodeEnum;

    NS_LoadPersistentPropertiesFromURISpec(getter_AddRefs(props),
        NS_LITERAL_CSTRING("resource://gre/res/fonts/pangoFontEncoding.properties"));

    if (!props)
        goto loser;

    // Enumerate the properties in this file and figure out all of the
    // fonts for which we have custom encodings.
    props->Enumerate(getter_AddRefs(encodeEnum));
    if (!encodeEnum)
        goto loser;

    while (encodeEnum->HasMoreElements(&dumb), dumb) {
        nsCOMPtr<nsIPropertyElement> prop;
        encodeEnum->GetNext(getter_AddRefs(prop));
        if (!prop)
            goto loser;

        nsCAutoString name;
        prop->GetKey(name);
        nsAutoString value;
        prop->GetValue(value);

        if (!StringBeginsWith(name, NS_LITERAL_CSTRING("encoding."))) {
            printf("string doesn't begin with encoding?\n");
            continue;
        }

        name = Substring(name, 9);

        if (StringEndsWith(name, NS_LITERAL_CSTRING(".ttf"))) {
            name = Substring(name, 0, name.Length() - 4);

            // Strip off a .wide if it's there.
            if (StringEndsWith(value, NS_LITERAL_STRING(".wide"))) {
                g_hash_table_insert(wide_hash, g_strdup(name.get()),
                                    g_strdup("wide"));
                value = Substring(value, 0, name.Length() - 5);
            }

            g_hash_table_insert(encoder_hash,
                                g_strdup(name.get()),
                                g_strdup(NS_ConvertUTF16toUTF8(value).get()));
        }
        else if (StringEndsWith(name, NS_LITERAL_CSTRING(".ftcmap"))) {
            name = Substring(name, 0, name.Length() - 7);
            g_hash_table_insert(cmap_hash,
                                g_strdup(name.get()),
                                g_strdup(NS_ConvertUTF16toUTF8(value).get()));
        }
        else {
            printf("unknown suffix used for mapping\n");
        }
    }

    pango_fc_font_map_add_decoder_find_func(PANGO_FC_FONT_MAP(pango_xft_get_font_map(GDK_DISPLAY(),gdk_x11_get_default_screen())),
                                            mozilla_find_decoder,
                                            NULL,
                                            NULL);

    initialized = PR_TRUE;

#ifdef DEBUG_CUSTOM_ENCODER
    printf("*** encoders\n");
    g_hash_table_foreach(encoder_hash, (GHFunc)dump_hash, NULL);

    printf("*** cmaps\n");
    g_hash_table_foreach(cmap_hash, (GHFunc)dump_hash, NULL);
#endif

    return 0;

 loser:
    return -1;
}

FcCharSet *
mozilla_decoder_get_charset (PangoFcDecoder *decoder,
                             PangoFcFont    *fcfont)
{
    MozillaDecoderPrivate *priv = MOZILLA_DECODER_GET_PRIVATE(decoder);

    if (priv->charset)
        return priv->charset;

    // First time this has been accessed.  Populate the charset.
    priv->charset = FcCharSetCreate();

    if (!gCharsetManager) {
        nsServiceManager::GetService(kCharsetConverterManagerCID,
        NS_GET_IID(nsICharsetConverterManager), (nsISupports**)&gCharsetManager);
    }

    nsCOMPtr<nsIUnicodeEncoder> encoder;
    nsCOMPtr<nsICharRepresentable> represent;

    if (!gCharsetManager)
        goto end;

    gCharsetManager->GetUnicodeEncoderRaw(priv->encoder, getter_AddRefs(encoder));
    if (!encoder)
        goto end;
    
    encoder->SetOutputErrorBehavior(encoder->kOnError_Replace, nsnull, '?');

    priv->uEncoder = encoder;

    represent = do_QueryInterface(encoder);
    if (!represent)
        goto end;

    PRUint32 map[UCS2_MAP_LEN];
    memset(map, 0, sizeof(map));

    represent->FillInfo(map);

    for (int i = 0; i < NUM_UNICODE_CHARS; i++) {
        if (IS_REPRESENTABLE(map, i))
            FcCharSetAddChar(priv->charset, i);
    }

 end:
    return priv->charset;
}

PangoGlyph
mozilla_decoder_get_glyph   (PangoFcDecoder *decoder,
                             PangoFcFont    *fcfont,
                             guint32         wc)
{
    MozillaDecoderPrivate *priv = MOZILLA_DECODER_GET_PRIVATE(decoder);

    PangoGlyph retval = 0;
    PRUnichar inchar = wc;
    PRInt32 inlen = 1;
    char outchar[2] = {0,0};
    PRInt32 outlen = 2;

    priv->uEncoder->Convert(&inchar, &inlen, outchar, &outlen);
    if (outlen != 1) {
        printf("Warning: mozilla_decoder_get_glyph doesn't support more than one character conversions.\n");
        return 0;
    }

    FT_Face face = pango_fc_font_lock_face(fcfont);

#ifdef DEBUG_CUSTOM_ENCODER
    char *filename;
    FcPatternGetString(fcfont->font_pattern, FC_FILE, 0, (FcChar8 **)&filename);
    printf("filename is %s\n", filename);
#endif

    // Make sure to set the right charmap before trying to get the
    // glyph
    if (priv->cmap) {
        if (!strcmp(priv->cmap, "mac_roman")) {
            FT_Select_Charmap(face, ft_encoding_apple_roman);
        }
        else if (!strcmp(priv->cmap, "unicode")) {
            FT_Select_Charmap(face, ft_encoding_unicode);
        }
        else {
            printf("Warning: Invalid charmap entry for family %s\n",
                   priv->family);
        }
    }

    // Standard 8 bit to glyph translation
    if (!priv->is_wide) {
        FcChar32 blah = PRUint8(outchar[0]);
        retval = FT_Get_Char_Index(face, blah);
#ifdef DEBUG_CUSTOM_ENCODER
        printf("wc 0x%x outchar[0] 0x%x index 0x%x retval 0x%x face %p\n",
               wc, outchar[0], blah, retval, (void *)face);
#endif
    }
    else {
        printf("Warning: We don't support .wide fonts!\n");
        retval = 0;
    }

    pango_fc_font_unlock_face(fcfont);

    return retval;
}

PangoFcDecoder *
mozilla_find_decoder (FcPattern *pattern, gpointer user_data)
{
    // Compare the family name of the font that's been opened to see
    // if we have a custom decoder.
    const char *orig = NULL;
    FcPatternGetString(pattern, FC_FAMILY, 0, (FcChar8 **)&orig);

    nsCAutoString family;
    family.Assign(orig);

    family.StripWhitespace();
    ToLowerCase(family);

    char *encoder = (char *)g_hash_table_lookup(encoder_hash, family.get());
    if (!encoder)
        return NULL;

    MozillaDecoder *decoder = mozilla_decoder_new();

    MozillaDecoderPrivate *priv = MOZILLA_DECODER_GET_PRIVATE(decoder);

    priv->family = g_strdup(family.get());
    priv->encoder = g_strdup(encoder);

    char *cmap = (char *)g_hash_table_lookup(cmap_hash, family.get());
    if (cmap)
        priv->cmap = g_strdup(cmap);

    char *wide = (char *)g_hash_table_lookup(wide_hash, family.get());
    if (wide)
        priv->is_wide = TRUE;

    return PANGO_FC_DECODER(decoder);
}
