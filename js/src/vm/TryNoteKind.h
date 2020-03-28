/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_TryNoteKind_h
#define vm_TryNoteKind_h

namespace js {

/*
 * [SMDOC] Try Notes
 *
 * Trynotes are attached to regions that are involved with
 * exception unwinding. They can be broken up into four categories:
 *
 * 1. CATCH and FINALLY: Basic exception handling. A CATCH trynote
 *    covers the range of the associated try. A FINALLY trynote covers
 *    the try and the catch.

 * 2. FOR_IN and DESTRUCTURING: These operations create an iterator
 *    which must be cleaned up (by calling IteratorClose) during
 *    exception unwinding.
 *
 * 3. FOR_OF and FOR_OF_ITERCLOSE: For-of loops handle unwinding using
 *    catch blocks. These trynotes are used for for-of breaks/returns,
 *    which create regions that are lexically within a for-of block,
 *    but logically outside of it. See TryNoteIter::settle for more
 *    details.
 *
 * 4. LOOP: This represents normal for/while/do-while loops. It is
 *    unnecessary for exception unwinding, but storing the boundaries
 *    of loops here is helpful for heuristics that need to know
 *    whether a given op is inside a loop.
 */
enum TryNoteKind {
  JSTRY_CATCH,
  JSTRY_FINALLY,
  JSTRY_FOR_IN,
  JSTRY_DESTRUCTURING,
  JSTRY_FOR_OF,
  JSTRY_FOR_OF_ITERCLOSE,
  JSTRY_LOOP
};

}  // namespace js

#endif /* vm_TryNoteKind_h */
