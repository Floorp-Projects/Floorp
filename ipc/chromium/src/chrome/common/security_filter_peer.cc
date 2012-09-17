// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/security_filter_peer.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/png_encoder.h"
#include "base/gfx/size.h"
#include "base/string_util.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "skia/include/SkBitmap.h"
#include "skia/include/SkCanvas.h"
#include "webkit/glue/webkit_glue.h"
#include "SkDevice.h"


SecurityFilterPeer::SecurityFilterPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer)
    : original_peer_(peer),
      resource_loader_bridge_(resource_loader_bridge) {
}

SecurityFilterPeer::~SecurityFilterPeer() {
}

// static
SecurityFilterPeer* SecurityFilterPeer::CreateSecurityFilterPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    ResourceType::Type resource_type,
    const std::string& mime_type,
    FilterPolicy::Type filter_policy,
    int os_error) {
  if (filter_policy == FilterPolicy::DONT_FILTER) {
    NOTREACHED();
    return NULL;
  }

  if (StartsWithASCII(mime_type, "image/", false)) {
    // What we do with images depends on details of the |filter_policy|.
    if (filter_policy == FilterPolicy::FILTER_ALL_EXCEPT_IMAGES)
      return new ImageFilterPeer(resource_loader_bridge, peer);
    // Otherwise, fall through to blocking images hard.
  }

  // Regardless of what a frame contents replace it with our error message,
  // so it is visible it's been filtered-out.
  if (ResourceType::IsFrame(resource_type))
    return CreateSecurityFilterPeerForFrame(peer, os_error);

  // Any other content is entirely filtered-out.
  return new ReplaceContentPeer(resource_loader_bridge, peer,
                                std::string(), std::string());
}

// static
SecurityFilterPeer*
    SecurityFilterPeer::CreateSecurityFilterPeerForDeniedRequest(
    ResourceType::Type resource_type,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    int os_error) {
  // Create a filter for SSL and CERT errors.
  switch (os_error) {
    case net::ERR_SSL_PROTOCOL_ERROR:
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_INSECURE_RESPONSE:
      if (ResourceType::IsFrame(resource_type))
        return CreateSecurityFilterPeerForFrame(peer, os_error);
      // Any other content is entirely filtered-out.
      return new ReplaceContentPeer(NULL, peer, std::string(), std::string());
    default:
      // For other errors, we use our normal error handling.
      return NULL;
  }
}

// static
SecurityFilterPeer* SecurityFilterPeer::CreateSecurityFilterPeerForFrame(
    webkit_glue::ResourceLoaderBridge::Peer* peer, int os_error) {
  // TODO(jcampan): use a different message when getting a phishing/malware
  // error.
  std::wstring error_msg = l10n_util::GetString(IDS_UNSAFE_FRAME_MESSAGE);
  std::string html = StringPrintf(
      "<html><body style='background-color:#990000;color:white;'>"
      "%s</body></html>",
      WideToUTF8(error_msg).c_str());
  return new ReplaceContentPeer(NULL, peer, "text/html", html);
}

void SecurityFilterPeer::OnUploadProgress(uint64_t position, uint64_t size) {
  original_peer_->OnUploadProgress(position, size);
}

void SecurityFilterPeer::OnReceivedRedirect(const GURL& new_url) {
  NOTREACHED();
}

void SecurityFilterPeer::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  NOTREACHED();
}

void SecurityFilterPeer::OnReceivedData(const char* data, int len) {
  NOTREACHED();
}

void SecurityFilterPeer::OnCompletedRequest(const URLRequestStatus& status,
                                            const std::string& security_info) {
  NOTREACHED();
}

std::string SecurityFilterPeer::GetURLForDebugging() {
  return original_peer_->GetURLForDebugging();
}

// static
void ProcessResponseInfo(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info_in,
    webkit_glue::ResourceLoaderBridge::ResponseInfo* info_out,
    const std::string& mime_type) {
  DCHECK(info_out);
  *info_out = info_in;
  info_out->mime_type = mime_type;
  // Let's create our own HTTP headers.
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  raw_headers.push_back('\0');
  // Don't cache the data we are serving, it is not the real data for that URL
  // (if the filtered resource were to make it into the WebCore cache, then the
  // same URL loaded in a safe scenario would still return the filtered
  // resource).
  raw_headers.append("cache-control: no-cache");
  raw_headers.push_back('\0');
  if (!mime_type.empty()) {
    raw_headers.append("content-type: ");
    raw_headers.append(mime_type);
    raw_headers.push_back('\0');
  }
  raw_headers.push_back('\0');
  net::HttpResponseHeaders* new_headers =
      new net::HttpResponseHeaders(raw_headers);
  info_out->headers = new_headers;
}

////////////////////////////////////////////////////////////////////////////////
// BufferedPeer

BufferedPeer::BufferedPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    const std::string& mime_type)
    : SecurityFilterPeer(resource_loader_bridge, peer),
      mime_type_(mime_type) {
}

