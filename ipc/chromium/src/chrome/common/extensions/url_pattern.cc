// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/url_pattern.h"

#include "base/string_piece.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"

// TODO(aa): Consider adding chrome-extension? What about more obscure ones
// like data: and javascript: ?
static const char* kValidSchemes[] = {
  chrome::kHttpScheme,
  chrome::kHttpsScheme,
  chrome::kFileScheme,
  chrome::kFtpScheme,
  chrome::kChromeUIScheme,
};

static const char kPathSeparator[] = "/";

static bool IsValidScheme(const std::string& scheme) {
  for (size_t i = 0; i < arraysize(kValidSchemes); ++i) {
    if (scheme == kValidSchemes[i])
      return true;
  }

  return false;
}

bool URLPattern::Parse(const std::string& pattern) {
  size_t scheme_end_pos = pattern.find(chrome::kStandardSchemeSeparator);
  if (scheme_end_pos == std::string::npos)
    return false;

  scheme_ = pattern.substr(0, scheme_end_pos);
  if (!IsValidScheme(scheme_))
    return false;

  size_t host_start_pos = scheme_end_pos +
      strlen(chrome::kStandardSchemeSeparator);
  if (host_start_pos >= pattern.length())
    return false;

  // Parse out the host and path.
  size_t path_start_pos = 0;

  // File URLs are special because they have no host. There are other schemes
  // with the same structure, but we don't support them (yet).
  if (scheme_ == "file") {
    path_start_pos = host_start_pos;
  } else {
    size_t host_end_pos = pattern.find(kPathSeparator, host_start_pos);
    if (host_end_pos == std::string::npos)
      return false;

    host_ = pattern.substr(host_start_pos, host_end_pos - host_start_pos);

    // The first component can optionally be '*' to match all subdomains.
    std::vector<std::string> host_components;
    SplitString(host_, '.', &host_components);
    if (host_components[0] == "*") {
      match_subdomains_ = true;
      host_components.erase(host_components.begin(),
                            host_components.begin() + 1);
    }
    host_ = JoinString(host_components, '.');

    // No other '*' can occur in the host, though. This isn't necessary, but is
    // done as a convenience to developers who might otherwise be confused and
    // think '*' works as a glob in the host.
    if (host_.find('*') != std::string::npos)
      return false;

    path_start_pos = host_end_pos;
  }

  path_ = pattern.substr(path_start_pos);
  return true;
}

bool URLPattern::MatchesUrl(const GURL &test) {
  if (test.scheme() != scheme_)
    return false;

  if (!MatchesHost(test))
    return false;

  if (!MatchesPath(test))
    return false;

  return true;
}

bool URLPattern::MatchesHost(const GURL& test) {
  if (test.host() == host_)
    return true;

  if (!match_subdomains_ || test.HostIsIPAddress())
    return false;

  // If we're matching subdomains, and we have no host, that means the pattern
  // was <scheme>://*/<whatever>, so we match anything.
  if (host_.empty())
    return true;

  // Check if the test host is a subdomain of our host.
  if (test.host().length() <= (host_.length() + 1))
    return false;

  if (test.host().compare(test.host().length() - host_.length(),
                          host_.length(), host_) != 0)
    return false;

  return test.host()[test.host().length() - host_.length() - 1] == '.';
}

bool URLPattern::MatchesPath(const GURL& test) {
  if (path_escaped_.empty()) {
    path_escaped_ = path_;
    ReplaceSubstringsAfterOffset(&path_escaped_, 0, "\\", "\\\\");
    ReplaceSubstringsAfterOffset(&path_escaped_, 0, "?", "\\?");
  }

  if (!MatchPattern(test.PathForRequest(), path_escaped_))
    return false;

  return true;
}

std::string URLPattern::GetAsString() const {
  std::string spec = scheme_ + chrome::kStandardSchemeSeparator;

  if (match_subdomains_) {
    spec += "*";
    if (!host_.empty())
      spec += ".";
  }

  if (!host_.empty())
    spec += host_;

  if (!path_.empty())
    spec += path_;

  return spec;
}
