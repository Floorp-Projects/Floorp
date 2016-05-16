/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CHROME_BASE_BUFFER_H_
#define CHROME_BASE_BUFFER_H_

// Buffer is a simple std::string-like class for buffering up IPC messages. Its
// main distinguishing characteristic is the trade_bytes function.
class Buffer {
public:
  Buffer();
  ~Buffer();

  bool empty() const;
  const char* data() const;
  size_t size() const;

  void clear();
  void append(const char* bytes, size_t length);
  void assign(const char* bytes, size_t length);
  void erase(size_t start, size_t count);

  void reserve(size_t size);

  // This function should be used by a caller who wants to extract the first
  // |count| bytes from the buffer. Rather than copying the bytes out, this
  // function returns the entire buffer. The bytes in range [count, size()) are
  // copied out to a new buffer which becomes the current buffer. The
  // presumption is that |count| is very large and approximately equal to size()
  // so not much needs to be copied.
  char* trade_bytes(size_t count);

private:
  void try_realloc(size_t newlength);

  char* mBuffer;
  size_t mSize;
  size_t mReserved;
};

#endif // CHROME_BASE_BUFFER_H_
