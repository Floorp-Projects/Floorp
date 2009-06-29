// Copyright 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_
#define CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_

#include <vector>
#include <string>

#include "base/file_path.h"
#include "base/string_piece.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

class Pickle;

// Represents a user script, either a standalone one, or one that is part of an
// extension.
class UserScript {
 public:
  // Locations that user scripts can be run inside the document.
  enum RunLocation {
    DOCUMENT_START,  // After the documentElemnet is created, but before
                     // anything else happens.
    DOCUMENT_END,  // After the entire document is parsed. Same as
                   // DOMContentLoaded.

    RUN_LOCATION_LAST  // Leave this as the last item.
  };

  // Holds actual script file info.
  class File {
   public:
    File(const FilePath& path, const GURL& url):
         path_(path),
         url_(url) {
    }
    File() {}

    const FilePath& path() const { return path_; }
    void set_path(const FilePath& path) { path_ = path; }

    const GURL& url() const { return url_; }
    void set_url(const GURL& url) { url_ = url; }

    // If external_content_ is set returns it as content otherwise it returns
    // content_
    const StringPiece GetContent() const {
      if (external_content_.data())
        return external_content_;
      else
        return content_;
    }
    void set_external_content(const StringPiece& content) {
      external_content_ = content;
    }
    const void set_content(const StringPiece& content) {
      content_.assign(content.begin(), content.end());
    }

    // Serialization support. The content and path_ member will not be
    // serialized!
    void Pickle(::Pickle* pickle) const;
    void Unpickle(const ::Pickle& pickle, void** iter);

   private:
    // Where is the script file lives on the disk.
    FilePath path_;

    // The url to this scipt file.
    GURL url_;

    // The script content. It can be set to either loaded_content_ or
    // externally allocated string.
    StringPiece external_content_;

    // Set when the content is loaded by LoadContent
    std::string content_;
  };

  typedef std::vector<File> FileList;

  // Constructor. Default the run location to document end, which is like
  // Greasemonkey and probably more useful for typical scripts.
  UserScript() : run_location_(DOCUMENT_END) {}

  // The place in the document to run the script.
  RunLocation run_location() const { return run_location_; }
  void set_run_location(RunLocation location) { run_location_ = location; }

  // The globs, if any, that determine which pages this script runs against.
  // These are only used with "standalone" Greasemonkey-like user scripts.
  const std::vector<std::string>& globs() const { return globs_; }
  void add_glob(const std::string& glob) { globs_.push_back(glob); }
  void clear_globs() { globs_.clear(); }

  // The URLPatterns, if any, that determine which pages this script runs
  // against.
  const std::vector<URLPattern>& url_patterns() const { return url_patterns_; }
  void add_url_pattern(const URLPattern& pattern) {
    url_patterns_.push_back(pattern);
  }
  void clear_url_patterns() { url_patterns_.clear(); }

  // List of js scripts for this user script
  FileList& js_scripts() { return js_scripts_; }
  const FileList& js_scripts() const { return js_scripts_; }

  // List of css scripts for this user script
  FileList& css_scripts() { return css_scripts_; }
  const FileList& css_scripts() const { return css_scripts_; }

  const std::string& extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& id) { extension_id_ = id; }

  bool is_standalone() { return extension_id_.empty(); }

  // Returns true if the script should be applied to the specified URL, false
  // otherwise.
  bool MatchesUrl(const GURL& url);

  // Serialize the UserScript into a pickle. The content of the scripts and
  // paths to UserScript::Files will not be serialized!
  void Pickle(::Pickle* pickle) const;

  // Deserialize the script from a pickle. Note that this always succeeds
  // because presumably we were the one that pickled it, and we did it
  // correctly.
  void Unpickle(const ::Pickle& pickle, void** iter);

 private:
  // The location to run the script inside the document.
  RunLocation run_location_;

  // Greasemonkey-style globs that determine pages to inject the script into.
  // These are only used with standalone scripts.
  std::vector<std::string> globs_;

  // URLPatterns that determine pages to inject the script into. These are
  // only used with scripts that are part of extensions.
  std::vector<URLPattern> url_patterns_;

  // List of js scripts defined in content_scripts
  FileList js_scripts_;

  // List of css scripts defined in content_scripts
  FileList css_scripts_;

  // The ID of the extension this script is a part of, if any. Can be empty if
  // the script is a "standlone" user script.
  std::string extension_id_;
};

typedef std::vector<UserScript> UserScriptList;

#endif  // CHROME_COMMON_EXTENSIONS_USER_SCRIPT_H_
