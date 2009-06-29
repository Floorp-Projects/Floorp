// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CLIPBOARD_H_
#define BASE_CLIPBOARD_H_

#include <map>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/process.h"
#include "base/string16.h"
#include "base/gfx/size.h"

class Clipboard {
 public:
  typedef std::string FormatType;
#if defined(OS_LINUX)
  typedef struct _GtkClipboard GtkClipboard;
  typedef std::map<FormatType, std::pair<char*, size_t> > TargetMap;
#endif

  // ObjectType designates the type of data to be stored in the clipboard. This
  // designation is shared across all OSes. The system-specific designation
  // is defined by FormatType. A single ObjectType might be represented by
  // several system-specific FormatTypes. For example, on Linux the CBF_TEXT
  // ObjectType maps to "text/plain", "STRING", and several other formats. On
  // windows it maps to CF_UNICODETEXT.
  enum ObjectType {
    CBF_TEXT,
    CBF_HTML,
    CBF_BOOKMARK,
    CBF_LINK,
    CBF_FILES,
    CBF_WEBKIT,
    CBF_BITMAP,
    CBF_SMBITMAP // bitmap from shared memory
  };

  // ObjectMap is a map from ObjectType to associated data.
  // The data is organized differently for each ObjectType. The following
  // table summarizes what kind of data is stored for each key.
  // * indicates an optional argument.
  //
  // Key           Arguments    Type
  // -------------------------------------
  // CBF_TEXT      text         char array
  // CBF_HTML      html         char array
  //               url*         char array
  // CBF_BOOKMARK  html         char array
  //               url          char array
  // CBF_LINK      html         char array
  //               url          char array
  // CBF_FILES     files        char array representing multiple files.
  //                            Filenames are separated by null characters and
  //                            the final filename is double null terminated.
  //                            The filenames are encoded in platform-specific
  //                            encoding.
  // CBF_WEBKIT    none         empty vector
  // CBF_BITMAP    pixels       byte array
  //               size         gfx::Size struct
  // CBF_SMBITMAP  shared_mem   shared memory handle
  //               size         gfx::Size struct
  typedef std::vector<char> ObjectMapParam;
  typedef std::vector<ObjectMapParam> ObjectMapParams;
  typedef std::map<int /* ObjectType */, ObjectMapParams> ObjectMap;

  Clipboard();
  ~Clipboard();

  // Write a bunch of objects to the system clipboard. Copies are made of the
  // contents of |objects|. On Windows they are copied to the system clipboard.
  // On linux they are copied into a structure owned by the Clipboard object and
  // kept until the system clipboard is set again.
  void WriteObjects(const ObjectMap& objects);

  // Behaves as above. If there is some shared memory handle passed as one of
  // the objects, it came from the process designated by |process|. This will
  // assist in turning it into a shared memory region that the current process
  // can use.
  void WriteObjects(const ObjectMap& objects, base::ProcessHandle process);

  // Tests whether the clipboard contains a certain format
  bool IsFormatAvailable(const FormatType& format) const;

  // Reads UNICODE text from the clipboard, if available.
  void ReadText(string16* result) const;

  // Reads ASCII text from the clipboard, if available.
  void ReadAsciiText(std::string* result) const;

  // Reads HTML from the clipboard, if available.
  void ReadHTML(string16* markup, std::string* src_url) const;

  // Reads a bookmark from the clipboard, if available.
  void ReadBookmark(string16* title, std::string* url) const;

  // Reads a file or group of files from the clipboard, if available, into the
  // out parameter.
  void ReadFile(FilePath* file) const;
  void ReadFiles(std::vector<FilePath>* files) const;

  // Get format Identifiers for various types.
  static FormatType GetUrlFormatType();
  static FormatType GetUrlWFormatType();
  static FormatType GetMozUrlFormatType();
  static FormatType GetPlainTextFormatType();
  static FormatType GetPlainTextWFormatType();
  static FormatType GetFilenameFormatType();
  static FormatType GetFilenameWFormatType();
  static FormatType GetWebKitSmartPasteFormatType();
  // Win: MS HTML Format, Other: Generic HTML format
  static FormatType GetHtmlFormatType();
#if defined(OS_WIN)
  static FormatType GetBitmapFormatType();
  // Firefox text/html
  static FormatType GetTextHtmlFormatType();
  static FormatType GetCFHDropFormatType();
  static FormatType GetFileDescriptorFormatType();
  static FormatType GetFileContentFormatZeroType();

  // Duplicates any remote shared memory handle embedded inside |objects| that
  // was created by |process| so that it can be used by this process.
  static void DuplicateRemoteHandles(base::ProcessHandle process,
                                     ObjectMap* objects);
#endif

 private:
  void WriteText(const char* text_data, size_t text_len);

  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len);

  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len);

  void WriteHyperlink(const char* title_data,
                      size_t title_len,
                      const char* url_data,
                      size_t url_len);

  void WriteWebSmartPaste();

  void WriteFiles(const char* file_data, size_t file_len);

  void DispatchObject(ObjectType type, const ObjectMapParams& params);

  void WriteBitmap(const char* pixel_data, const char* size_data);
#if defined(OS_WIN)
  void WriteBitmapFromSharedMemory(const char* bitmap_data,
                                   const char* size_data,
                                   base::ProcessHandle handle);

  void WriteBitmapFromHandle(HBITMAP source_hbitmap,
                             const gfx::Size& size);

  // Safely write to system clipboard. Free |handle| on failure.
  void WriteToClipboard(unsigned int format, HANDLE handle);

  static void ParseBookmarkClipboardFormat(const string16& bookmark,
                                           string16* title,
                                           std::string* url);

  // Free a handle depending on its type (as intuited from format)
  static void FreeData(unsigned int format, HANDLE data);

  // Return the window that should be the clipboard owner, creating it
  // if neccessary.  Marked const for lazily initialization by const methods.
  HWND GetClipboardWindow() const;

  // Mark this as mutable so const methods can still do lazy initialization.
  mutable HWND clipboard_owner_;

  // True if we can create a window.
  bool create_window_;
#elif defined(OS_LINUX)
  // Data is stored in the |clipboard_data_| map until it is saved to the system
  // clipboard. The Store* functions save data to the |clipboard_data_| map. The
  // SetGtkClipboard function replaces whatever is on the system clipboard with
  // the contents of |clipboard_data_|.
  // The Write* functions make a deep copy of the data passed to them an store
  // it in |clipboard_data_|.

  // Write changes to gtk clipboard.
  void SetGtkClipboard();
  // Free pointers in clipboard_data_ and clear() the map.
  void FreeTargetMap();
  // Insert a mapping into clipboard_data_.
  void InsertMapping(const char* key, char* data, size_t data_len);

  TargetMap* clipboard_data_;
  GtkClipboard* clipboard_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(Clipboard);
};

#endif  // BASE_CLIPBOARD_H_
