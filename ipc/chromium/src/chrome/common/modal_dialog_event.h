// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MODAL_DIALOG_EVENT_H_
#define CHROME_COMMON_MODAL_DIALOG_EVENT_H_

// This structure is passed around where we need a modal dialog event, which
// is currently not plumbed on Mac & Linux.
//
// TODO(port) Fix this. This structure should probably go away and we should
// do the modal dialog event in some portable way. If you remove this, be
// sure to also remove the ParamTraits for it in resource_messages.h
struct ModalDialogEvent {
#if defined(OS_WIN)
  HANDLE event;
#endif
};

#endif  // CHROME_COMMON_MODAL_DIALOG_EVENT_H_
