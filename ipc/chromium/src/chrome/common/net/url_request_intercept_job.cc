// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This job type handles Chrome plugin network interception.  When a plugin
// wants to intercept a request, a job of this type is created.  The intercept
// job communicates with the plugin to retrieve the response headers and data.

#include "chrome/common/net/url_request_intercept_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/notification_service.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

using base::Time;
using base::TimeDelta;

//
// URLRequestInterceptJob
//

URLRequestInterceptJob::URLRequestInterceptJob(URLRequest* request,
                                               ChromePluginLib* plugin,
                                               ScopableCPRequest* cprequest)
    : URLRequestJob(request),
      cprequest_(cprequest),
      plugin_(plugin),
      got_headers_(false),
      read_buffer_(NULL) {
  cprequest_->data = this;  // see FromCPRequest().

  NotificationService::current()->AddObserver(
      this, NotificationType::CHROME_PLUGIN_UNLOADED,
      Source<ChromePluginLib>(plugin_));
}

URLRequestInterceptJob::~URLRequestInterceptJob() {
  if (plugin_) {
    plugin_->functions().request_funcs->end_request(cprequest_.get(),
                                                    CPERR_SUCCESS);
    DetachPlugin();
  }
}

void URLRequestInterceptJob::DetachPlugin() {
  NotificationService::current()->RemoveObserver(
      this, NotificationType::CHROME_PLUGIN_UNLOADED,
      Source<ChromePluginLib>(plugin_));
  plugin_ = NULL;
}

void URLRequestInterceptJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestInterceptJob::StartAsync));
}

void URLRequestInterceptJob::Kill() {
  if (plugin_) {
    plugin_->functions().request_funcs->end_request(cprequest_.get(),
                                                    CPERR_CANCELLED);
    DetachPlugin();
  }
  URLRequestJob::Kill();
}

bool URLRequestInterceptJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                         int* bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);

  if (!plugin_)
    return false;

  int rv = plugin_->functions().request_funcs->read(cprequest_.get(),
                                                    dest->data(), dest_size);
  if (rv >= 0) {
    *bytes_read = rv;
    return true;
  }

  if (rv == CPERR_IO_PENDING) {
    read_buffer_ = dest;
    read_buffer_size_ = dest_size;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    // TODO(mpcomplete): better error code
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, net::ERR_FAILED));
  }

  return false;
}

bool URLRequestInterceptJob::GetMimeType(std::string* mime_type) const {
  return request_->response_headers()->GetMimeType(mime_type);
}

bool URLRequestInterceptJob::GetCharset(std::string* charset) {
  return request_->response_headers()->GetCharset(charset);
}

bool URLRequestInterceptJob::GetContentEncoding(std::string* encoding_type) {
  // TODO(darin): what if there are multiple content encodings?
  return request_->response_headers()->EnumerateHeader(NULL, "Content-Encoding",
                                                       encoding_type);
}

void URLRequestInterceptJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!plugin_)
    return;

  std::string raw_headers;
  int size = plugin_->functions().request_funcs->get_response_info(
      cprequest_.get(), CPRESPONSEINFO_HTTP_RAW_HEADERS, NULL, 0);
  int rv = size < 0 ? size :
      plugin_->functions().request_funcs->get_response_info(
          cprequest_.get(), CPRESPONSEINFO_HTTP_RAW_HEADERS,
          WriteInto(&raw_headers, size+1), size);
  if (rv != CPERR_SUCCESS) {
    // TODO(mpcomplete): what should we do here?
    raw_headers = "HTTP/1.1 404 Not Found" + '\0';
  }

  info->headers = new net::HttpResponseHeaders(raw_headers);

  if (request_->url().SchemeIsSecure()) {
    // Make up a fake certificate for intercepted data since we don't have
    // access to the real SSL info.
    // TODO(mpcomplete): we should probably have the interception API transmit
    // the SSL info, but the only consumer of this API (Gears) doesn't keep that
    // around. We should change that.
    const char* kCertIssuer = "Chrome Internal";
    const int kLifetimeDays = 100;

    DLOG(WARNING) << "Issuing a fake SSL certificate for interception of URL "
        << request_->url();

    info->ssl_info.cert =
        new net::X509Certificate(request_->url().GetWithEmptyPath().spec(),
                                 kCertIssuer,
                                 Time::Now(),
                                 Time::Now() +
                                     TimeDelta::FromDays(kLifetimeDays));
    info->ssl_info.cert_status = 0;
    info->ssl_info.security_bits = 0;
  }
}

int URLRequestInterceptJob::GetResponseCode() const {
  if (!plugin_)
    return -1;

  int status = 200;
  plugin_->functions().request_funcs->get_response_info(
      cprequest_.get(), CPRESPONSEINFO_HTTP_STATUS, &status, sizeof(status));

  return status;
}

bool URLRequestInterceptJob::IsRedirectResponse(GURL* location,
                                                int* http_status_code) {
  if (!request_->response_headers())
    return false;

  std::string value;
  if (!request_->response_headers()->IsRedirect(&value))
    return false;

  *location = request_->url().Resolve(value);
  *http_status_code = request_->response_headers()->response_code();
  return true;
}

void URLRequestInterceptJob::StartAsync() {
  // We may have been orphaned...
  if (!request_ || !plugin_)
    return;

  int rv = plugin_->functions().request_funcs->start_request(cprequest_.get());
  if (rv != CPERR_IO_PENDING)
    OnStartCompleted(rv);
}

void URLRequestInterceptJob::OnStartCompleted(int result) {
  if (result != CPERR_SUCCESS) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                net::ERR_CONNECTION_FAILED));
    return;
  }

  NotifyHeadersComplete();
}

void URLRequestInterceptJob::OnReadCompleted(int bytes_read) {
  if (bytes_read < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  SetStatus(URLRequestStatus());  // clear the async flag
  NotifyReadComplete(bytes_read);
}

void URLRequestInterceptJob::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(type == NotificationType::CHROME_PLUGIN_UNLOADED);
  DCHECK(plugin_ == Source<ChromePluginLib>(source).ptr());

  DetachPlugin();
}
