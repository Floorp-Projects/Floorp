// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/native_widget_types.h"

#include "base/gfx/gtk_native_view_id_manager.h"
#include "base/logging.h"

namespace gfx {

NativeViewId IdFromNativeView(NativeView view) {
  return Singleton<GtkNativeViewManager>()->GetIdForWidget(view);
}

}  // namespace gfx
