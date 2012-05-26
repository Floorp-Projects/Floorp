/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_hal_WindowIdentifier_h
#define mozilla_hal_WindowIdentifier_h

#include "mozilla/Types.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"

namespace mozilla {
namespace hal {

/**
 * This class serves two purposes.
 *
 * First, this class wraps a pointer to a window.
 *
 * Second, WindowIdentifier lets us uniquely identify a window across
 * processes.  A window exposes an ID which is unique only within its
 * process.  Thus to identify a window, we need to know the ID of the
 * process which contains it.  But the scope of a process's ID is its
 * parent; that is, two processes with different parents might have
 * the same ID.
 *
 * So to identify a window, we need its ID plus the IDs of all the
 * processes in the path from the window's process to the root
 * process.  We throw in the IDs of the intermediate windows (a
 * content window is contained in a window at each level of the
 * process tree) for good measures.
 *
 * You can access this list of IDs by calling AsArray().
 */
class WindowIdentifier
{
public:
  /**
   * Create an empty WindowIdentifier.  Calls to any of this object's
   * public methods will assert -- an empty WindowIdentifier may be
   * used only as a placeholder to code which promises not to touch
   * the object.
   */
  WindowIdentifier();

  /**
   * Copy constructor.
   */
  WindowIdentifier(const WindowIdentifier& other);

  /**
   * Wrap the given window in a WindowIdentifier.  These two
   * constructors automatically grab the window's ID and append it to
   * the array of IDs.
   *
   * Note that these constructors allow an implicit conversion to a
   * WindowIdentifier.
   */
  explicit WindowIdentifier(nsIDOMWindow* window);

  /**
   * Create a new WindowIdentifier with the given id array and window.
   * This automatically grabs the window's ID and appends it to the
   * array.
   */
  WindowIdentifier(const nsTArray<uint64_t>& id, nsIDOMWindow* window);

  /**
   * Get the list of window and process IDs we contain.
   */
  typedef InfallibleTArray<uint64_t> IDArrayType;
  const IDArrayType& AsArray() const;

  /**
   * Append the ID of the ContentChild singleton to our array of
   * window/process IDs.
   */
  void AppendProcessID();

  /**
   * Does this WindowIdentifier identify both a window and the process
   * containing that window?  If so, we say it has traveled through
   * IPC.
   */
  bool HasTraveledThroughIPC() const;

  /**
   * Get the window this object wraps.
   */
  nsIDOMWindow* GetWindow() const;

private:
  /**
   * Get the ID of the window object we wrap.
   */
  uint64_t GetWindowID() const;

  AutoInfallibleTArray<uint64_t, 3> mID;
  nsCOMPtr<nsIDOMWindow> mWindow;
  bool mIsEmpty;
};

} // namespace hal
} // namespace mozilla

#endif // mozilla_hal_WindowIdentifier_h
