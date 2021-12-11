/* vim:set ts=2 sw=2 sts=2 cin et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIconChannel.h"

#include <stdlib.h>
#include <unistd.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/EndianUtils.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/ipc/ByteBuf.h"
#include <algorithm>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include "nsMimeTypes.h"
#include "nsIMIMEService.h"

#include "nsServiceManagerUtils.h"

#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsIStringStream.h"
#include "nsServiceManagerUtils.h"
#include "nsIURL.h"
#include "nsIPipe.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "prlink.h"
#include "gfxPlatform.h"

using mozilla::CheckedInt32;
using mozilla::ipc::ByteBuf;

NS_IMPL_ISUPPORTS(nsIconChannel, nsIRequest, nsIChannel)

static nsresult MozGdkPixbufToByteBuf(GdkPixbuf* aPixbuf, ByteBuf* aByteBuf) {
  int width = gdk_pixbuf_get_width(aPixbuf);
  int height = gdk_pixbuf_get_height(aPixbuf);
  NS_ENSURE_TRUE(height < 256 && width < 256 && height > 0 && width > 0 &&
                     gdk_pixbuf_get_colorspace(aPixbuf) == GDK_COLORSPACE_RGB &&
                     gdk_pixbuf_get_bits_per_sample(aPixbuf) == 8 &&
                     gdk_pixbuf_get_has_alpha(aPixbuf) &&
                     gdk_pixbuf_get_n_channels(aPixbuf) == 4,
                 NS_ERROR_UNEXPECTED);

  const int n_channels = 4;
  CheckedInt32 buf_size =
      4 + n_channels * CheckedInt32(height) * CheckedInt32(width);
  if (!buf_size.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  uint8_t* const buf = (uint8_t*)moz_xmalloc(buf_size.value());
  uint8_t* out = buf;

  *(out++) = width;
  *(out++) = height;
  *(out++) = uint8_t(mozilla::gfx::SurfaceFormat::OS_RGBA);

  // Set all bits to ensure in nsIconDecoder we color manage and premultiply.
  *(out++) = 0xFF;

  const guchar* const pixels = gdk_pixbuf_get_pixels(aPixbuf);
  int instride = gdk_pixbuf_get_rowstride(aPixbuf);
  int outstride = width * n_channels;

  // encode the RGB data and the A data and adjust the stride as necessary.
  mozilla::gfx::SwizzleData(pixels, instride,
                            mozilla::gfx::SurfaceFormat::R8G8B8A8, out,
                            outstride, mozilla::gfx::SurfaceFormat::OS_RGBA,
                            mozilla::gfx::IntSize(width, height));

  *aByteBuf = ByteBuf(buf, buf_size.value(), buf_size.value());
  return NS_OK;
}

static nsresult ByteBufToStream(ByteBuf&& aBuf, nsIInputStream** aStream) {
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // stream takes ownership of buf and will free it on destruction.
  // This function cannot fail.
  rv = stream->AdoptData(reinterpret_cast<char*>(aBuf.mData), aBuf.mLen);
  MOZ_ASSERT(CheckedInt32(aBuf.mLen).isValid(),
             "aBuf.mLen should fit in int32_t");
  aBuf.mData = nullptr;

  // If this no longer holds then re-examine buf's lifetime.
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  NS_ENSURE_SUCCESS(rv, rv);

  stream.forget(aStream);
  return NS_OK;
}

static nsresult StreamToChannel(already_AddRefed<nsIInputStream> aStream,
                                nsIURI* aURI, nsIChannel** aChannel) {
  // nsIconProtocolHandler::NewChannel will provide the correct loadInfo for
  // this iconChannel. Use the most restrictive security settings for the
  // temporary loadInfo to make sure the channel can not be opened.
  nsCOMPtr<nsIPrincipal> nullPrincipal =
      mozilla::NullPrincipal::CreateWithoutOriginAttributes();
  return NS_NewInputStreamChannel(
      aChannel, aURI, std::move(aStream), nullPrincipal,
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
      nsIContentPolicy::TYPE_INTERNAL_IMAGE, nsLiteralCString(IMAGE_ICON_MS));
}

static GtkWidget* gProtoWindow = nullptr;
static GtkWidget* gStockImageWidget = nullptr;

static void ensure_stock_image_widget() {
  // Only the style of the GtkImage needs to be used, but the widget is kept
  // to track dynamic style changes.
  if (!gProtoWindow) {
    gProtoWindow = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget* protoLayout = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(gProtoWindow), protoLayout);

    gStockImageWidget = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(protoLayout), gStockImageWidget);

    gtk_widget_ensure_style(gStockImageWidget);
  }
}

static GtkIconSize moz_gtk_icon_size(const char* name) {
  if (strcmp(name, "button") == 0) {
    return GTK_ICON_SIZE_BUTTON;
  }

  if (strcmp(name, "menu") == 0) {
    return GTK_ICON_SIZE_MENU;
  }

  if (strcmp(name, "toolbar") == 0) {
    return GTK_ICON_SIZE_LARGE_TOOLBAR;
  }

  if (strcmp(name, "toolbarsmall") == 0) {
    return GTK_ICON_SIZE_SMALL_TOOLBAR;
  }

  if (strcmp(name, "dnd") == 0) {
    return GTK_ICON_SIZE_DND;
  }

  if (strcmp(name, "dialog") == 0) {
    return GTK_ICON_SIZE_DIALOG;
  }

  return GTK_ICON_SIZE_MENU;
}

static int32_t GetIconSize(nsIMozIconURI* aIconURI) {
  nsAutoCString iconSizeString;

  aIconURI->GetIconSize(iconSizeString);
  if (iconSizeString.IsEmpty()) {
    uint32_t size;
    mozilla::DebugOnly<nsresult> rv = aIconURI->GetImageSize(&size);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetImageSize failed");
    return size;
  }
  int size;

  GtkIconSize icon_size = moz_gtk_icon_size(iconSizeString.get());
  gtk_icon_size_lookup(icon_size, &size, nullptr);
  return size;
}

/* Scale icon buffer to preferred size */
static nsresult ScaleIconBuf(GdkPixbuf** aBuf, int32_t iconSize) {
  // Scale buffer only if width or height differ from preferred size
  if (gdk_pixbuf_get_width(*aBuf) != iconSize &&
      gdk_pixbuf_get_height(*aBuf) != iconSize) {
    GdkPixbuf* scaled =
        gdk_pixbuf_scale_simple(*aBuf, iconSize, iconSize, GDK_INTERP_BILINEAR);
    // replace original buffer by scaled
    g_object_unref(*aBuf);
    *aBuf = scaled;
    if (!scaled) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_OK;
}

/* static */
nsresult nsIconChannel::GetIconWithGIO(nsIMozIconURI* aIconURI,
                                       ByteBuf* aDataOut) {
  GIcon* icon = nullptr;
  nsCOMPtr<nsIURL> fileURI;

  // Read icon content
  aIconURI->GetIconURL(getter_AddRefs(fileURI));

  // Get icon for file specified by URI
  if (fileURI) {
    nsAutoCString spec;
    fileURI->GetAsciiSpec(spec);
    if (fileURI->SchemeIs("file")) {
      GFile* file = g_file_new_for_uri(spec.get());
      GFileInfo* fileInfo =
          g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON,
                            G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
      g_object_unref(file);
      if (fileInfo) {
        // icon from g_content_type_get_icon doesn't need unref
        icon = g_file_info_get_icon(fileInfo);
        if (icon) {
          g_object_ref(icon);
        }
        g_object_unref(fileInfo);
      }
    }
  }

  // Try to get icon by using MIME type
  if (!icon) {
    nsAutoCString type;
    aIconURI->GetContentType(type);
    // Try to get MIME type from file extension by using nsIMIMEService
    if (type.IsEmpty()) {
      nsCOMPtr<nsIMIMEService> ms(do_GetService("@mozilla.org/mime;1"));
      if (ms) {
        nsAutoCString fileExt;
        aIconURI->GetFileExtension(fileExt);
        ms->GetTypeFromExtension(fileExt, type);
      }
    }
    char* ctype = nullptr;  // character representation of content type
    if (!type.IsEmpty()) {
      ctype = g_content_type_from_mime_type(type.get());
    }
    if (ctype) {
      icon = g_content_type_get_icon(ctype);
      g_free(ctype);
    }
  }

  // Get default icon theme
  GtkIconTheme* iconTheme = gtk_icon_theme_get_default();
  GtkIconInfo* iconInfo = nullptr;
  // Get icon size
  int32_t iconSize = GetIconSize(aIconURI);

  if (icon) {
    // Use icon and theme to get GtkIconInfo
    iconInfo = gtk_icon_theme_lookup_by_gicon(iconTheme, icon, iconSize,
                                              (GtkIconLookupFlags)0);
    g_object_unref(icon);
  }

  if (!iconInfo) {
    // Mozilla's mimetype lookup failed. Try the "unknown" icon.
    iconInfo = gtk_icon_theme_lookup_icon(iconTheme, "unknown", iconSize,
                                          (GtkIconLookupFlags)0);
    if (!iconInfo) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  // Create a GdkPixbuf buffer containing icon and scale it
  GdkPixbuf* buf = gtk_icon_info_load_icon(iconInfo, nullptr);
  gtk_icon_info_free(iconInfo);
  if (!buf) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = ScaleIconBuf(&buf, iconSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = MozGdkPixbufToByteBuf(buf, aDataOut);
  g_object_unref(buf);
  return rv;
}

/* static */
nsresult nsIconChannel::GetIcon(nsIURI* aURI, ByteBuf* aDataOut) {
  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  if (gfxPlatform::IsHeadless()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString stockIcon;
  iconURI->GetStockIcon(stockIcon);
  if (stockIcon.IsEmpty()) {
    return GetIconWithGIO(iconURI, aDataOut);
  }

  // Search for stockIcon
  nsAutoCString iconSizeString;
  iconURI->GetIconSize(iconSizeString);

  nsAutoCString iconStateString;
  iconURI->GetIconState(iconStateString);

  GtkIconSize icon_size = moz_gtk_icon_size(iconSizeString.get());
  GtkStateType state = iconStateString.EqualsLiteral("disabled")
                           ? GTK_STATE_INSENSITIVE
                           : GTK_STATE_NORMAL;

  // First lookup the icon by stock id and text direction.
  GtkTextDirection direction = GTK_TEXT_DIR_NONE;
  if (StringEndsWith(stockIcon, "-ltr"_ns)) {
    direction = GTK_TEXT_DIR_LTR;
  } else if (StringEndsWith(stockIcon, "-rtl"_ns)) {
    direction = GTK_TEXT_DIR_RTL;
  }

  bool forceDirection = direction != GTK_TEXT_DIR_NONE;
  nsAutoCString stockID;
  bool useIconName = false;
  if (!forceDirection) {
    direction = gtk_widget_get_default_direction();
    stockID = stockIcon;
  } else {
    // GTK versions < 2.22 use icon names from concatenating stock id with
    // -(rtl|ltr), which is how the moz-icon stock name is interpreted here.
    stockID = Substring(stockIcon, 0, stockIcon.Length() - 4);
    // However, if we lookup bidi icons by the stock name, then GTK versions
    // >= 2.22 will use a bidi lookup convention that most icon themes do not
    // yet follow.  Therefore, we first check to see if the theme supports the
    // old icon name as this will have bidi support (if found).
    GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
    // Micking what gtk_icon_set_render_icon does with sizes, though it's not
    // critical as icons will be scaled to suit size.  It just means we follow
    // the same paths and so share caches.
    gint width, height;
    if (gtk_icon_size_lookup(icon_size, &width, &height)) {
      gint size = std::min(width, height);
      // We use gtk_icon_theme_lookup_icon() without
      // GTK_ICON_LOOKUP_USE_BUILTIN instead of gtk_icon_theme_has_icon() so
      // we don't pick up fallback icons added by distributions for backward
      // compatibility.
      GtkIconInfo* icon = gtk_icon_theme_lookup_icon(
          icon_theme, stockIcon.get(), size, (GtkIconLookupFlags)0);
      if (icon) {
        useIconName = true;
        gtk_icon_info_free(icon);
      }
    }
  }

  ensure_stock_image_widget();
  GtkStyle* style = gtk_widget_get_style(gStockImageWidget);
  GtkIconSet* icon_set = nullptr;
  if (!useIconName) {
    icon_set = gtk_style_lookup_icon_set(style, stockID.get());
  }

  if (!icon_set) {
    // Either we have chosen icon-name lookup for a bidi icon, or stockIcon is
    // not a stock id so we assume it is an icon name.
    useIconName = true;
    // Creating a GtkIconSet is a convenient way to allow the style to
    // render the icon, possibly with variations suitable for insensitive
    // states.
    icon_set = gtk_icon_set_new();
    GtkIconSource* icon_source = gtk_icon_source_new();

    gtk_icon_source_set_icon_name(icon_source, stockIcon.get());
    gtk_icon_set_add_source(icon_set, icon_source);
    gtk_icon_source_free(icon_source);
  }

  GdkPixbuf* icon = gtk_icon_set_render_icon(
      icon_set, style, direction, state, icon_size, gStockImageWidget, nullptr);
  if (useIconName) {
    gtk_icon_set_unref(icon_set);
  }

  // According to documentation, gtk_icon_set_render_icon() never returns
  // nullptr, but it does return nullptr when we have the problem reported
  // here: https://bugzilla.gnome.org/show_bug.cgi?id=629878#c13
  if (!icon) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = MozGdkPixbufToByteBuf(icon, aDataOut);

  g_object_unref(icon);

  return rv;
}

nsresult nsIconChannel::Init(nsIURI* aURI) {
  nsresult rv;
  nsCOMPtr<nsIInputStream> stream;

  using ContentChild = mozilla::dom::ContentChild;
  if (auto* contentChild = ContentChild::GetSingleton()) {
    // Get the icon via IPC and translate the promise of a ByteBuf
    // into an actually-existing channel.
    RefPtr<ContentChild::GetSystemIconPromise> icon =
        contentChild->SendGetSystemIcon(aURI);
    if (!icon) {
      return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIAsyncInputStream> inputStream;
    nsCOMPtr<nsIAsyncOutputStream> outputStream;
    nsresult rv =
        NS_NewPipe2(getter_AddRefs(inputStream), getter_AddRefs(outputStream),
                    true, false, 0, UINT32_MAX);
    NS_ENSURE_SUCCESS(rv, rv);

    // FIXME: Bug 1718324
    // The GetSystemIcon() call will end up on the parent doing GetIcon()
    // and by using ByteBuf we might not be immune to some deadlock, at least
    // on paper. From analysis in
    // https://phabricator.services.mozilla.com/D118596#3865440 we should be
    // safe in practice, but it would be nicer to just write that differently.

    icon->Then(
        mozilla::GetCurrentSerialEventTarget(), __func__,
        [outputStream](
            mozilla::Tuple<nsresult, mozilla::Maybe<ByteBuf>>&& aArg) {
          nsresult rv = mozilla::Get<0>(aArg);
          mozilla::Maybe<ByteBuf> bytes = std::move(mozilla::Get<1>(aArg));

          if (NS_SUCCEEDED(rv)) {
            MOZ_RELEASE_ASSERT(bytes);
            uint32_t written;
            rv = outputStream->Write(reinterpret_cast<char*>(bytes->mData),
                                     static_cast<uint32_t>(bytes->mLen),
                                     &written);
            if (NS_SUCCEEDED(rv)) {
              const bool wroteAll = static_cast<size_t>(written) == bytes->mLen;
              MOZ_ASSERT(wroteAll);
              if (!wroteAll) {
                rv = NS_ERROR_UNEXPECTED;
              }
            }
          } else {
            MOZ_ASSERT(!bytes);
          }

          if (NS_FAILED(rv)) {
            outputStream->CloseWithStatus(rv);
          }
        },
        [outputStream](mozilla::ipc::ResponseRejectReason) {
          outputStream->CloseWithStatus(NS_ERROR_FAILURE);
        });

    stream = inputStream.forget();
  } else {
    // Get the icon directly.
    ByteBuf bytebuf;
    rv = GetIcon(aURI, &bytebuf);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ByteBufToStream(std::move(bytebuf), getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return StreamToChannel(stream.forget(), aURI, getter_AddRefs(mRealChannel));
}

void nsIconChannel::Shutdown() {
  if (gProtoWindow) {
    gtk_widget_destroy(gProtoWindow);
    gProtoWindow = nullptr;
    gStockImageWidget = nullptr;
  }
}