BufferedPeer::~BufferedPeer() {
}

void BufferedPeer::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool response_filtered) {
  ProcessResponseInfo(info, &response_info_, mime_type_);
}

void BufferedPeer::OnReceivedData(const char* data, int len) {
  data_.append(data, len);
}

void BufferedPeer::OnCompletedRequest(const URLRequestStatus& status,
                                      const std::string& security_info) {
  // Make sure we delete ourselves at the end of this call.
  scoped_ptr<BufferedPeer> this_deleter(this);

  // Give sub-classes a chance at altering the data.
  if (status.status() != URLRequestStatus::SUCCESS || !DataReady()) {
    // Pretend we failed to load the resource.
    original_peer_->OnReceivedResponse(response_info_, true);
    URLRequestStatus status(URLRequestStatus::CANCELED, 0);
    original_peer_->OnCompletedRequest(status, security_info);
    return;
  }

  original_peer_->OnReceivedResponse(response_info_, true);
  if (!data_.empty())
    original_peer_->OnReceivedData(data_.data(),
                                   static_cast<int>(data_.size()));
  original_peer_->OnCompletedRequest(status, security_info);
}

////////////////////////////////////////////////////////////////////////////////
// ReplaceContentPeer

ReplaceContentPeer::ReplaceContentPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    const std::string& mime_type,
    const std::string& data)
    : SecurityFilterPeer(resource_loader_bridge, peer),
      mime_type_(mime_type),
      data_(data) {
}

ReplaceContentPeer::~ReplaceContentPeer() {
}

void ReplaceContentPeer::OnReceivedResponse(
    const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
    bool content_filtered) {
  // Ignore this, we'll serve some alternate content in OnCompletedRequest.
}

void ReplaceContentPeer::OnReceivedData(const char* data, int len) {
  // Ignore this, we'll serve some alternate content in OnCompletedRequest.
}

void ReplaceContentPeer::OnCompletedRequest(const URLRequestStatus& status,
                                            const std::string& security_info) {
  webkit_glue::ResourceLoaderBridge::ResponseInfo info;
  ProcessResponseInfo(info, &info, mime_type_);
  info.security_info = security_info;
  info.content_length = static_cast<int>(data_.size());
  original_peer_->OnReceivedResponse(info, true);
  if (!data_.empty())
    original_peer_->OnReceivedData(data_.data(),
                                   static_cast<int>(data_.size()));
  original_peer_->OnCompletedRequest(URLRequestStatus(), security_info);

  // The request processing is complete, we must delete ourselves.
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// ImageFilterPeer

ImageFilterPeer::ImageFilterPeer(
    webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
    webkit_glue::ResourceLoaderBridge::Peer* peer)
    : BufferedPeer(resource_loader_bridge, peer, "image/png") {
}

ImageFilterPeer::~ImageFilterPeer() {
}

bool ImageFilterPeer::DataReady() {
  static SkBitmap* stamp_bitmap = NULL;
  if (!stamp_bitmap) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    stamp_bitmap = rb.GetBitmapNamed(IDR_INSECURE_CONTENT_STAMP);
    if (!stamp_bitmap) {
      NOTREACHED();
      return false;
    }
  }

  // Let's decode the image we have been given.
  SkBitmap image;
  if (!webkit_glue::DecodeImage(data_, &image))
    return false;  // We could not decode that image.

  // Let's create a new canvas to modify the image.
  SkBitmap canvas_bitmap;
  canvas_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                          image.width(), image.height());
  canvas_bitmap.allocPixels();
  SkCanvas canvas(canvas_bitmap);

  // Let's paint the actual image with an alpha.
  SkRect rect;
  rect.fLeft = 0;
  rect.fTop = 0;
  rect.fRight = SkIntToScalar(image.width());
  rect.fBottom = SkIntToScalar(image.height());

  SkPaint paint;
  paint.setAlpha(80);
  canvas.drawBitmapRect(image, NULL, rect, &paint);

  // Then stamp-it.
  paint.setAlpha(150);
  int visible_row_count = image.height() / stamp_bitmap->height();
  if (image.height() % stamp_bitmap->height() != 0)
    visible_row_count++;

  for (int row = 0; row < visible_row_count; row++) {
    for (int x = (row % 2 == 0) ? 0 : stamp_bitmap->width(); x < image.width();
         x += stamp_bitmap->width() * 2) {
      canvas.drawBitmap(*stamp_bitmap,
                        SkIntToScalar(x),
                        SkIntToScalar(row * stamp_bitmap->height()),
                        &paint);
    }
  }

  // Now encode it to a PNG.
  std::vector<unsigned char> output;
  if (!PNGEncoder::EncodeBGRASkBitmap(canvas.getDevice()->accessBitmap(false),
                                      false, &output)) {
    return false;
  }

  // Copy the vector content to data_ which is a string.
  data_.clear();
  data_.resize(output.size());
  std::copy(output.begin(), output.end(), data_.begin());

  return true;
}
