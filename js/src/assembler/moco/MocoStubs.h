/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _include_assembler_moco_stubs_h_
#define _include_assembler_moco_stubs_h_

namespace JSC {

class JITCode {
public:
  JITCode(void* start, size_t size)
    : m_start(start), m_size(size)
  { }
  JITCode() { }
  void*  start() const { return m_start; }
  size_t size() const { return m_size; }
private:
  void*  m_start;
  size_t m_size;
};

class CodeBlock {
public:
  CodeBlock(JITCode& jc)
    : m_jitcode(jc)
  {
  }
  JITCode& getJITCode() { return m_jitcode; }
private:
  JITCode& m_jitcode;
};

} // namespace JSC

#endif /* _include_assembler_moco_stubs_h_ */

