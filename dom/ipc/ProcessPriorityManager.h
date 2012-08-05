/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ProcessPriorityManager_h_
#define mozilla_ProcessPriorityManager_h_

namespace mozilla {
namespace dom {
namespace ipc {

/**
 * Initialize the ProcessPriorityManager.
 *
 * The ProcessPriorityManager informs the hal back-end whether this is the root
 * Gecko process, and, if we're not the root, informs hal when this process
 * transitions between having no visible top-level windows, and having at least
 * one visible top-level window.
 *
 * Hal may adjust this process's operating system priority (e.g. niceness, on
 * *nix) according to these notificaitons.
 *
 * This function call does nothing if the pref for OOP tabs is not set.
 */
void InitProcessPriorityManager();

} // namespace ipc
} // namespace dom
} // namespace mozilla

#endif
